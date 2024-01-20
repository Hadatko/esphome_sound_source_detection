/*
 * Copyright (c) 2024 Dusan Cervenka.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "detect.h"

#include "esphome/core/log.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include "arduinoFFT.h"

#define OCTAVES 9
// A-weighting curve from 31.5 Hz ... 8000 Hz
static const float aweighting[] = {-39.4, -26.2, -16.1, -8.6, -3.2, 0.0, 1.2, 1.0, -1.1};

static const char *const TAG = "detect_audio";

namespace esphome {
namespace detect_audio {

DetectAudio::DetectAudio() : m_mic(nullptr), m_currentPeak(0), m_cnt(0), m_mn(99999), m_mx(0), m_sum(0) {}

DetectAudio::~DetectAudio() { delete m_mic; }

void DetectAudio::setup() {
  ESP_LOGCONFIG(TAG, "Setting up audio detection...");
  if (!((m_buffer_size > 0) && ((m_buffer_size & (m_buffer_size - 1)) == 0))) {
    ESP_LOGE(TAG, "Wrong buffer size. Need be power of 2.");
  } else if (m_mic == nullptr) {
    ESP_LOGCONFIG(TAG, "Setting i2s component failed due to missing i2s component");
  } else {
    ESP_LOGCONFIG(TAG, "Pass");
    m_buffer_len = 0;
    m_mic->add_data_callback(std::bind(&DetectAudio::micDataCb, this, std::placeholders::_1));
    // m_mic->stop();
    // m_mic->set_sample_rate(22627);
    // m_mic->set_use_apll(true);
    // m_mic->start();  // when you start i2s audio mic, this detection component would get data. No need to start here
  }
}

void DetectAudio::loop() {
  // this component works using its cb funkction
}

void DetectAudio::set_i2s(i2s_audio::I2SAudioMicrophone *mic) {
  ESP_LOGCONFIG(TAG, "Setting i2s component...");
  m_mic = mic;
}

void DetectAudio::addSoundSource(esphome::template_::TemplateBinarySensor *&soundSource, uint16_t peak) {
  m_soundSourcesIds.push_back({soundSource, peak, 0});
}

uint32_t DetectAudio::getCurrentPeak() { return m_currentPeak; }
float DetectAudio::getCurrentLoudness() { return m_currentLoudness; }
uint32_t DetectAudio::getLoudnessSum() { return m_sum; }
uint32_t DetectAudio::getLoudnessMax() { return m_mx; }
uint32_t DetectAudio::getLoudnessMin() { return m_mn; }
void DetectAudio::clearMetrics() {
  m_currentLoudness = m_sum = m_mx = 0;
  m_mn = 99999;
}

// calculates energy from Re and Im parts and places it back in the Re part (Im part is zeroed)
void DetectAudio::calculateEnergy() {
  for (uint16_t i = 0; i < m_buffer_len; i++) {
    m_real[i] = sq(m_real[i]) + sq(m_imag[i]);
    m_imag[i] = 0.0;
  }
}
// sums up energy in bins per octave
void DetectAudio::sumEnergy(float *energies, int bin_size, int num_octaves) {
  // skip the first bin
  int bin = bin_size;
  for (int octave = 0; octave < num_octaves; octave++) {
    float sum = 0.0;
    for (int i = 0; i < bin_size; i++) {
      sum += m_real[bin++];
    }
    energies[octave] = sum;
    bin_size *= 2;
  }
}

float DetectAudio::decibel(float v) { return 10.0 * log(v) / log(10); }
// converts energy to logaritmic, returns A-weighted sum
float DetectAudio::calculateLoudness(float *energies, const float *weights, int num_octaves, float scale) {
  float sum = 0.0;
  for (int i = 0; i < num_octaves; i++) {
    float energy = scale * energies[i];
    sum += energy * pow(10, weights[i] / 10.0);
    energies[i] = decibel(energy);
  }
  // ESP_LOGI(TAG, "decibel %f", sum);
  return decibel(sum);
}

unsigned int DetectAudio::countSetBits(unsigned int n) {
  unsigned int count = 0;
  while (n) {
    count += n & 1;
    n >>= 1;
  }
  return count;
}
// detecting 2 frequencies. Set wide to true to match the previous and next bin as well
bool DetectAudio::detectFrequency(unsigned int *mem, unsigned int minMatch, unsigned int peak, unsigned int bin1,
                                  unsigned int bin2, bool wide) {
  *mem = *mem << 1;
  if (peak == bin1 || peak == bin2 ||
      (wide && (peak == bin1 + 1 || peak == bin1 - 1 || peak == bin2 + 1 || peak == bin2 - 1))) {
    *mem |= 1;
  }
  ESP_LOGI(TAG, "peak %d     bin1 %d    bin2 %d", peak, bin1, bin2);
  if (countSetBits(*mem) >= minMatch) {
    return true;
  }
  return false;
}

void DetectAudio::calculateMetrics(int val) {
  m_cnt++;
  m_sum += val;
  if (val > m_mx) {
    m_mx = val;
  }
  if ((val < m_mn) && (val > 0)) {
    m_mn = val;
  }
}

void DetectAudio::micDataCb(const std::vector<int16_t> &data) {
  // ESP_LOGCONFIG(TAG, "Received mic data - size %d", data.size());

  if ((m_buffer_len + data.size()) <= m_buffer_size) {
    for (uint16_t i = 0; i < data.size(); i++) {
      m_real[m_buffer_len + i] = data.data()[i] / 10.0;
      m_imag[m_buffer_len + i] = 0.0;
    }
    m_buffer_len += data.size();
  }

  if (m_buffer_len + data.size() <= m_buffer_size) {
    return;
  }

  arduinoFFT fft(m_real, m_imag, m_buffer_len, m_buffer_len);

  // apply flat top window, optimal for energy calculations
  fft.Windowing(FFT_WIN_TYP_FLT_TOP, FFT_FORWARD);
  fft.Compute(FFT_FORWARD);

  // calculate energy in each bin
  calculateEnergy();
  float energy[OCTAVES];
  // sum up energy in bin for each octave
  sumEnergy(energy, 1, OCTAVES);
  // calculate loudness per octave + A weighted loudness
  m_currentLoudness = calculateLoudness(energy, aweighting, OCTAVES, 1.0);
  unsigned int peak = (int) floor(fft.MajorPeak());
  // Serial.println(peak);

  ESP_LOGI(TAG, "%s %s peak %d", __DATE__, __TIME__, peak);
  m_currentPeak = peak;

  for (auto &soundSource : m_soundSourcesIds) {
      ESP_LOGI(TAG, "detecting %d %d", soundSource.mem, soundSource.level);
    if (detectFrequency(&soundSource.mem, 15, peak, soundSource.level, soundSource.level + 1, true)) {
      soundSource.soundSensor->publish_state(true);
      ESP_LOGI(TAG, "source detected");
    } else {
      soundSource.soundSensor->publish_state(false);
      ESP_LOGI(TAG, "source not detected");
    }
  }

  calculateMetrics(m_currentLoudness);
  m_buffer_len = 0;
}

}  // namespace detect_audio

}  // namespace esphome

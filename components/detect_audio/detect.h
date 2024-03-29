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

#pragma once

// #define USE_ESP32

#ifdef USE_ESP32

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/i2s_audio/microphone/i2s_audio_microphone.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {

namespace detect_audio {

class DetectAudioButton : public button::Button {
  void press_action() override {}
};

class DetectAudio : public Component {
public:
  struct soundSource_t {
    binary_sensor::BinarySensor *soundSensor;
    uint16_t level;
    unsigned int mem;
  };

  DetectAudio();
  ~DetectAudio();

  void setup() override;

  void loop() override;

  void micDataCb(const std::vector<int16_t> &data);

  void set_i2s(i2s_audio::I2SAudioMicrophone *mic);

  void addSoundSource(std::string soundSourceName, uint16_t peak);

  void clearMetrics();

protected:
  i2s_audio::I2SAudioMicrophone *m_mic;

private:
  static constexpr auto m_buffer_size = 1024; // x^2
  float m_real[m_buffer_size];
  float m_imag[m_buffer_size];
  uint16_t m_buffer_len;
  unsigned int m_cnt;
  std::vector<soundSource_t> m_soundSourcesIds;
  sensor::Sensor m_currentPeak;
  sensor::Sensor m_currentLoudness;
  sensor::Sensor m_sum;
  sensor::Sensor m_mn;
  sensor::Sensor m_mx;
  DetectAudioButton m_clearMetrics;

  void calculateEnergy();

  void sumEnergy(float *energies, int bin_size, int num_octaves);

  float decibel(float v);

  float calculateLoudness(float *energies, const float *weights,
                          int num_octaves, float scale);

  unsigned int countSetBits(unsigned int n);

  bool detectFrequency(unsigned int *mem, unsigned int minMatch,
                       unsigned int peak, unsigned int bin1, unsigned int bin2,
                       bool wide);

  void calculateMetrics(int val);
};

} // namespace detect_audio
} // namespace esphome

#endif // USE_ESP32

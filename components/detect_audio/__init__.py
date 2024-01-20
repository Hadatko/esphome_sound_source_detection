# Copyright (c) 2024 Dusan Cervenka.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components.i2s_audio import microphone
from esphome.const import CONF_ID

CODEOWNERS = ["@hadatko"]
DEPENDENCIES = ["microphone"]

detect_audio_ns = cg.esphome_ns.namespace("detect_audio")
DetectAudioComponent = detect_audio_ns.class_("DetectAudio", cg.Component)
CONF_I2S_ID = "i2s_id"

CONF_DETECT_AUDIO_ID = "detect_audio_id"

CONFIG_SCHEMA =   cv.Schema( {
        cv.GenerateID(): cv.declare_id(DetectAudioComponent),
        cv.GenerateID(CONF_I2S_ID): cv.use_id(microphone.I2SAudioMicrophone)})

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    i2s_component = await cg.get_variable(config[CONF_I2S_ID])
    cg.add(var.set_i2s(i2s_component))
    await cg.register_component(var, config)

    # await microphone.register_microphone(var, config)

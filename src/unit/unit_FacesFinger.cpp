/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_FacesFinger.cpp
  @brief Faces Finger Module (FPC1020A via M-Bus) for M5UnitUnified
 */
#include "unit_FacesFinger.hpp"
#include <M5Utility.hpp>
#include <driver/gpio.h>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;

namespace m5 {
namespace unit {

const char UnitFacesFinger::name[] = "UnitFacesFinger";
const types::uid_t UnitFacesFinger::uid{"UnitFacesFinger"_mmh3};
const types::attr_t UnitFacesFinger::attr{attribute::AccessUART};

bool UnitFacesFinger::begin()
{
    // Faces Finger panel power ON
    if (_faces_cfg.panel_power_pin >= 0) {
        gpio_set_direction(static_cast<gpio_num_t>(_faces_cfg.panel_power_pin), GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(_faces_cfg.panel_power_pin), 1);
        M5_LIB_LOGI("Panel power ON (GPIO%d)", _faces_cfg.panel_power_pin);
    }

    // Touch IC power ON
    if (_faces_cfg.touch_power_pin >= 0) {
        gpio_set_direction(static_cast<gpio_num_t>(_faces_cfg.touch_power_pin), GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(_faces_cfg.touch_power_pin), 1);
        M5_LIB_LOGI("Touch IC power ON (GPIO%d)", _faces_cfg.touch_power_pin);
    }

    m5::utility::delay(100);

    return UnitFPC1020A::begin();
}

}  // namespace unit
}  // namespace m5

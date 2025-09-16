/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Characteristic example using M5UnitUnified for UnitFinger
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_FINGER) && !defined(USING_HAT_FINGER)
// #define USING_UNIT_FINGER
// #define USING_HAT_FINGER
#endif
// *************************************************************

using namespace m5::unit::fpc1xxx;

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
#if defined(USING_UNIT_FINGER)
m5::unit::UnitFinger unit;
#elif defined(USING_HAT_FINGER)
m5::unit::HatFinger unit;
#else
#error Please choose unit!
#endif
uint16_t target_user_id{};

}  // namespace

void setup()
{
    M5.begin();

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

#if defined(USING_HAT_FINGER)
    auto pin_num_in  = 26;
    auto pin_num_out = 0;
#else
    auto pin_num_in  = M5.getPin(m5::pin_name_t::port_c_rxd);
    auto pin_num_out = M5.getPin(m5::pin_name_t::port_c_txd);
#endif
    if (pin_num_in < 0 || pin_num_out < 0) {
        M5_LOGW("PortC is not available");
        Wire.end();
        pin_num_in  = M5.getPin(m5::pin_name_t::port_a_pin1);
        pin_num_out = M5.getPin(m5::pin_name_t::port_a_pin2);
    }
    M5_LOGI("getPin: %d,%d", pin_num_in, pin_num_out);

    // clang-format off
#if defined(CONFIG_IDF_TARGET_ESP32C6)
    auto& s = Serial1;
#elif SOC_UART_NUM > 2
    auto& s = Serial2;
#elif SOC_UART_NUM > 1
    auto& s = Serial1;
#else
#error "Not enough Serial"
#endif
    // clang-format on
    s.end();
    s.begin(19200, SERIAL_8N1, pin_num_in, pin_num_out);

    if (!Units.add(unit, s) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
    M5_LOGI("%s", Units.debugInfo().c_str());
    lcd.fillScreen(TFT_DARKGREEN);

    if (unit.findAvailableUserID(target_user_id)) {
        M5.Log.printf("Lowest unregistered UserID: %u\n", target_user_id);
    }
}

void loop()
{
    M5.update();
    auto touch = M5.Touch.getDetail();
    Units.update();

    // Compare, identify, verify
    if (M5.BtnA.wasClicked() || touch.wasClicked()) {
        M5.Speaker.tone(1500, 20);
        lcd.drawString("Try scan", 0, 0);
        M5.Log.printf("Try scan for compare\n");

        uint8_t characteristic[193]{};
        if (unit.scanCharacteristic(characteristic)) {
            M5.Log.printf("SACN OK\n");
            m5::utility::log::dump(characteristic, 193, false);
        } else {
            M5_LOGE("Failed to scan");
            lcd.drawString("Try again    ", 0, 0);
            return;
        }

        M5.Log.printf("Compare\n");
        lcd.drawString("Compare...      ", 0, 0);
        M5.Speaker.tone(2000, 20);

        bool match{};
        if (unit.compareCharacteristic(match, characteristic) && match) {
            M5.Log.printf("Compared\n");
            lcd.drawString("Compared         ", 0, 0);
            M5.Speaker.tone(2500, 20);
        } else {
            M5_LOGE("Failed to compare");
            lcd.drawString("Try again    ", 0, 0);
            return;
        }

        uint16_t user_id{};
        if (unit.identifyCharacteristic(user_id, characteristic)) {
            M5.Log.printf("Identify OK %u\n", user_id);
            lcd.setCursor(0, 0);
            lcd.printf("Identified %u        ", user_id);
            if (unit.verifyCharacteristic(match, user_id, characteristic) && match) {
                M5.Log.printf("Verify OK\n");
                lcd.setCursor(0, 0);
                lcd.printf("Verified %u       ", user_id);
            } else {
                M5_LOGE("Failed to verify");
            }
        } else {
            M5.Log.printf("No user\n");
            lcd.drawString("No user         ", 0, 0);
        }
        return;
    }

    // Register
    if (M5.BtnA.wasHold() || touch.wasHold()) {
        uint8_t characteristic[193]{};
        lcd.drawString("Try scan         ", 0, 0);
        M5.Log.printf("Try scan for register\n");
        if (unit.scanCharacteristic(characteristic)) {
            M5.Log.printf("SACN OK\n");
            m5::utility::log::dump(characteristic, 193, false);
        } else {
            M5_LOGE("Failed to scan");
            lcd.drawString("Try again    ", 0, 0);
            return;
        }

        auto user_id = target_user_id;
        if (unit.registerCharacteristic(user_id, 3, characteristic)) {
            M5.Log.printf("Registered characteristic user_id %u\n", user_id);
            ++target_user_id;
        } else {
            M5_LOGE("Failed to register");
            lcd.drawString("Try again    ", 0, 0);
            return;
        }
    }
}

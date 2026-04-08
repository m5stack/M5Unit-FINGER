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
#if !defined(USING_UNIT_FINGER) && !defined(USING_HAT_FINGER) && !defined(USING_FACES_FINGER)
// For UnitFinger (U008)
// #define USING_UNIT_FINGER
// For HatFinger (U074)
// #define USING_HAT_FINGER
// For FacesFinger (Faces Finger Module)
// #define USING_FACES_FINGER
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
#elif defined(USING_FACES_FINGER)
m5::unit::UnitFacesFinger unit;
#else
#error Please choose unit!
#endif
uint16_t target_user_id{};

#if defined(USING_HAT_FINGER)
struct UartPins {
    int rx;
    int tx;
};

UartPins get_hat_uart_pins(const m5::board_t board)
{
    switch (board) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
            return {26, 0};
        case m5::board_t::board_M5StickS3:
            return {0, 8};
        case m5::board_t::board_M5StackCoreInk:
            return {26, 25};
        default:
            return {-1, -1};
    }
}
#endif

#if defined(USING_FACES_FINGER)
// M-Bus pins for Faces Finger (GPIO varies by board)
struct FacesPins {
    int rx;             // UART RX (mbus_pin15)
    int tx;             // UART TX (mbus_pin16)
    int panel_power;    // Panel power (mbus_pin10)
    int touch_power;    // Touch IC power (mbus_pin20)
};

FacesPins get_faces_pins()
{
    return {
        M5.getPin(m5::pin_name_t::mbus_pin15),   // RX
        M5.getPin(m5::pin_name_t::mbus_pin16),   // TX
        M5.getPin(m5::pin_name_t::mbus_pin10),   // Panel power (GPIO26 on Core)
        M5.getPin(m5::pin_name_t::mbus_pin20),   // Touch IC power (GPIO5 on Core)
    };
}
#endif

}  // namespace

void setup()
{
    auto m5cfg = M5.config();
#if defined(USING_HAT_FINGER)
    m5cfg.pmic_button  = false;
    m5cfg.internal_imu = false;
    m5cfg.internal_rtc = false;
#endif
    M5.begin(m5cfg);
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

#if defined(USING_HAT_FINGER)
    const auto pins = get_hat_uart_pins(M5.getBoard());
    M5_LOGI("getHatPin: RX:%d TX:%d", pins.rx, pins.tx);
    if (pins.rx < 0 || pins.tx < 0) {
        M5_LOGE("No Hat port on this board");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
    auto pin_num_in  = pins.rx;
    auto pin_num_out = pins.tx;
#elif defined(USING_FACES_FINGER)
    const auto fp = get_faces_pins();
    M5_LOGI("getFacesPin: RX:%d TX:%d PWR:%d TCH:%d", fp.rx, fp.tx, fp.panel_power, fp.touch_power);
    if (fp.rx < 0 || fp.tx < 0) {
        M5_LOGE("No M-Bus on this board");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
    auto pin_num_in  = fp.rx;
    auto pin_num_out = fp.tx;

    // Set Faces config from M-Bus pins
    {
        auto cfg            = unit.config();
        cfg.panel_power_pin = fp.panel_power;
        cfg.touch_power_pin = fp.touch_power;
        unit.config(cfg);
    }
#else
    auto pin_num_in  = M5.getPin(m5::pin_name_t::port_c_rxd);
    auto pin_num_out = M5.getPin(m5::pin_name_t::port_c_txd);
    if (pin_num_in < 0 || pin_num_out < 0) {
        M5_LOGW("PortC is not available");
        // NanoC6: Ex_I2C.setPort() registers m5gfx::i2c on GROVE pins;
        // Wire.end() alone won't release it, causing dual-driver conflict on uart_driver_install
        if (M5.getBoard() == m5::board_t::board_M5NanoC6) {
            M5.Ex_I2C.release();
        }
        Wire.end();
        pin_num_in  = M5.getPin(m5::pin_name_t::port_a_pin1);
        pin_num_out = M5.getPin(m5::pin_name_t::port_a_pin2);
    }
#endif
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
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());
    lcd.fillScreen(TFT_DARKGREEN);

    if (unit.findAvailableUserID(target_user_id)) {
        M5.Log.printf("Lowest unregistered UserID: %u\n", target_user_id);
    }
}

void loop()
{
    M5.update();
    Units.update();

    // Compare, identify, verify
    if (M5.BtnA.wasClicked()) {
        M5.Speaker.tone(1500, 20);
        lcd.drawString("Try scan", 0, 0);
        M5.Log.printf("Try scan for compare\n");

        uint8_t characteristic[193]{};
        if (unit.scanCharacteristic(characteristic)) {
            M5.Log.printf("SCAN OK\n");
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
    if (M5.BtnA.wasHold()) {
        uint8_t characteristic[193]{};
        lcd.drawString("Try scan         ", 0, 0);
        M5.Log.printf("Try scan for register\n");
        if (unit.scanCharacteristic(characteristic)) {
            M5.Log.printf("SCAN OK\n");
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

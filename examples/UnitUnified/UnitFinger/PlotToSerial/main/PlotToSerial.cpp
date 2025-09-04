/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Simple example using M5UnitUnified for UnitFinger
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

void print_all_users()
{
    std::vector<User> v{};
    if (unit.readAllUser(v)) {
        M5.Log.printf("All user data (%u):\n", v.size());
        uint16_t idx{};
        for (auto&& u : v) {
            M5.Log.printf("  [%3d] ID:%5u, PERMISSION:%u\n", idx++, u.id, u.permission);
        }
    }
}

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

    {
        uint8_t clv{}, timeout{};
        Mode mode{};
        uint16_t user_count{};
        unit.readRegistrationMode(mode);
        unit.readComparisonLevel(clv);
        unit.readTimeout(timeout);
        unit.readRegisteredUserCount(user_count);

        M5.Log.printf("=== %s information ===\n", unit.deviceName());
        M5.Log.printf("           Mode: %s\n", mode == Mode::DenyDuplicate ? "Deny duplicate" : "Allow duplicate");
        M5.Log.printf("  Comparison Lv: %u\n", clv);
        M5.Log.printf("        Timeout: %u\n", timeout);
        M5.Log.printf("Registered user: %u\n", user_count);
        print_all_users();
        if (unit.seachUnregisterdUserID(target_user_id)) {
            M5.Log.printf("Lowest unregistered UserID: %u\n", target_user_id);
        }

        uint8_t characteristic[193]{};
        if (target_user_id > 1 && unit.readUserCharacteristic(characteristic, target_user_id - 1)) {
            M5.Log.printf("--- UserID:%u characteristic\n", target_user_id - 1);
            m5::utility::log::dump(characteristic, m5::stl::size(characteristic), false);
        }
    }
    lcd.fillScreen(TFT_DARKGREEN);
}

void loop()
{
    M5.update();
    auto touch = M5.Touch.getDetail();
    Units.update();

    // Register finger
    if (M5.BtnA.wasHold() || touch.wasHold()) {
        lcd.fillScreen(TFT_DARKGREEN);
        M5.Speaker.tone(2500, 20);
        lcd.drawString("Try register", 0, 0);
        M5.Log.printf("Try register\n");

        if (unit.registerFinger(target_user_id, 1)) {
            M5.Speaker.tone(2000, 20);
            lcd.setCursor(0, 0);
            lcd.printf("Registered %u", target_user_id);
            M5.Log.printf("Registered user %u\n", target_user_id);
            ++target_user_id;
        } else {
            M5.Log.printf("Failed to register %u", target_user_id);
            lcd.drawString("Try again       ", 0, 0);
        }
        return;
    }

    // Identify and verify finger
    if (M5.BtnA.wasClicked() || touch.wasClicked()) {
        uint16_t id{};
        uint8_t per{};
        M5.Speaker.tone(1500, 20);
        lcd.drawString("Try identify", 0, 0);
        M5.Log.printf("Try identify\n");
        if (unit.identifyFinger(id, per)) {
            if (id) {
                M5.Speaker.tone(2000, 20);
                M5.Log.printf("Identified user:%u, permission:%u\n", id, per);

                // If it was identified, it should pass verification
                lcd.drawString("Try verify        ", 0, 0);
                M5.Log.printf("Try verify\n");
                bool match{};
                if (unit.verifyFinger(match, id) && match) {
                    M5.Speaker.tone(2500, 20);
                    M5.Log.printf("Verified: %u\n", id);
                    lcd.drawString("Verified         ", 0, 0);
                } else {
                    M5.Log.printf("Failed or not match %u\n", id);
                    lcd.drawString("Failed to Verify   ", 0, 0);
                }
            } else {
                M5.Speaker.tone(800, 40);
                M5.Log.printf("No user\n");
                lcd.drawString("No user       ", 0, 0);
            }
        } else {
            M5.Speaker.tone(800, 40);
            M5.Log.printf("No user\n");
            lcd.drawString("No user       ", 0, 0);
        }
    }
}

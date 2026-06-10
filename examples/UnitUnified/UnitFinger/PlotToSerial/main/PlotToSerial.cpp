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
#include <wiring/m5_unit_unified_wiring.hpp>
#include <wiring/m5_unit_finger_wiring.hpp>

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_FINGER) && !defined(USING_HAT_FINGER) && !defined(USING_FACES_FINGER)
// For UnitFinger (SKU:U008)
// #define USING_UNIT_FINGER
// For HatFinger (SKU:U074)
// #define USING_HAT_FINGER
// For FacesFinger (SKU:A066)
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
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

#if defined(USING_HAT_FINGER)
    // HatFinger: board-aware Hat UART (wiring helper covers StickCPlus/Plus2/StickS3/CoreInk/NessoN1).
    if (!m5::unit::wiring::addHatUART(Units, unit, 19200) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#elif defined(USING_FACES_FINGER)
    // FacesFinger: M-Bus UART with panel / touch power pin control (FINGER-local wiring helper).
    if (!m5::unit::fpc1xxx::faces::wiring::addFacesUART(Units, unit, 19200) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#else  // USING_UNIT_FINGER
    // UnitFinger (GROVE PortC): wiring helper handles PortA fallback + NanoC6/NanoH2 Ex_I2C release.
    if (!m5::unit::wiring::addUART(Units, unit, 19200) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#endif

    M5_LOGI("M5UnitUnified initialized");
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
        M5.Log.printf("           Mode: %s\n",
                      mode == Mode::ProhibitDuplicate ? "Prohibit duplicate" : "Allow duplicate");
        M5.Log.printf("  Comparison Lv: %u\n", clv);
        M5.Log.printf("        Timeout: %u\n", timeout);
        M5.Log.printf("Registered user: %u\n", user_count);
        print_all_users();
        if (unit.findAvailableUserID(target_user_id)) {
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
    Units.update();

    // Register finger
    if (M5.BtnA.wasHold()) {
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
    if (M5.BtnA.wasClicked()) {
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

#if !defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#if CONFIG_FREERTOS_UNICORE
static inline void feedIdleTaskPeriodically(void)
{
    constexpr uint32_t FEED_INTERVAL_MS   = 2000;
    constexpr TickType_t FEED_SLEEP_TICKS = pdMS_TO_TICKS(5);
    static uint32_t s_next_feed_ms        = 0;
    const uint32_t now_ms                 = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (now_ms >= s_next_feed_ms) {
        s_next_feed_ms = now_ms + FEED_INTERVAL_MS;
        vTaskDelay(FEED_SLEEP_TICKS);
    }
}
#endif

extern "C" void app_main(void)
{
    setup();
    for (;;) {
#if CONFIG_FREERTOS_UNICORE
        feedIdleTaskPeriodically();
#endif
        loop();
    }
}
#endif

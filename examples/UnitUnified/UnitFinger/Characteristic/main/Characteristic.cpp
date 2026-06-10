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

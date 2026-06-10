/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Capture finger and show image example using M5UnitUnified for Unit/HatFinger
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
LGFX_Sprite sprite4, sprite8;

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

void make_sprite8(LGFX_Sprite& s, const std::vector<uint8_t>& v, const uint16_t wid, const uint16_t hgt)
{
    // Make 8bit grayscale sprite image
    for (int y = 0; y < hgt; ++y) {
        for (int x = 0; x < wid; ++x) {
            s.writePixel(x, y, v[y * wid + x]);
        }
    }
}

void make_sprite4(LGFX_Sprite& s, const std::vector<uint8_t>& v, const uint16_t wid, const uint16_t hgt)
{
    // Make 4bit grayscale sprite image
    for (int y = 0; y < hgt; ++y) {
        for (int x = 0; x < wid; x += 2) {
            uint8_t nibbles = v[y * (wid >> 1) + x / 2];
            s.writePixel(x, y, ((nibbles >> 4) & 0x0F));
            s.writePixel(x + 1, y, (nibbles & 0x0F));
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

    //
    if (lcd.width() && lcd.height()) {
        sprite4.setPsram(false);
        sprite4.setColorDepth(lgfx::color_depth_t::grayscale_4bit);
        sprite4.createSprite(m5::unit::UnitFinger::RESOLUTION_WIDTH >> 1, m5::unit::UnitFinger::RESOLUTION_HEIGHT >> 1);
        sprite4.createPalette();

        sprite8.setPsram(false);
        sprite8.setColorDepth(lgfx::color_depth_t::grayscale_8bit);
        sprite8.createSprite(m5::unit::UnitFinger::RESOLUTION_WIDTH, m5::unit::UnitFinger::RESOLUTION_HEIGHT);
        sprite8.createPalette();
    }
    lcd.fillScreen(TFT_DARKGREEN);
}

void loop()
{
    static bool raw{};

    M5.update();
    Units.update();

    if (M5.BtnA.wasClicked()) {
        lcd.fillScreen(TFT_DARKGREEN);
        M5.Speaker.tone(1500, 20);

        lcd.setCursor(0, 0);
        lcd.printf("Try capture %s", raw ? "RAW" : "COMPRESSED");
        M5.Log.printf("Try capture %s\n", raw ? "RAW" : "COMPRESSED");

        std::vector<uint8_t> v{};
        //
        // NOTE: It takes a lot of time if raw is true
        //
        if (unit.captureImage(v, raw)) {
            M5.Speaker.tone(3000, 20);
            if (lcd.width() && lcd.height()) {
                if (raw) {
                    make_sprite8(sprite8, v, unit.imageWidth(raw), unit.imageHeight(raw));
                    sprite8.pushSprite(&lcd, 0, 0);
                } else {
                    make_sprite4(sprite4, v, unit.imageWidth(raw), unit.imageHeight(raw));
                    sprite4.pushSprite(&lcd, 0, 0);
                }
            } else {
                M5.Log.printf("Captured data\n");
                m5::utility::log::dump(v.data(), v.size(), false);
            }
            raw = !raw;
        } else {
            M5_LOGE("Failed to capture");
            lcd.drawString("Try again       ", 0, 0);
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

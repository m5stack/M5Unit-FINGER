/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Capture finger and show image example using M5UnitUnified for UnitFinger2
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <algorithm>
#include <cstring>
#include <cstdio>

using namespace m5::unit::finger2;

namespace {
auto& lcd = M5.Display;
LGFX_Sprite sprite4;

m5::unit::UnitUnified Units;
m5::unit::UnitFinger2 unit;

constexpr char GUIDE[] = "Place finger, Click:Capture";

// Screen state (the background color tells the state)
enum class State : uint8_t { Idle, Place, Release, Busy, Success, Failure };

struct StateStyle {
    uint16_t bg;
    uint16_t fg;
};
constexpr StateStyle state_style[] = {
    {TFT_DARKGREY, TFT_WHITE},   // Idle
    {TFT_YELLOW, TFT_BLACK},     // Place
    {TFT_BLUE, TFT_WHITE},       // Release
    {TFT_ORANGE, TFT_BLACK},     // Busy
    {TFT_DARKGREEN, TFT_WHITE},  // Success
    {TFT_RED, TFT_WHITE},        // Failure
};

// Small text font: AsciiFont8x16 (scaled on a large screen), or Font0 (6x8) if the text does not fit the width w
void set_small_font(const char* s, const int32_t w)
{
    lcd.setFont(&fonts::AsciiFont8x16);
    lcd.setTextSize(std::max<int32_t>(1, lcd.width() / 320));
    if (lcd.textWidth(s) > w) {
        lcd.setFont(&fonts::Font0);
        lcd.setTextSize(1);
    }
}

// Small text whose top (or bottom if bottom is true) is at y
// It is split into two lines at the last space if it does not fit, returns the height used
int32_t draw_small(const char* s, const int32_t x, const int32_t y, const bool bottom)
{
    const int32_t w = lcd.width() - x;
    set_small_font(s, w);
    lcd.setTextDatum(bottom ? textdatum_t::bottom_left : textdatum_t::top_left);
    const int32_t fh     = lcd.fontHeight();
    const char* const sp = strrchr(s, ' ');
    if (lcd.textWidth(s) <= w || !sp) {
        lcd.drawString(s, x, y);
        return fh;
    }
    char first[48]{};
    snprintf(first, sizeof(first), "%.*s", static_cast<int>(sp - s), s);
    lcd.drawString(first, x, bottom ? y - fh : y);
    lcd.drawString(sp + 1, x, bottom ? y : y + fh);
    return fh * 2;
}

// Status (large, centered) and guide (small, bottom)
void draw_status(const State state, const char* status, const char* guide = GUIDE)
{
    if (!lcd.width()) {
        return;  // No display
    }
    const auto& st = state_style[m5::stl::to_underlying(state)];
    lcd.startWrite();
    lcd.fillScreen(st.bg);
    lcd.setFont(&fonts::AsciiFont8x16);
    lcd.setTextColor(st.fg, st.bg);
    lcd.setTextSize(std::max<int32_t>(1, lcd.width() / 160));
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.drawString(status, lcd.width() >> 1, lcd.height() >> 1);
    draw_small(guide, 0, lcd.height(), true);
    lcd.endWrite();
}

// Captured image on the left, caption on the right
void draw_image(LGFX_Sprite& s, const char* caption)
{
    draw_status(State::Success, "");
    lcd.startWrite();
    s.pushSprite(&lcd, 0, 0);
    draw_small(caption, s.width() + 8, 0, false);
    lcd.endWrite();
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

    // UnitFinger2 (GROVE PortC): wiring helper handles PortA fallback + NanoC6/NanoH2 Ex_I2C release.
    if (!m5::unit::wiring::addUART(Units, unit, 115200) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());
    if (lcd.width() && lcd.height()) {
        sprite4.setPsram(false);
        sprite4.setColorDepth(lgfx::color_depth_t::grayscale_4bit);
        sprite4.createSprite(m5::unit::UnitFinger2::IMAGE_WIDTH, m5::unit::UnitFinger2::IMAGE_HEIGHT);
        sprite4.createPalette();
    }
    draw_status(State::Idle, "Ready");
}

void loop()
{
    M5.update();
    Units.update();

    if (M5.BtnA.wasClicked()) {
        M5.Speaker.tone(2500, 20);
        draw_status(State::Busy, "Capturing");
        M5.Log.printf("Try capture\n");

        std::vector<uint8_t> v{};
        if (unit.wakeup()) {
            bool detected{};
            if (unit.capture(detected)) {
                uint8_t percentage{};
                bool quality{};
                if (unit.readImageInformation(percentage, quality)) {
                    M5.Log.printf("ImageInfo:%u%% quality:%u\n", percentage, quality);
                }
                if (detected) {
                    if (unit.readImage(v)) {
                        M5.Speaker.tone(4000, 20);
                        M5.Log.printf("Successful\n");
                        if (lcd.width() && lcd.height()) {
                            make_sprite4(sprite4, v, unit.imageWidth(), unit.imageHeight());
                            draw_image(sprite4, "Captured");
                        } else {
                            M5.Log.printf("Image data %u x %u\n", unit.imageWidth(), unit.imageHeight());
                            m5::utility::log::dump(v.data(), v.size(), false);
                        }
                        return;

                    } else {
                        M5_LOGE("Failed to readImage");
                    }
                } else {
                    M5_LOGE("Finger not detected");
                    M5.Speaker.tone(1000, 20);
                    draw_status(State::Failure, "No finger");
                    return;
                }
            } else {
                M5_LOGE("Failed to capture");
            }
        } else {
            M5_LOGE("Failed to wakeup");
        }
        M5.Speaker.tone(1000, 20);
        draw_status(State::Failure, "Try again");
    }
}

#if !defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#if CONFIG_FREERTOS_UNICORE
// Single-core SoCs: run loop() back-to-back, but every ~2 s yield a 5 ms slice so the IDLE task
// runs and feeds the task watchdog (default 5 s).
static inline void feedIdleTaskPeriodically(void)
{
    constexpr uint32_t FEED_INTERVAL_MS{2000};
    constexpr TickType_t FEED_SLEEP_TICKS{pdMS_TO_TICKS(5)};
    static uint32_t s_last_feed_ms{};
    const uint32_t now_ms{static_cast<uint32_t>(esp_timer_get_time() / 1000)};
    if (now_ms - s_last_feed_ms >= FEED_INTERVAL_MS) {
        s_last_feed_ms = now_ms;
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

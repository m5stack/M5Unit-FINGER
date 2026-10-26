/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  autoEnroll/Identify example using M5UnitUnified for UnitFinger2
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

m5::unit::UnitUnified Units;
m5::unit::UnitFinger2 unit;
auto_enroll_flag_t enroll_flags = auto_enroll_flag::ALLOW_OVERWRITE_PAGE;
constexpr uint8_t CAPTURE_TIMES{5};

constexpr char GUIDE[] = "Hold:Register Click:Identify";

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

void print_exists_users()
{
    uint16_t num{};
    if (unit.wakeup() && unit.readValidTemplates(num)) {
        uint8_t table[32]{};
        M5.Log.printf("=== Exists templates %u/%u ===\n", num, unit.capacity());
        if (unit.readIndexTable(table)) {
            for (uint16_t id = 0; id < 32 * 8; ++id) {
                uint32_t idx   = id >> 3;
                uint32_t shift = id & 0x07;
                if (table[idx] & (1U << shift)) {
                    M5.Log.printf("page:%03u\n", id);
                }
            }
        }
    }
}

bool callback_enroll(const uint16_t call_times, const uint16_t page_id, const ConfirmCode confirm,
                     const AutoEnrollStage stage, const uint8_t state)
{
    char status[24]{};
    switch (stage) {
        case AutoEnrollStage::GetImage:
            snprintf(status, sizeof(status), "Reading %u/%u", state, CAPTURE_TIMES);
            draw_status(State::Busy, status);
            break;
        case AutoEnrollStage::GenerateCharacteristic:
            if (confirm != ConfirmCode::OK) {
                // The unit waits for the next capture by itself
                snprintf(status, sizeof(status), "Again %u/%u", state, CAPTURE_TIMES);
                draw_status(State::Place, status);
                M5.Log.printf("Please touch finger again(%u)\n", state);
            } else if (state < CAPTURE_TIMES) {
                M5.Speaker.tone(3000, 20);
                snprintf(status, sizeof(status), "Release %u/%u", state, CAPTURE_TIMES);
                draw_status(State::Release, status);
                M5.Log.printf("Please release finger(%u)\n", state);
            }
            break;
        case AutoEnrollStage::ReleasedFinger:
            snprintf(status, sizeof(status), "Place %u/%u", state + 1, CAPTURE_TIMES);
            draw_status(State::Place, status);
            M5.Log.printf("Please touch finger(%u)\n", state + 1);
            break;
        case AutoEnrollStage::MergeTemplate:
        case AutoEnrollStage::Inspection:
            draw_status(State::Busy, "Storing");
            break;
        default:
            break;
    }
    M5.Log.printf("  Enroll[%02u]:%02u [%02X] (%02X)\n", call_times, (uint8_t)stage, (uint8_t)confirm, state);
    return true;  // Abort if false
}

void auto_enroll()
{
    static bool highlow{};

    if (unit.wakeup()) {
        ConfirmCode confirm{};
        // Available lowest/highest page_id
        uint16_t page{0xFFFF};
        if (!(highlow == false ? unit.findLowestAvailablePage(page) : unit.findHighestAvailablePage(page)) ||
            page == 0xFFFF) {
            M5_LOGE("Failed or Invalid page");
            return;
        }

        M5.Log.printf("Try register to %u\n", page);
        char status[24]{};
        snprintf(status, sizeof(status), "Place 1/%u", CAPTURE_TIMES);
        draw_status(State::Place, status);
        if (unit.autoEnroll(confirm, page, CAPTURE_TIMES, enroll_flags, callback_enroll)) {
            highlow = !highlow;
            M5.Log.printf("Registered: %u\n", page);
            M5.Speaker.tone(2000, 20);
            snprintf(status, sizeof(status), "Registered %u", page);
            draw_status(State::Success, status);
        } else {
            M5.Speaker.tone(800, 40);
            draw_status(State::Failure, "Failed");
            M5_LOGE("Failed to autoEnroll %02X, %u", confirm, page);
            switch (confirm) {
                case ConfirmCode::TemplateNotEmpty:
                    M5_LOGE("The specified page %u already has a fingerprint registered", page);
                    break;
                case ConfirmCode::AlreadyExists:
                    M5_LOGE("The same fingerprint template can not registered");
                    break;
                case ConfirmCode::OperationBlocked:
                    M5_LOGE("Abort in callback");
                    break;

                default:
                    break;
            }
        }
    } else {
        M5_LOGE("Failed to wakeup");
    }
}

bool callback_identify(const uint16_t call_times, const ConfirmCode confirm, const AutoIdentifyStage stage)
{
    if (stage == AutoIdentifyStage::GetImage) {
        draw_status(State::Busy, "Searching");
    }
    M5.Log.printf("  Identify[%02u]:%02u [%02X]\n", call_times, (uint8_t)stage, (uint8_t)confirm);
    return true;  // Abort if false
}

void auto_identify()
{
    bool matched{};
    uint16_t page{};
    uint16_t score{};

    if (unit.wakeup()) {
        char status[24]{};
        // 1:N
        draw_status(State::Place, "Place finger");
        if (unit.autoIdentify(matched, page, score, 0xFFFF, 0, 0x0000, callback_identify)) {
            M5.Log.printf("<1:N> %s: Page:%u: Score:%u\n", matched ? "Found" : "Not found", page, score);
            if (matched) {
                // 1:1
                draw_status(State::Place, "Place again");
                if (unit.autoIdentify(matched, page, score, page, 0, 0x0000, callback_identify)) {
                    M5.Log.printf("<1:1> %s: Page:%u: Score:%u\n", matched ? "Match" : "Unmatched", page, score);
                    snprintf(status, sizeof(status), matched ? "Matched %u" : "Unmatched %u", page);
                    draw_status(matched ? State::Success : State::Failure, status);
                } else {
                    M5_LOGE("Failed to autoIdentify(1:1)");
                    draw_status(State::Failure, "Failed");
                }
            } else {
                draw_status(State::Failure, "Not found");
            }
        } else {
            M5_LOGE("Failed to autoIdentify(1:N)");
            draw_status(State::Failure, "Failed");
        }
    } else {
        M5_LOGE("Failed to wakeup");
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

    print_exists_users();
    draw_status(State::Idle, "Place finger");
}

void loop()
{
    M5.update();
    Units.update();

    // Identify
    if (M5.BtnA.wasClicked()) {
        M5.Speaker.tone(2000, 20);
        auto_identify();
    }

    // Register
    if (M5.BtnA.wasHold()) {
        M5.Speaker.tone(4000, 20);
        auto_enroll();
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

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Simple example using M5UnitUnified for UnitFinger2
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <algorithm>

using namespace m5::unit::finger2;

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitFinger2 unit;

uint16_t template_count{};
char last_result[48]{};

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

// Header (unit / templates), status (large) and guide / last result
void draw_screen(const State state, const char* status)
{
    if (!lcd.width()) {
        return;  // No display
    }
    const auto& st         = state_style[m5::stl::to_underlying(state)];
    const int32_t small_sz = std::max<int32_t>(1, lcd.width() / 320);
    const int32_t large_sz = std::max<int32_t>(1, lcd.width() / 160);
    const int32_t line_h   = 16 * small_sz;

    char header[48]{};
    snprintf(header, sizeof(header), "%s templates:%u/%u", unit.deviceName(), template_count, unit.capacity());

    lcd.startWrite();
    lcd.fillScreen(st.bg);
    lcd.setFont(&fonts::AsciiFont8x16);
    lcd.setTextColor(st.fg, st.bg);

    lcd.setTextSize(small_sz);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.drawString(header, 0, 0);

    lcd.setTextSize(large_sz);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.drawString(status, lcd.width() >> 1, lcd.height() >> 1);

    lcd.setTextSize(small_sz);
    lcd.setTextDatum(textdatum_t::bottom_left);
    lcd.drawString("Hold:Register Click:Search", 0, lcd.height() - line_h);
    lcd.drawString(last_result, 0, lcd.height());
    lcd.endWrite();
}

void update_template_count()
{
    if (!unit.wakeup() || !unit.readValidTemplates(template_count)) {
        M5_LOGW("Failed to read the number of templates");
    }
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

// Example of registration like as autoEnroll
// Registered IDs will be overwritten.
// Existing templates will not be considered.
bool register_finger()
{
    static bool highlow{};

    if (unit.wakeup()) {
        // Available lowest/highest page_id
        uint16_t page{0xFFFF};
        if (!(highlow == false ? unit.findLowestAvailablePage(page) : unit.findHighestAvailablePage(page)) ||
            page == 0xFFFF) {
            M5_LOGE("Failed or Invalid page");
            return false;
        }
        M5.Log.printf("Try register to %u\n", page);

        bool detected{};
        uint8_t cnt{};
        while (cnt < 5) {
            // Wait touch finger
            char status[24]{};
            snprintf(status, sizeof(status), "Place %u/5", cnt + 1);
            draw_screen(State::Place, status);
            M5.Log.printf("Please touch finger(%u)\n", cnt + 1);

            const auto start_at = m5::utility::millis();
            bool touched{};
            do {
                touched = unit.capture(detected, true /*for enroll */) && detected;
                m5::utility::delay(1);
            } while (!touched && !m5::utility::hasElapsed(start_at, 5000));
            if (!touched) {
                M5_LOGE("Finger undetected %u", cnt + 1);
                return false;
            }

            // Generate characteristic (1-5)
            if (!unit.generateCharacteristic(cnt + 1)) {
                M5_LOGE("Failed to GenerateCharacteristic %u", cnt + 1);
                return false;
            }

            // Wait release finger
            if (cnt < 4) {
                M5.Speaker.tone(3000, 20);
                snprintf(status, sizeof(status), "Release %u/5", cnt + 1);
                draw_screen(State::Release, status);
                M5.Log.printf("Please release finger(%u)\n", cnt + 1);

                const auto start_at = m5::utility::millis();
                bool released{};
                do {
                    released = unit.capture(detected, true) && !detected;
                    m5::utility::delay(1);
                } while (!released && !m5::utility::hasElapsed(start_at, 5000));
                if (!released) {
                    M5_LOGE("Finger not released %u", cnt + 1);
                    return false;
                }
            }

            ++cnt;
        }

        // Generate template(merge 1-5)
        draw_screen(State::Busy, "Storing");
        if (!unit.generateTemplate()) {
            M5_LOGE("Failed to generateTemplate");
            return false;
        }

        // Store template
        if (!unit.storeTemplate(page)) {
            M5_LOGE("Failed to storeTemplate");
            return false;
        }

        highlow = !highlow;
        M5.Log.printf("Registered: %u\n", page);
        snprintf(last_result, sizeof(last_result), "Registered page:%u", page);
        return true;
    }
    return false;
}

// 1:1
bool match_finger(const uint16_t page)
{
    bool detected{};
    if (unit.capture(detected) && detected && unit.generateCharacteristic(1) && unit.loadTemplate(2, page)) {
        bool matched{};
        uint16_t score{};
        if (unit.match(matched, score)) {
            M5.Log.printf("<1:1>%u %s, Score:%u\n", page, matched ? "Matched" : "Unmatched", score);
            return matched;
        } else {
            M5_LOGE("Failed to match");
        }
    } else {
        M5_LOGE("Failed to any function");
    }
    return false;
}

// 1:N
uint16_t search_finger()
{
    bool detected{};
    // search is performed on the specified buffer
    if (unit.capture(detected) && detected && unit.generateCharacteristic(1)) {
        bool matched{};
        uint16_t page{};
        uint16_t score{};
        if (unit.search(matched, page, score, 1 /* buffer id */)) {
            M5.Log.printf("<1:N> %s Page:%u Score:%u\n", matched ? "Found" : "Not found", page, score);
            if (matched) {
                snprintf(last_result, sizeof(last_result), "Found page:%u score:%u", page, score);
            } else {
                snprintf(last_result, sizeof(last_result), "Not found");
            }
            return matched ? page : 0xFFFF;
        } else {
            M5_LOGE("Failed to search");
            snprintf(last_result, sizeof(last_result), "Search failed");
        }
    } else {
        M5_LOGE("Failed to capture/generate");
        snprintf(last_result, sizeof(last_result), "Capture failed");
    }
    return 0xFFFF;
}

// 1:N
uint16_t search_now_finger()
{
    bool detected{};
    // searchNow is performed on the last generated characteristic
    if (unit.capture(detected) && detected && unit.generateCharacteristic(2)) {
        bool matched{};
        uint16_t page{};
        uint16_t score{};
        if (unit.searchNow(matched, page, score)) {
            M5.Log.printf("<1:N now> %s Page:%u Score:%u\n", matched ? "Found" : "Not found", page, score);
            return matched ? page : 0xFFFF;
        } else {
            M5_LOGE("Failed to search");
        }
    } else {
        M5_LOGE("Failed to capture/generate");
    }
    return 0xFFFF;
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
    update_template_count();
    draw_screen(State::Idle, "Place finger");
}

void loop()
{
    M5.update();
    Units.update();

    // Search,Match
    if (M5.BtnA.wasClicked()) {
        M5.Speaker.tone(2000, 20);
        draw_screen(State::Busy, "Searching");
        auto page = search_finger();
        if (page != 0xFFFF) {
            // Check the same finger again with searchNow (1:N) and match (1:1)
            const bool now_ok   = (search_now_finger() == page);
            const bool match_ok = now_ok && match_finger(page);
            if (match_ok) {
                M5.Speaker.tone(2500, 20);
                draw_screen(State::Success, "Found");
            } else {
                M5.Speaker.tone(800, 40);
                snprintf(last_result, sizeof(last_result), "page:%u %s", page,
                         now_ok ? "1:1 unmatched" : "1:N(now) unmatched");
                draw_screen(State::Failure, "Unmatched");
            }
        } else {
            M5.Speaker.tone(800, 40);
            draw_screen(State::Failure, "Not found");
        }
    }

    // Register
    if (M5.BtnA.wasHold()) {
        M5.Speaker.tone(4000, 20);
        if (register_finger()) {
            M5.Speaker.tone(2000, 20);
            update_template_count();
            draw_screen(State::Success, "Registered");
        } else {
            M5.Speaker.tone(800, 40);
            snprintf(last_result, sizeof(last_result), "Register failed");
            draw_screen(State::Failure, "Try again");
        }
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

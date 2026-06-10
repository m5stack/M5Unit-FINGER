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

using namespace m5::unit::finger2;

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitFinger2 unit;

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
void register_finger()
{
    static bool highlow{};

    if (unit.wakeup()) {
        // Available lowest/highest page_id
        uint16_t page{0xFFFF};
        if (!(highlow == false ? unit.findLowestAvailablePage(page) : unit.findHighestAvailablePage(page)) ||
            page == 0xFFFF) {
            M5_LOGE("Failed or Invalid page");
            return;
        }
        M5.Log.printf("Try register to %u\n", page);

        bool detected{};
        uint8_t cnt{};
        while (cnt < 5) {
            // Wait touch finger
            lcd.fillScreen(TFT_ORANGE);
            lcd.drawString("Please touch finger", 0, 16);
            M5.Log.printf("Please touch finger(%u)\n", cnt + 1);

            auto timeout_at = m5::utility::millis() + 5000;
            bool touched{};
            do {
                touched = unit.capture(detected, true /*for enroll */) && detected;
                m5::utility::delay(1);
            } while (!touched && m5::utility::millis() <= timeout_at);
            if (!touched) {
                M5_LOGE("Finger undetected %u", cnt + 1);
                return;
            }

            // Generate characteristic (1-5)
            if (!unit.generateCharacteristic(cnt + 1)) {
                M5_LOGE("Failed to GenerateCharacteristic %u", cnt + 1);
                return;
            }

            // Wait release finger
            if (cnt < 4) {
                M5.Speaker.tone(3000, 20);
                lcd.fillScreen(TFT_BLUE);
                lcd.drawString("Please release finger", 0, 16);
                M5.Log.printf("Please release finger(%u)\n", cnt + 1);

                auto timeout_at = m5::utility::millis() + 5000;
                bool released{};
                do {
                    released = unit.capture(detected, true) && !detected;
                    m5::utility::delay(1);
                } while (!released && m5::utility::millis() <= timeout_at);
                if (!released) {
                    M5_LOGE("Finger not released %u", cnt + 1);
                    return;
                }
            }

            ++cnt;
        }

        // Generate template(merge 1-5)
        lcd.fillScreen(TFT_DARKGREEN);
        if (!unit.generateTemplate()) {
            M5_LOGE("Failed to generateTemplate");
            return;
        }

        // Store template
        if (!unit.storeTemplate(page)) {
            M5_LOGE("Failed to storeTemplate");
            return;
        }

        highlow = !highlow;
        M5.Log.printf("Registered: %u\n", page);
    }
}

// 1:1
void match_finger(const uint16_t page)
{
    bool detected{};
    if (unit.capture(detected) && detected && unit.generateCharacteristic(1) && unit.loadTemplate(2, page)) {
        bool matched{};
        uint16_t score{};
        if (unit.match(matched, score)) {
            M5.Log.printf("<1:1>%u %s, Score:%u\n", page, matched ? "Matched" : "Unmatched", score);
        } else {
            M5_LOGE("Failed to match");
        }
    } else {
        M5_LOGE("Failed to any function");
    }
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
            return matched ? page : 0xFFFF;
        } else {
            M5_LOGE("Failed to search");
        }
    } else {
        M5_LOGE("Failed to capture/generate");
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

    lcd.fillScreen(TFT_DARKGREEN);
    lcd.setFont(&fonts::AsciiFont8x16);

    print_exists_users();
}

void loop()
{
    M5.update();
    Units.update();

    // Search,Match
    if (M5.BtnA.wasClicked()) {
        M5.Speaker.tone(2000, 20);
        auto page = search_finger();
        if (page != 0xFFFF) {
            auto page2 = search_now_finger();
            if (page2 != 0xFFFF) {
                match_finger(page);
            }
        }
    }

    // Register
    if (M5.BtnA.wasHold()) {
        M5.Speaker.tone(4000, 20);
        register_finger();
        lcd.fillScreen(TFT_DARKGREEN);
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

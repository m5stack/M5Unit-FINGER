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
        auto page = highlow ? unit.findLowestAvailablePage() : unit.findHighestAvailablePage();
        if (page == 0xFFFF) {
            M5_LOGE("Invalild page");
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
            M5_LOGE("Failed to macth");
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
        if (unit.search(matched, page, score, 1 /* bufer id */)) {
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
    // seachNow is performed on the last generated characteristic
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

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    auto pin_num_in  = M5.getPin(m5::pin_name_t::port_c_rxd);
    auto pin_num_out = M5.getPin(m5::pin_name_t::port_c_txd);
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
    s.begin(115200, SERIAL_8N1, pin_num_in, pin_num_out);

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
    lcd.setFont(&fonts::AsciiFont8x16);

    print_exists_users();
}

void loop()
{
    M5.update();
    auto touch = M5.Touch.getDetail();
    Units.update();

    // Search,Match
    if (M5.BtnA.wasClicked() || touch.wasClicked()) {
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
    if (M5.BtnA.wasHold() || touch.wasHold()) {
        M5.Speaker.tone(4000, 20);
        register_finger();
        lcd.fillScreen(TFT_DARKGREEN);
    }
}

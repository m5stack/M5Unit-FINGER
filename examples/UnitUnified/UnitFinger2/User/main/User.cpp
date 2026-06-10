/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  User management example using M5UnitUnified for UnitFinger2
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <esp_random.h>
#include <algorithm>

using namespace m5::unit::finger2;

extern const uint8_t template_data[];    // template_data.cpp
extern const size_t template_data_size;  // template_data.cpp

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitFinger2 unit;

uint32_t cur_menu{};
uint16_t cur_user{0};

bool callback_batch(const uint16_t call_times, const uint16_t actual_size, const uint16_t batch_size,
                    const uint16_t total_size, const uint16_t planned_size, const bool completed)
{
    M5.Log.printf("    [%03u]:%3u/%3u,%4u/%4u:%s\n", call_times, actual_size, batch_size, total_size, planned_size,
                  completed ? "COMPLETED" : "CONTINUE");

    return true;  // Abort if false
}

void make_random_user()
{
    unit.wakeup();

    // Template to buffer
    if (!unit.writeTemplateAllBatches(template_data, template_data_size, 128, callback_batch)) {
        M5_LOGE("Failed to writeTemplateAllBatches");
        return;
    }

    for (uint_fast8_t i = 0; i < 10; ++i) {
        // Store
        uint16_t page = esp_random() % 100;
        if (!unit.storeTemplate(page)) {
            M5_LOGE("Failed to storeTemplate");
            continue;
        }
        M5.Log.printf("Register %u\n", page);
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

void show_menu()
{
    M5.Log.printf(
        "===MENU=== user:%u\n"
        "%c Change cur user\n"
        "%c Show template\n"
        "%c Show all users\n"
        "%c Delete user\n"
        "%c Delete all users\n",
        cur_user, cur_menu == 0 ? '*' : ' ', cur_menu == 1 ? '*' : ' ', cur_menu == 2 ? '*' : ' ',
        cur_menu == 3 ? '*' : ' ', cur_menu == 4 ? '*' : ' ');
}

// The function will not return until a selection is made
int select_menu()
{
    for (;;) {
        M5.update();
        Units.update();

        if (M5.BtnA.wasHold()) {
            cur_menu = (cur_menu + 1) % 5;
            show_menu();
        } else if (M5.BtnA.wasClicked()) {
            if (cur_menu == 0) {
                if (++cur_user > 99) {
                    cur_user = 0;
                }
                show_menu();
                continue;
            }
            return cur_menu;
        }
    }
}

bool select_yesno()
{
    for (;;) {
        M5.update();
        Units.update();
        if (M5.BtnA.wasHold()) {
            return false;
        }
        if (M5.BtnA.wasClicked()) {
            return true;
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
    lcd.fillScreen(TFT_DARKGREEN);

    // If the number of registered templates is 0, dummy data is registered
    uint16_t num{};
    if (unit.wakeup() && unit.readValidTemplates(num) && num == 0) {
        make_random_user();
    }
    print_exists_users();

    show_menu();
}

void loop()
{
    // update in select_menu()
    switch (select_menu()) {
        case 1: {
            if (unit.wakeup() && unit.loadTemplate(1, cur_user)) {
                std::vector<uint8_t> temp{};
                temp.resize(m5::unit::UnitFinger2::TEMPLATE_SIZE);
                uint16_t actual{};
                if (unit.readTemplateAllBatches(actual, temp.data(), temp.size(), 256, callback_batch)) {
                    m5::utility::log::dump(temp.data(), actual, false);
                } else {
                    M5_LOGE("Failed to readTemplateAllBatches %u", cur_user);
                }
            } else {
                M5_LOGE("Failed to loadTemplate %u", cur_user);
            }

        } break;
        case 2:
            print_exists_users();
            break;
        case 3:
            if (unit.wakeup() && unit.deleteTemplate(cur_user)) {
                M5.Log.printf("==> User %u deleted\n", cur_user);
            } else {
                M5_LOGE("Failed to deleteTemplate %u", cur_user);
            }
            break;
        case 4:
            M5.Log.printf("==> Delete all user sure?\n");
            if (select_yesno()) {
                if (unit.wakeup() && unit.clear()) {
                    M5.Log.printf("==> All users deleted\n");
                    cur_user = 0;
                } else {
                    M5_LOGE("Failed to clear");
                }
            }
            break;
        default:
            break;
    }
    M5.Log.printf("\n");
    show_menu();
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

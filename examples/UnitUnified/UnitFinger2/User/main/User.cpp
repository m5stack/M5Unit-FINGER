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

constexpr char GUIDE[] = "Hold:Next Click:Select";

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
    lcd.setTextSize(std::max<int32_t>(1, lcd.width() / 320));
    lcd.setTextDatum(textdatum_t::bottom_left);
    lcd.drawString(guide, 0, lcd.height());
    lcd.endWrite();
}

char last_result[48]{};
constexpr const char* menu_items[] = {"Change user", "Show template", "Show all users", "Delete user",
                                      "Delete all users"};

// Menu with the selected item highlighted, the last result and the guide
void draw_menu()
{
    if (!lcd.width()) {
        return;  // No display
    }
    const int32_t sz     = std::max<int32_t>(1, lcd.width() / 320);
    const int32_t line_h = 16 * sz;

    char header[32]{};
    snprintf(header, sizeof(header), "Current user:%u", cur_user);

    lcd.startWrite();
    lcd.fillScreen(TFT_DARKGREY);
    lcd.setFont(&fonts::AsciiFont8x16);
    lcd.setTextSize(sz);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
    lcd.drawString(header, 0, 0);
    for (uint32_t i = 0; i < m5::stl::size(menu_items); ++i) {
        const bool selected = (i == cur_menu);
        const int32_t y     = line_h * static_cast<int32_t>(i + 1);
        if (selected) {
            lcd.fillRect(0, y, lcd.width(), line_h, TFT_YELLOW);
        }
        lcd.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? TFT_YELLOW : TFT_DARKGREY);
        lcd.drawString(menu_items[i], 0, y);
    }
    lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
    lcd.setTextDatum(textdatum_t::bottom_left);
    lcd.drawString(last_result, 0, lcd.height() - line_h);
    lcd.drawString(GUIDE, 0, lcd.height());
    lcd.endWrite();
}

bool callback_batch(const uint16_t call_times, const uint16_t actual_size, const uint16_t batch_size,
                    const uint16_t total_size, const uint16_t planned_size, const bool completed)
{
    M5.Log.printf("    [%03u]:%3u/%3u,%4u/%4u:%s\n", call_times, actual_size, batch_size, total_size, planned_size,
                  completed ? "COMPLETED" : "CONTINUE");

    return true;  // Abort if false
}

void make_random_user()
{
    if (!unit.wakeup()) {
        M5_LOGE("Failed to wakeup");
        return;
    }

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
    draw_menu();
}

// The function will not return until a selection is made
int select_menu()
{
    for (;;) {
        m5::utility::delay(1);
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
        m5::utility::delay(1);
        M5.update();
        Units.update();
        // Hold to confirm, so that a click by mistake does not delete
        if (M5.BtnA.wasHold()) {
            return true;
        }
        if (M5.BtnA.wasClicked()) {
            return false;
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
                if (unit.readTemplateAllBatches(actual, temp.data(), temp.size(), 256, callback_batch) &&
                    actual == temp.size()) {
                    m5::utility::log::dump(temp.data(), actual, false);
                    snprintf(last_result, sizeof(last_result), "Template %u on serial", cur_user);
                } else {
                    M5_LOGE("Failed to readTemplateAllBatches %u (%u/%u)", cur_user, actual,
                            static_cast<unsigned>(temp.size()));
                }
            } else {
                M5_LOGE("Failed to loadTemplate %u", cur_user);
                snprintf(last_result, sizeof(last_result), "No template:%u", cur_user);
            }

        } break;
        case 2:
            print_exists_users();
            snprintf(last_result, sizeof(last_result), "Listed on serial");
            break;
        case 3:
            if (unit.wakeup() && unit.deleteTemplate(cur_user)) {
                M5.Log.printf("==> User %u deleted\n", cur_user);
                snprintf(last_result, sizeof(last_result), "Deleted user:%u", cur_user);
            } else {
                M5_LOGE("Failed to deleteTemplate %u", cur_user);
                snprintf(last_result, sizeof(last_result), "Failed to delete:%u", cur_user);
            }
            break;
        case 4:
            M5.Log.printf("==> Delete all users? (Hold:Yes Click:No)\n");
            draw_status(State::Failure, "Delete all?", "Hold:Yes Click:No");
            if (select_yesno()) {
                if (unit.wakeup() && unit.clear()) {
                    M5.Log.printf("==> All users deleted\n");
                    snprintf(last_result, sizeof(last_result), "All users deleted");
                    cur_user = 0;
                } else {
                    M5_LOGE("Failed to clear");
                    snprintf(last_result, sizeof(last_result), "Failed to delete all");
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

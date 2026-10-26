/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  User management example using M5UnitUnified for UnitFinger
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <wiring/m5_unit_finger_wiring.hpp>
#include <esp_random.h>
#include <algorithm>
#include <cstring>
#include <cstdio>

using namespace m5::unit::fpc1xxx;

#if !defined(USING_UNIT_FINGER) && !defined(USING_HAT_FINGER) && !defined(USING_FACES_FINGER)
// For UnitFinger (SKU:U008)
// #define USING_UNIT_FINGER
// For HatFinger (SKU:U074)
// #define USING_HAT_FINGER
// For FacesFinger (SKU:A066)
// #define USING_FACES_FINGER
#endif

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

uint32_t cur_menu{};
uint16_t cur_user{1};

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

char last_result[48]{};
constexpr const char* menu_items[] = {"Change user", "Show user", "Show all users", "Delete user", "Delete all users"};

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
    lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
    draw_small(header, 0, 0, false);
    lcd.setFont(&fonts::AsciiFont8x16);
    lcd.setTextSize(sz);
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
    const int32_t guide_h = draw_small(GUIDE, 0, lcd.height(), true);
    draw_small(last_result, 0, lcd.height() - guide_h, true);
    lcd.endWrite();
}

void make_random_user()
{
    for (uint_fast8_t i = 1; i <= 10; ++i) {
        uint8_t perm = esp_random() % 3 + 1;
        std::array<uint8_t, 193> characteristic{};
        std::generate(characteristic.begin(), characteristic.end(),
                      []() { return static_cast<uint8_t>(esp_random()); });
        if (!unit.registerCharacteristic(i, perm, characteristic.data())) {
            M5_LOGE("Failed to register %u", i);
        }
    }
}

void print_all_users()
{
    std::vector<User> v{};
    if (unit.readAllUser(v)) {
        if (v.empty()) {
            M5.Log.printf("No registered users\n");
            return;
        }
        M5.Log.printf("All user data (%u):\n", v.size());
        uint16_t idx{};
        for (auto&& u : v) {
            M5.Log.printf("  [%3d] ID:%5u, PERMISSION:%u\n", idx++, u.id, u.permission);
        }
    }
}

void show_menu()
{
    M5.Log.printf(
        "===MENU=== user:%u\n"
        "%c Change cur user\n"
        "%c Show user\n"
        "%c Show all users\n"
        "%c Delete user\n"
        "%c Delete all users\n",
        cur_user, cur_menu == 0 ? '*' : ' ', cur_menu == 1 ? '*' : ' ', cur_menu == 2 ? '*' : ' ',
        cur_menu == 3 ? '*' : ' ', cur_menu == 4 ? '*' : ' ');
    draw_menu();
}

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
                if (++cur_user > 150) {
                    cur_user = 1;
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
        if (!unit.readRegistrationMode(mode) || !unit.readComparisonLevel(clv) || !unit.readTimeout(timeout) ||
            !unit.readRegisteredUserCount(user_count)) {
            M5_LOGW("Failed to read settings");
        }

        M5.Log.printf("=== %s information ===\n", unit.deviceName());
        M5.Log.printf("           Mode: %s\n",
                      mode == Mode::ProhibitDuplicate ? "Prohibit duplicate" : "Allow duplicate");
        M5.Log.printf("  Comparison Lv: %u\n", clv);
        M5.Log.printf("        Timeout: %u\n", timeout);
        M5.Log.printf("Registered user: %u\n", user_count);
    }

    // If the number of registered users is 0, dummy data is registered
    uint16_t users{};
    if (unit.readRegisteredUserCount(users) && users == 0) {
        make_random_user();
    }
    show_menu();
}

void loop()
{
    M5.update();
    Units.update();

    switch (select_menu()) {
        case 1: {
            uint8_t perm{};
            uint8_t characteristic[193]{};
            if (unit.readUser(perm, cur_user) && unit.readUserCharacteristic(characteristic, cur_user)) {
                M5.Log.printf("User:%u Permission:%u\n", cur_user, perm);
                snprintf(last_result, sizeof(last_result), "User:%u permission:%u", cur_user, perm);
                m5::utility::log::dump(characteristic, 193, false);
            } else {
                M5_LOGE("Failed to read user %u", cur_user);
                snprintf(last_result, sizeof(last_result), "No user:%u", cur_user);
            }

        } break;
        case 2:
            print_all_users();
            snprintf(last_result, sizeof(last_result), "Listed on serial");
            break;
        case 3:
            if (unit.deleteUser(cur_user)) {
                M5.Log.printf("==> User %u deleted\n", cur_user);
                snprintf(last_result, sizeof(last_result), "Deleted user:%u", cur_user);
            } else {
                M5_LOGE("Failed to delete user %u", cur_user);
                snprintf(last_result, sizeof(last_result), "Failed to delete:%u", cur_user);
            }
            break;
        case 4:
            M5.Log.printf("==> Delete all users? (Hold:Yes Click:No)\n");
            draw_status(State::Failure, "Delete all?", "Hold:Yes Click:No");
            if (select_yesno()) {
                if (unit.deleteAllUsers()) {
                    M5.Log.printf("==> All users deleted\n");
                    snprintf(last_result, sizeof(last_result), "All users deleted");
                    cur_user = 1;
                } else {
                    M5_LOGE("Failed to delete all users");
                    snprintf(last_result, sizeof(last_result), "Failed to delete all");
                }
            } else {
                snprintf(last_result, sizeof(last_result), "Cancelled");
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

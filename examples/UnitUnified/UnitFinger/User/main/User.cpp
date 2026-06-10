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
}

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
        unit.readRegistrationMode(mode);
        unit.readComparisonLevel(clv);
        unit.readTimeout(timeout);
        unit.readRegisteredUserCount(user_count);

        M5.Log.printf("=== %s information ===\n", unit.deviceName());
        M5.Log.printf("           Mode: %s\n",
                      mode == Mode::ProhibitDuplicate ? "Prohibit duplicate" : "Allow duplicate");
        M5.Log.printf("  Comparison Lv: %u\n", clv);
        M5.Log.printf("        Timeout: %u\n", timeout);
        M5.Log.printf("Registered user: %u\n", user_count);
    }
    lcd.fillScreen(TFT_DARKGREEN);

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
                m5::utility::log::dump(characteristic, 193, false);
            } else {
                M5_LOGE("Failed to read user %u", cur_user);
            }

        } break;
        case 2:
            print_all_users();
            break;
        case 3:
            if (unit.deleteUser(cur_user)) {
                M5.Log.printf("==> User %u deleted\n", cur_user);
            } else {
                M5_LOGE("Failed to delete user %u", cur_user);
            }
            break;
        case 4:
            M5.Log.printf("==> Delete all user sure?\n");
            if (select_yesno()) {
                if (unit.deleteAllUsers()) {
                    M5.Log.printf("==> All users deleted\n");
                    cur_user = 1;
                } else {
                    M5_LOGE("Failed to delete all users");
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

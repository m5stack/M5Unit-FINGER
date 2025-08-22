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
#include <esp_random.h>
#include <algorithm>

using namespace m5::unit::fpc1xxx;

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitFinger unit;

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
        auto touch = M5.Touch.getDetail();

        if (M5.BtnA.wasHold() || touch.wasHold()) {
            cur_menu = (cur_menu + 1) % 5;
            show_menu();
        } else if (M5.BtnA.wasClicked() || touch.wasClicked()) {
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
        auto touch = M5.Touch.getDetail();
        if (M5.BtnA.wasHold() || touch.wasHold()) {
            return false;
        }
        if (M5.BtnA.wasClicked() || touch.wasClicked()) {
            return true;
        }
    }
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

#if defined(CONFIG_IDF_TARGET_ESP32C6)
    auto& s = Serial1;
#elif SOC_UART_NUM > 2
    auto& s = Serial2;
#elif SOC_UART_NUM > 1
    auto& s = Serial1;
#else
#error "Not enough Serial"
#endif
    s.end();
    s.begin(19200, SERIAL_8N1, pin_num_in, pin_num_out);

    if (!Units.add(unit, s) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.clear(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    M5_LOGI("M5UnitUnified has been begun");
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
        M5.Log.printf("           Mode: %s\n", mode == Mode::DenyDuplicate ? "Deny duplicate" : "Allow duplicate");
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
                    M5.Log.printf("==> All users deleted\n", cur_user);
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

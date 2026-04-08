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

#if !defined(USING_UNIT_FINGER) && !defined(USING_HAT_FINGER) && !defined(USING_FACES_FINGER)
// For UnitFinger (U008)
// #define USING_UNIT_FINGER
// For HatFinger (U074)
// #define USING_HAT_FINGER
// For FacesFinger (Faces Finger Module)
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

#if defined(USING_HAT_FINGER)
struct UartPins {
    int rx;
    int tx;
};

UartPins get_hat_uart_pins(const m5::board_t board)
{
    switch (board) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
            return {26, 0};
        case m5::board_t::board_M5StickS3:
            return {0, 8};
        case m5::board_t::board_M5StackCoreInk:
            return {26, 25};
        default:
            return {-1, -1};
    }
}
#endif

#if defined(USING_FACES_FINGER)
// M-Bus pins for Faces Finger (GPIO varies by board)
struct FacesPins {
    int rx;             // UART RX (mbus_pin15)
    int tx;             // UART TX (mbus_pin16)
    int panel_power;    // Panel power (mbus_pin10)
    int touch_power;    // Touch IC power (mbus_pin20)
};

FacesPins get_faces_pins()
{
    return {
        M5.getPin(m5::pin_name_t::mbus_pin15),   // RX
        M5.getPin(m5::pin_name_t::mbus_pin16),   // TX
        M5.getPin(m5::pin_name_t::mbus_pin10),   // Panel power (GPIO26 on Core)
        M5.getPin(m5::pin_name_t::mbus_pin20),   // Touch IC power (GPIO5 on Core)
    };
}
#endif

}  // namespace

void setup()
{
    auto m5cfg = M5.config();
#if defined(USING_HAT_FINGER)
    m5cfg.pmic_button  = false;
    m5cfg.internal_imu = false;
    m5cfg.internal_rtc = false;
#endif
    M5.begin(m5cfg);
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

#if defined(USING_HAT_FINGER)
    const auto pins = get_hat_uart_pins(M5.getBoard());
    M5_LOGI("getHatPin: RX:%d TX:%d", pins.rx, pins.tx);
    if (pins.rx < 0 || pins.tx < 0) {
        M5_LOGE("No Hat port on this board");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
    auto pin_num_in  = pins.rx;
    auto pin_num_out = pins.tx;
#elif defined(USING_FACES_FINGER)
    const auto fp = get_faces_pins();
    M5_LOGI("getFacesPin: RX:%d TX:%d PWR:%d TCH:%d", fp.rx, fp.tx, fp.panel_power, fp.touch_power);
    if (fp.rx < 0 || fp.tx < 0) {
        M5_LOGE("No M-Bus on this board");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
    auto pin_num_in  = fp.rx;
    auto pin_num_out = fp.tx;

    // Set Faces config from M-Bus pins
    {
        auto cfg            = unit.config();
        cfg.panel_power_pin = fp.panel_power;
        cfg.touch_power_pin = fp.touch_power;
        unit.config(cfg);
    }
#else
    auto pin_num_in  = M5.getPin(m5::pin_name_t::port_c_rxd);
    auto pin_num_out = M5.getPin(m5::pin_name_t::port_c_txd);
    if (pin_num_in < 0 || pin_num_out < 0) {
        M5_LOGW("PortC is not available");
        // NanoC6: Ex_I2C.setPort() registers m5gfx::i2c on GROVE pins;
        // Wire.end() alone won't release it, causing dual-driver conflict on uart_driver_install
        if (M5.getBoard() == m5::board_t::board_M5NanoC6) {
            M5.Ex_I2C.release();
        }
        Wire.end();
        pin_num_in  = M5.getPin(m5::pin_name_t::port_a_pin1);
        pin_num_out = M5.getPin(m5::pin_name_t::port_a_pin2);
    }
#endif
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
    s.end();
    s.begin(19200, SERIAL_8N1, pin_num_in, pin_num_out);

    if (!Units.add(unit, s) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

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

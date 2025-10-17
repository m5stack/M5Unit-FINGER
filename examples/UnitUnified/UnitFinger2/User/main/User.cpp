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
#include <esp_random.h>
#include <algorithm>

using namespace m5::unit::fpc1xxx;

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
        auto touch = M5.Touch.getDetail();

        if (M5.BtnA.wasHold() || touch.wasHold()) {
            cur_menu = (cur_menu + 1) % 5;
            show_menu();
        } else if (M5.BtnA.wasClicked() || touch.wasClicked()) {
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
                    cur_user = 1;
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

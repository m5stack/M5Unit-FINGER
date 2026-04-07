/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  autoEnroll/Identify example using M5UnitUnified for UnitFinger2
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
auto_enroll_flag_t enroll_flags = auto_enroll_flag::ALLOW_OVERWRITE_PAGE;

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

bool callback_enroll(const uint16_t call_times, const uint16_t page_id, const ConfirmCode confirm,
                     const AutoEnrollStage stage, const uint8_t state)
{
    lcd.startWrite();
    switch (stage) {
        case AutoEnrollStage::GetImage:
            lcd.fillScreen(TFT_DARKGREEN);
            break;
        case AutoEnrollStage::GenerateCharacteristic: {
            M5.Speaker.tone(3000, 20);
            lcd.fillScreen(TFT_BLUE);
            lcd.drawString("Please release finger", 0, 16);
            M5.Log.printf("Please release finger(%u)\n", state);
        } break;
        case AutoEnrollStage::ReleasedFinger:
            lcd.fillScreen(TFT_ORANGE);
            lcd.drawString("Please touch finger", 0, 16);
            M5.Log.printf("Please touch finger(%u)\n", state);
            break;
        case AutoEnrollStage::MergeTemplate:
            lcd.fillScreen(TFT_DARKGREEN);
            break;
        default:
            break;
    }
    lcd.setCursor(0, 0);
    lcd.printf("ENROLL:%02u %02u [%02X] (%02X)", call_times, (uint8_t)stage, (uint8_t)confirm, state);
    M5.Log.printf("  Enroll[%02u]:%02u [%02X] (%02X)\n", call_times, (uint8_t)stage, (uint8_t)confirm, stage);
    lcd.endWrite();

    return true;  // Abort if false
}

void auto_enroll()
{
    static bool highlow{};

    if (unit.wakeup()) {
        ConfirmCode confirm{};
        // Available lowest/highest page_id
        uint16_t page{0xFFFF};
        if (!(highlow == false ? unit.findLowestAvailablePage(page) : unit.findHighestAvailablePage(page)) ||
            page == 0xFFFF) {
            M5_LOGE("Failed or Invalid page");
            return;
        }

        M5.Log.printf("Try register to %u\n", page);
        if (unit.autoEnroll(confirm, page, 5 /* capture times */, enroll_flags, callback_enroll)) {
            highlow = !highlow;
            M5.Log.printf("Registered: %u\n", page);
        } else {
            M5_LOGE("Failed to autoEnroll %02X, %u", confirm, page);
            switch (confirm) {
                case ConfirmCode::TemplateNotEmpty:
                    M5_LOGE("The specified page %u already has a fingerprint registered", page);
                    break;
                case ConfirmCode::AlreadyExists:
                    M5_LOGE("The same fingerprint template can not registered");
                    break;
                case ConfirmCode::OperationBlocked:
                    M5_LOGE("Abort in callback");
                    break;

                default:
                    break;
            }
        }
        lcd.fillScreen(TFT_DARKGREEN);
    } else {
        M5_LOGE("Failed to wakeup");
    }
}

bool callback_identify(const uint16_t call_times, const ConfirmCode confirm, const AutoIdentifyStage stage)
{
    lcd.startWrite();
    lcd.setCursor(0, 0);
    lcd.printf("IDENTIFY:%02u [%02X]", (uint8_t)stage, (uint8_t)confirm);
    lcd.endWrite();
    M5.Log.printf("  Identify[%02u]:%02u [%02X]\n", call_times, (uint8_t)stage, (uint8_t)confirm);
    return true;  // Abort if false
}

void auto_identify()
{
    bool matched{};
    uint16_t page{};
    uint16_t score{};

    if (unit.wakeup()) {
        // 1:N
        if (unit.autoIdentify(matched, page, score, 0xFFFF, 0, 0x0000, callback_identify)) {
            M5.Log.printf("<1:N> %s: Page:%u: Score:%u\n", matched ? "Found" : "Not found", page, score);
            if (matched) {
                // 1:1
                if (unit.autoIdentify(matched, page, score, page, 0, 0x0000, callback_identify)) {
                    M5.Log.printf("<1:1> %s: Page:%u: Score:%u\n", matched ? "Match" : "Unmatched", page, score);
                } else {
                    M5_LOGE("Failed to autoIdentify(1:1)");
                }
            }
        } else {
            M5_LOGE("Failed to autoIdentify(1:N)");
        }
    } else {
        M5_LOGE("Failed to wakeup");
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
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
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

    // Identify
    if (M5.BtnA.wasClicked()) {
        M5.Speaker.tone(2000, 20);
        auto_identify();
    }

    // Register
    if (M5.BtnA.wasHold()) {
        M5.Speaker.tone(4000, 20);
        auto_enroll();
    }
}

/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Capture finger and show image example using M5UnitUnified for UnitFinger2
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>

using namespace m5::unit::finger2;

namespace {
auto& lcd = M5.Display;
LGFX_Sprite sprite4;

m5::unit::UnitUnified Units;
m5::unit::UnitFinger2 unit;

void make_sprite4(LGFX_Sprite& s, const std::vector<uint8_t>& v, const uint16_t wid, const uint16_t hgt)
{
    // Make 4bit grayscale sprite image
    for (int y = 0; y < hgt; ++y) {
        for (int x = 0; x < wid; x += 2) {
            uint8_t nibbles = v[y * (wid >> 1) + x / 2];
            s.writePixel(x, y, ((nibbles >> 4) & 0x0F));
            s.writePixel(x + 1, y, (nibbles & 0x0F));
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

    if (lcd.width() && lcd.height()) {
        sprite4.setPsram(false);
        sprite4.setColorDepth(lgfx::color_depth_t::grayscale_4bit);
        sprite4.createSprite(m5::unit::UnitFinger2::IMAGE_WIDTH, m5::unit::UnitFinger2::IMAGE_HEIGHT);
        sprite4.createPalette();
    }
}

void loop()
{
    M5.update();
    Units.update();

    if (M5.BtnA.wasClicked()) {
        lcd.fillScreen(TFT_DARKGREEN);
        M5.Speaker.tone(2500, 20);
        lcd.setCursor(0, 0);
        lcd.printf("Try capture     ");
        M5.Log.printf("Try capture\n");

        std::vector<uint8_t> v{};
        if (unit.wakeup()) {
            bool detected{};
            if (unit.capture(detected)) {
                uint8_t percentage{};
                bool quality{};
                if (unit.readImageInformation(percentage, quality)) {
                    M5.Log.printf("ImageInfo:%u%% quality:%u\n", percentage, quality);
                }
                if (detected) {
                    if (unit.readImage(v)) {
                        M5.Speaker.tone(4000, 20);
                        M5.Log.printf("Successful\n");
                        if (lcd.width() && lcd.height()) {
                            make_sprite4(sprite4, v, unit.imageWidth(), unit.imageHeight());
                            sprite4.pushSprite(&lcd, 0, 0);
                        } else {
                            M5.Log.printf("Image data %u x %u\n", unit.imageWidth(), unit.imageHeight());
                            m5::utility::log::dump(v.data(), v.size(), false);
                        }
                        return;

                    } else {
                        M5_LOGE("Failed to readImage");
                    }
                } else {
                    M5_LOGE("Finger not detected");
                }
            } else {
                M5_LOGE("Failed to capture");
            }
        } else {
            M5_LOGE("Failed to wakeup");
        }
        M5.Speaker.tone(1000, 20);
        lcd.setCursor(0, 0);
        lcd.printf("Try again       ");
    }
}

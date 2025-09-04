/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Capture finger example using M5UnitUnified for UnitFinger
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_FINGER) && !defined(USING_HAT_FINGER)
// #define USING_UNIT_FINGER
// #define USING_HAT_FINGER
#endif
// *************************************************************

using namespace m5::unit::fpc1xxx;

namespace {
auto& lcd = M5.Display;
LGFX_Sprite sprite4, sprite8;

m5::unit::UnitUnified Units;
#if defined(USING_UNIT_FINGER)
m5::unit::UnitFinger unit;
#elif defined(USING_HAT_FINGER)
m5::unit::HatFinger unit;
#else
#error Please choose unit!
#endif

void make_sprite8(LGFX_Sprite& s, const std::vector<uint8_t>& v, const uint16_t wid, const uint16_t hgt)
{
    // Make 8bit grayscale sprite image
    for (int y = 0; y < hgt; ++y) {
        for (int x = 0; x < wid; x += 2) {
            s.writePixel(x, y, v[y * wid + x]);
        }
    }
}

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

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

#if defined(USING_HAT_FINGER)
    auto pin_num_in  = 26;
    auto pin_num_out = 0;
#else
    auto pin_num_in  = M5.getPin(m5::pin_name_t::port_c_rxd);
    auto pin_num_out = M5.getPin(m5::pin_name_t::port_c_txd);
#endif
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

    //
    if (lcd.width() && lcd.height()) {
        sprite4.setPsram(false);
        sprite4.setColorDepth(lgfx::color_depth_t::grayscale_4bit);
        sprite4.createSprite(m5::unit::UnitFinger::RESOLUTION_WIDTH >> 1, m5::unit::UnitFinger::RESOLUTION_HEIGHT >> 1);
        sprite4.createPalette();

        sprite8.setPsram(false);
        sprite8.setColorDepth(lgfx::color_depth_t::grayscale_8bit);
        sprite8.createSprite(m5::unit::UnitFinger::RESOLUTION_WIDTH, m5::unit::UnitFinger::RESOLUTION_HEIGHT);
        sprite8.createPalette();
    }
    lcd.fillScreen(TFT_DARKGREEN);
}

void loop()
{
    static bool raw{};

    M5.update();
    auto touch = M5.Touch.getDetail();
    Units.update();

    if (M5.BtnA.wasClicked() || touch.wasClicked()) {
        lcd.fillScreen(TFT_DARKGREEN);
        M5.Speaker.tone(1500, 20);

        lcd.setCursor(0, 0);
        lcd.printf("Try capture %s", raw ? "RAW" : "COMPRESSED");
        M5.Log.printf("Try capture %s\n", raw ? "RAW" : "COMPRESSED");

        std::vector<uint8_t> v{};
        // NOTE: It takes a lot of time if raw is true
        if (unit.captureImage(v, raw)) {
            if (lcd.width() && lcd.height()) {
                if (raw) {
                    make_sprite8(sprite8, v, unit.width(), unit.height());
                    sprite8.pushSprite(&lcd, 0, 0);
                } else {
                    make_sprite4(sprite4, v, unit.width() >> 1, unit.height() >> 1);
                    sprite4.pushSprite(&lcd, 0, 0);
                }
            } else {
                M5.Log.printf("Captured data\n");
                m5::utility::log::dump(v.data(), v.size(), false);
            }
            raw = !raw;
        } else {
            M5_LOGE("Failed to capture");
            lcd.drawString("Try again       ", 0, 0);
            return;
        }
    }
}

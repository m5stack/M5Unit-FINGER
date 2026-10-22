/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Simple example using M5UnitUnified for UnitFinger
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedFINGER.h>
#include <M5Utility.hpp>
#include <wiring/m5_unit_unified_wiring.hpp>
#include <wiring/m5_unit_finger_wiring.hpp>

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_FINGER) && !defined(USING_HAT_FINGER) && !defined(USING_FACES_FINGER)
// For UnitFinger (SKU:U008)
// #define USING_UNIT_FINGER
// For HatFinger (SKU:U074)
// #define USING_HAT_FINGER
// For FacesFinger (SKU:A066)
// #define USING_FACES_FINGER
#endif
// *************************************************************

using namespace m5::unit::fpc1xxx;

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

uint16_t target_user_id{};
uint16_t user_count{};
char last_result[48]{};

// Screen state (the background color tells the state)
enum class State : uint8_t { Idle, Place, Busy, Success, Failure };

struct StateStyle {
    uint16_t bg;
    uint16_t fg;
};
constexpr StateStyle state_style[] = {
    {TFT_DARKGREY, TFT_WHITE},   // Idle
    {TFT_YELLOW, TFT_BLACK},     // Place
    {TFT_ORANGE, TFT_BLACK},     // Busy
    {TFT_DARKGREEN, TFT_WHITE},  // Success
    {TFT_RED, TFT_WHITE},        // Failure
};

// Header (unit / users), status (large) and guide / last result
void draw_screen(const State state, const char* status)
{
    if (!lcd.width()) {
        return;  // No display
    }
    const auto& st         = state_style[m5::stl::to_underlying(state)];
    const int32_t small_sz = std::max<int32_t>(1, lcd.width() / 320);
    const int32_t large_sz = std::max<int32_t>(1, lcd.width() / 160);
    const int32_t line_h   = 16 * small_sz;

    char header[48]{};
    snprintf(header, sizeof(header), "%s users:%u next:%u", unit.deviceName(), user_count, target_user_id);

    lcd.startWrite();
    lcd.fillScreen(st.bg);
    lcd.setFont(&fonts::AsciiFont8x16);
    lcd.setTextColor(st.fg, st.bg);

    lcd.setTextSize(small_sz);
    lcd.setTextDatum(textdatum_t::top_left);
    lcd.drawString(header, 0, 0);

    lcd.setTextSize(large_sz);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.drawString(status, lcd.width() >> 1, lcd.height() >> 1);

    lcd.setTextSize(small_sz);
    lcd.setTextDatum(textdatum_t::bottom_left);
    lcd.drawString("Hold:Register Click:Match", 0, lcd.height() - line_h);
    lcd.drawString(last_result, 0, lcd.height());
    lcd.endWrite();
}

void update_user_count()
{
    if (!unit.readRegisteredUserCount(user_count)) {
        M5_LOGW("Failed to read user count");
    }
}

void print_all_users()
{
    std::vector<User> v{};
    if (unit.readAllUser(v)) {
        M5.Log.printf("All user data (%u):\n", v.size());
        uint16_t idx{};
        for (auto&& u : v) {
            M5.Log.printf("  [%3d] ID:%5u, PERMISSION:%u\n", idx++, u.id, u.permission);
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
        print_all_users();
        if (unit.findAvailableUserID(target_user_id)) {
            M5.Log.printf("Lowest unregistered UserID: %u\n", target_user_id);
        }

        uint8_t characteristic[193]{};
        if (target_user_id > 1 && unit.readUserCharacteristic(characteristic, target_user_id - 1)) {
            M5.Log.printf("--- UserID:%u characteristic\n", target_user_id - 1);
            m5::utility::log::dump(characteristic, m5::stl::size(characteristic), false);
        }
    }
    draw_screen(State::Idle, "Place finger");
}

void loop()
{
    M5.update();
    Units.update();

    // Register finger (the finger must be on the sensor)
    if (M5.BtnA.wasHold()) {
        M5.Speaker.tone(2500, 20);
        draw_screen(State::Place, "Keep finger on");
        M5.Log.printf("Try register\n");

        const auto id = target_user_id;
        if (unit.registerFinger(id, 1)) {
            M5.Speaker.tone(2000, 20);
            M5.Log.printf("Registered user %u\n", id);
            snprintf(last_result, sizeof(last_result), "Registered ID:%u", id);
            if (!unit.findAvailableUserID(target_user_id)) {
                M5_LOGW("No available user ID");
            }
            update_user_count();
            draw_screen(State::Success, "Registered");
        } else {
            M5.Speaker.tone(800, 40);
            M5.Log.printf("Failed to register %u\n", id);
            snprintf(last_result, sizeof(last_result), "Register failed ID:%u", id);
            draw_screen(State::Failure, "Try again");
        }
        return;
    }

    // Identify and verify finger
    if (M5.BtnA.wasClicked()) {
        uint16_t id{};
        uint8_t per{};
        M5.Speaker.tone(1500, 20);
        draw_screen(State::Busy, "Identifying");
        M5.Log.printf("Try identify\n");

        if (unit.identifyFinger(id, per) && id) {
            M5.Log.printf("Identified user:%u, permission:%u\n", id, per);
            // If it was identified, it should pass verification
            M5.Log.printf("Try verify\n");
            bool match{};
            if (unit.verifyFinger(match, id) && match) {
                M5.Speaker.tone(2500, 20);
                M5.Log.printf("Verified: %u\n", id);
                snprintf(last_result, sizeof(last_result), "Matched ID:%u", id);
                draw_screen(State::Success, "Matched");
            } else {
                M5.Speaker.tone(800, 40);
                M5.Log.printf("Failed or not match %u\n", id);
                snprintf(last_result, sizeof(last_result), "Verify failed ID:%u", id);
                draw_screen(State::Failure, "Not verified");
            }
        } else {
            M5.Speaker.tone(800, 40);
            M5.Log.printf("No user\n");
            snprintf(last_result, sizeof(last_result), "No matching user");
            draw_screen(State::Failure, "No user");
        }
    }
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

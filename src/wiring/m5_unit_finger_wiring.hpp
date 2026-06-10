/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file m5_unit_finger_wiring.hpp
  @brief Header-only board-aware wiring helpers specific to M5Unit-FINGER (FacesFinger / M-Bus)
  @note Include this LAST. Requires M5Unified.h to be included beforehand
        (detected via __M5UNIFIED_HPP__). Mirrors the pattern of
        m5_unit_unified_wiring.hpp from M5UnitUnified for FacesFinger M-Bus
        connection, which is out of scope for the generic wiring helpers
        (no board-aware M-Bus variant there).
*/
#ifndef M5_UNIT_FINGER_WIRING_HPP
#define M5_UNIT_FINGER_WIRING_HPP

#include "../unit/unit_FacesFinger.hpp"
#include <M5UnitUnified.hpp>
#include <wiring/m5_unit_unified_wiring.hpp>  // for m5::unit::wiring::defaultUartSerial()

#if defined(ARDUINO)
#include <HardwareSerial.h>
#endif

namespace m5 {
namespace unit {
namespace fpc1xxx {
namespace faces {
namespace wiring {

#if defined(__M5UNIFIED_HPP__)
/*!
  @struct FacesPins
  @brief M-Bus pin assignments for FacesFinger (UART + panel/touch power)
*/
struct FacesPins {
    int rx;           //!< UART RX (mbus_pin15)
    int tx;           //!< UART TX (mbus_pin16)
    int panel_power;  //!< Panel power switch (mbus_pin10)
    int touch_power;  //!< Touch IC power switch (mbus_pin20)
};

/*!
  @brief Board-aware lookup for FacesFinger M-Bus pin assignments
  @return FacesPins for the current board; {-1, -1, -1, -1} for unsupported boards
*/
inline FacesPins getFacesPins()
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5Stack:  // Core / Gray / Fire (M-Bus)
            return {16, 17, 26, 5};
        default:
            return {-1, -1, -1, -1};
    }
}

#if defined(ARDUINO)
/*!
  @brief Add a UnitFacesFinger on the board's M-Bus UART header
  @param units UnitUnified manager
  @param unit UnitFacesFinger instance (its config()'s panel_power_pin / touch_power_pin are set here)
  @param baud Baud rate (default 19200 = FPC1020A default)
  @param config Serial config (e.g. SERIAL_8N1)
  @return True if successful; false on unsupported board (Serial / unit untouched)
  @note FacesFinger is M-Bus only (M5Stack Core / Gray / Fire). The panel and touch IC power pins are
        applied to the unit's config before begin() so that UnitFacesFinger::begin() drives them.
        The HardwareSerial is selected via m5::unit::wiring::defaultUartSerial() (mirrors addUART).
*/
inline bool addFacesUART(UnitUnified& units, UnitFacesFinger& unit, const uint32_t baud = 19200,
                         const uint32_t config = SERIAL_8N1)
{
    const auto fp = getFacesPins();
    if (fp.rx < 0 || fp.tx < 0) {
        M5_LIB_LOGE("wiring: addFacesUART unsupported board=0x%02x", static_cast<int>(M5.getBoard()));
        return false;
    }
    M5_LIB_LOGI("wiring: addFacesUART board=0x%02x rx=%d tx=%d pwr=%d tch=%d baud=%lu", static_cast<int>(M5.getBoard()),
                fp.rx, fp.tx, fp.panel_power, fp.touch_power, (unsigned long)baud);

    auto cfg            = unit.config();
    cfg.panel_power_pin = fp.panel_power;
    cfg.touch_power_pin = fp.touch_power;
    unit.config(cfg);

    HardwareSerial& serial = m5::unit::wiring::defaultUartSerial();
    serial.end();
    serial.begin(baud, config, fp.rx, fp.tx);
    return units.add(unit, serial);
}
#else   // ESP-IDF native
/*!
  @brief Add a UnitFacesFinger on the board's M-Bus UART header (ESP-IDF native)
  @param units UnitUnified manager
  @param unit UnitFacesFinger instance (its config()'s panel_power_pin / touch_power_pin are set here)
  @param baud Baud rate (default 19200 = FPC1020A default)
  @param config UartConfig (default `UartConfig::Default` = 8N1)
  @return True if successful; false on unsupported board / UART install failure
  @note FacesFinger is M-Bus only (M5Stack Core / Gray / Fire). The panel and touch IC power pins are
        applied to the unit's config before begin() so that UnitFacesFinger::begin() drives them (via
        the ESP-IDF native driver/gpio API). The UART port is installed via the M5UnitUnified native
        helper, mirroring m5::unit::wiring::addUART.
*/
inline bool addFacesUART(UnitUnified& units, UnitFacesFinger& unit, const uint32_t baud = 19200,
                         const m5::unit::wiring::UartConfig config = m5::unit::wiring::UartConfig::Default)
{
    const auto fp = getFacesPins();
    if (fp.rx < 0 || fp.tx < 0) {
        M5_LIB_LOGE("wiring: addFacesUART unsupported board=0x%02x", static_cast<int>(M5.getBoard()));
        return false;
    }
    M5_LIB_LOGI("wiring(ESP-IDF): addFacesUART board=0x%02x rx=%d tx=%d pwr=%d tch=%d baud=%u",
                static_cast<int>(M5.getBoard()), fp.rx, fp.tx, fp.panel_power, fp.touch_power, (unsigned)baud);

    auto cfg            = unit.config();
    cfg.panel_power_pin = fp.panel_power;
    cfg.touch_power_pin = fp.touch_power;
    unit.config(cfg);

    auto port = m5::unit::wiring::uartPortHandle(m5::unit::wiring::defaultUartPort(), static_cast<gpio_num_t>(fp.rx),
                                                 static_cast<gpio_num_t>(fp.tx), baud, config);
    if (port == UART_NUM_MAX) {
        return false;
    }
    return units.add(unit, port);
}
#endif  // ARDUINO
#endif  // __M5UNIFIED_HPP__

}  // namespace wiring
}  // namespace faces
}  // namespace fpc1xxx
}  // namespace unit
}  // namespace m5
#endif  // M5_UNIT_FINGER_WIRING_HPP

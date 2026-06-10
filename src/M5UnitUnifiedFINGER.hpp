/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file M5UnitUnifiedFINGER.hpp
  @brief Main header of M5Unit-FINGER using M5UnitUnified

  @mainpage M5Unit-FINGER
  Library for UnitFINGER using M5UnitUnified.
*/
#ifndef M5_UNIT_UNIFIED_FINGER_HPP
#define M5_UNIT_UNIFIED_FINGER_HPP

#include "unit/unit_FPC1xxx.hpp"
#include "unit/unit_FacesFinger.hpp"
#include "unit/unit_Finger2.hpp"

/*!
  @namespace m5
  @brief Top level namespace of M5Stack
 */
namespace m5 {

/*!
  @namespace unit
  @brief Unit-related namespace
 */
namespace unit {

using UnitFinger = m5::unit::UnitFPC1020A;  //!< Alias for Unit Finger (FPC1020A, UART / GROVE)
using HatFinger  = m5::unit::UnitFPC1020A;  //!< Alias for Hat Finger (FPC1020A, UART / HAT)

}  // namespace unit
}  // namespace m5
#endif

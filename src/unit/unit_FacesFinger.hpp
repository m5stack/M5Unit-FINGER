/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_FacesFinger.hpp
  @brief Faces Finger Module (FPC1020A via M-Bus) for M5UnitUnified
 */
#ifndef M5_UNIT_FINGER_UNIT_FACES_FINGER_HPP
#define M5_UNIT_FINGER_UNIT_FACES_FINGER_HPP

#include "unit_FPC1xxx.hpp"

namespace m5 {
namespace unit {

/*!
  @class UnitFacesFinger
  @brief For Faces Finger Module (FPC1020A via M-Bus)
  @details Initializes panel power and touch IC power in begin()
 */
class UnitFacesFinger : public UnitFPC1020A {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitFacesFinger, 0x00);

public:
    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        uint32_t timeout_ms{1000 * 4};  //!< Serial I/O timeout (ms)
        int panel_power_pin{-1};        //!< Finger panel power pin (mbus_pin10)
        int touch_power_pin{-1};        //!< Touch IC power pin (mbus_pin20)
    };

    UnitFacesFinger() : UnitFPC1020A()
    {
    }
    virtual ~UnitFacesFinger() = default;

    //! @brief Begin communication with the unit
    virtual bool begin() override;

    ///@name Settings for begin
    ///@{
    /*! @brief Gets the configuration */
    inline config_t config()
    {
        return _faces_cfg;
    }
    //! @brief Set the configuration
    inline void config(const config_t& cfg)
    {
        _faces_cfg = cfg;
        UnitFPC1XXX::config_t base_cfg;
        base_cfg.timeout_ms = cfg.timeout_ms;
        UnitFPC1XXX::config(base_cfg);
    }
    ///@}

private:
    config_t _faces_cfg{};
};

}  // namespace unit
}  // namespace m5

#endif

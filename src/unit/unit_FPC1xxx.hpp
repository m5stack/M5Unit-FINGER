/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_FPC1xxx.hpp
  @brief FPC1xxx family with DSP unit for M5UnitUnified
 */
#ifndef M5_UNIT_FINGER_UNIT_FPC1XXX_HPP
#define M5_UNIT_FINGER_UNIT_FPC1XXX_HPP

#include <M5UnitComponent.hpp>
#include <array>
#include <vector>

namespace m5 {
namespace unit {
namespace fpc1xxx {

/*!
  @enum ACK
  @brief ACK code
 */
enum class ACK : uint8_t {
    Success,           //!< Succeed
    Fail,              //!< Failed
    Full = 0x04,       //!< Capacity full
    NoUser,            //!< No user
    Occupied,          //!< ID occupied
    ExistUser = 0x07,  //!< User exists
    Timeout,           //!< Timeout
};

/*!
  @enum BaudRate
  @brief Connection baud rate
 */
enum class BaudRate : uint8_t {
    Baud9600 = 1,  //!< 9600 baud
    Baud19200,     //!< 19200 baud (as default)
    Baud38400,     //!< 38400 baud
    Baud57600,     //!< 57600 baud
    Baud115200,    //!< 115200 baud
};

/*!
  @enum Mode
  @brief Registration mode
  @note Applies to fingerprint registration only
 */
enum class Mode : uint8_t {
    AllowDuplicate,     //!< Allow duplicate registrations
    ProhibitDuplicate,  //!< Characteristic duplication prohibited
};

/*!
  @struct User
  @brief User data
 */
struct User {
    User(const uint16_t i, const uint8_t p) : id{i}, permission{p}
    {
    }
    uint16_t id{};         //!< identifier
    uint8_t permission{};  //!< permission (Value can be 1, 2, or 3)
};

}  // namespace fpc1xxx

/*!
  @class UnitFPC1XXX
  @brief Base class for FPC1xxx family
  @note Family spec
  |Chip| Resolution | Unit |
  |---|---|---|
  |FPC1020| 192x192||
  |FPC1020AM| 192x192||
  |FPC1020AP| 192x192||
  |FPC1020A|160x160| Unit Finger (SKU:U008)|
  |FPC1021|160x160||
  |FPC1024|192x192||
  |FPC1025|160x160||
  |FPC1523|96x96||
*/
class UnitFPC1XXX : public Component {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitFPC1XXX, 0x00);

public:
    // The following variables must be defined in the derived class
    static constexpr uint16_t RESOLUTION_WIDTH{0};
    static constexpr uint16_t RESOLUTION_HEIGHT{0};
    static constexpr uint16_t MINIMUM_USER_ID{0};
    static constexpr uint16_t MAXIMUM_USER_ID{0};

protected:
    UnitFPC1XXX() : Component(DEFAULT_ADDRESS)
    {
    }

public:
    using Frame         = std::array<uint8_t, 8>;  //    0xF5 CMD P1 P2 P3 0 CHK 0xF5
    using VariableFrame = std::vector<uint8_t>;    //    0xF5 CMD LEN(MSB) LEN(LSB) 0 0 CHK 0xF5,...

    virtual ~UnitFPC1XXX() = default;

    //! @brief Begin communication with the unit
    virtual bool begin() override;

    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        uint32_t timeout_ms{1000 * 4};  //!< Serial I/O timeout (ms)
    };

    ///@name Settings for begin
    ///@{
    /*! @brief Gets the configuration */
    inline config_t config()
    {
        return _cfg;
    }
    //! @brief Set the configuration
    inline void config(const config_t& cfg)
    {
        _cfg = cfg;
    }
    ///@}

    ///@name Properties
    ///@{
    //! @brief  Gets the width of resolution
    inline virtual uint16_t resolutionWidth() const
    {
        return 0;
    }
    //! @brief  Gets the height of resolution
    inline virtual uint16_t resolutionHeight() const
    {
        return 0;
    }
    //! @brief Get the image width
    inline uint16_t imageWidth(const bool raw) const
    {
        return resolutionWidth() >> (raw ? 0 : 1);
    }
    //! @brief Get the resolution height
    inline uint16_t imageHeight(const bool raw) const
    {
        return resolutionHeight() >> (raw ? 0 : 1);
    }
    //! @brief Get the minimum user ID
    inline virtual uint16_t minimumUserID() const
    {
        return MINIMUM_USER_ID;
    }
    //! @brief Get the maximum user ID
    inline virtual uint16_t maximumUserID() const
    {
        return MAXIMUM_USER_ID;
    }
    ///@}

    ///@name Settings
    ///@{
    /*!
      @brief Read the registration mode
      @param[out] mode Mode
      @return True if successful
    */
    bool readRegistrationMode(fpc1xxx::Mode& mode);
    /*!
      @brief Write the registration mode
      @param mode Mode
      @return True if successful
      @note Applies to fingerprint registration only
    */
    bool writeRegistrationMode(const fpc1xxx::Mode mode);
    /*!
      @brief Read the comparison level
      @param[out] lv Comparison level (0-9)
      @return True if successful
      @note The higher the value, the stricter the comparison
     */
    bool readComparisonLevel(uint8_t& lv);
    /*!
      @brief Write the comparison level
      @param lv Comparison level (0-9)
      @return True if successful
      @note The higher the value, the stricter the comparison
      @note The settings are saved in the DSP and remain valid after reset
     */
    bool writeComparisonLevel(const uint8_t lv);
    /*!
      @brief Write the baud rate
      @param baud BaudRate
      @return True if successful
      @note After response, the communication speed is changed
      @note The settings are saved in the DSP and remain valid after reset
    */
    bool writeBaudRate(const fpc1xxx::BaudRate baud);
    /*!
      @brief Read the timeout
      @param[out] timeout Timeout value
      @return True if successful
     */
    bool readTimeout(uint8_t& timeout);
    /*!
      @brief Write the timeout
      @param timeout Timeout value
      @return True if successful
     */
    bool writeTimeout(const uint8_t timeout);
    ///@}

    ///@name User data
    ///@{
    /*!
      @brief Read the number of registered users
      @param[out] count Number of registered users
      @return True if successful
    */
    bool readRegisteredUserCount(uint16_t& count);
    /*!
      @brief Read the user permission
      @param[out] permission Permission(1,2,3)
      @param user_id user ID
      @return True if successful
     */
    bool readUser(uint8_t& permission, const uint16_t user_id);
    /*!
      @brief Get the all user data
      @param[out] v User data vector
      @return True if successful
    */
    bool readAllUser(std::vector<fpc1xxx::User>& v);
    /*!
      @brief Read the user characteristic data
      @param[out] characteristic User characteristic (At least 193 bytes)
      @param user_id User ID
      @return True if successful
     */
    bool readUserCharacteristic(uint8_t characteristic[193], const uint16_t user_id);
    /*!
      @brief Find the unregistered user id in specific range
      @param[out] user_id User ID
      @param low Lowest user ID (Minimum user ID if zero)
      @param high Highest user ID (Maximum user ID if zero)
      @return True if successful
     */
    bool findAvailableUserID(uint16_t& user_id, const uint16_t low = 0, const uint16_t high = 0);
    /*!
      @brief Delete user data
      @param user_id UserID
      @return True if successful
     */
    bool deleteUser(const uint16_t user_id);
    /*!
      @brief Delete all user data
      @return True if successful
     */
    bool deleteAllUsers();
    ///@}

    ///@name Finger
    ///@{
    /*!
      @brief Register finger
      @param user_id UserID
      @param permission Permission (1,2, or 3)
      @param step Number of intermediate step repetitions
      @return True if successful
      @warning Data considered identical will fail if the mode is Mode::ProhibitDuplicate
     */
    bool registerFinger(const uint16_t user_id, const uint8_t permission, const uint8_t step = 4);
    /*!
      @brief Verify specific user finger (1:1)
      @param[out] match Match if true
      @param user_id UserID
      @return True if successful
     */
    bool verifyFinger(bool& match, const uint16_t user_id);
    /*!
      @brief Identify finger (1:N)
      @param[out] user_id Matching UserID, 0 if NoUser
      @param[out] permission Matching user permission, 0 if NoUser
      @return True if successful
     */
    bool identifyFinger(uint16_t& user_id, uint8_t& permission);
    /*!
      @brief Scan characteristic
      @param[out] characteristic Characteristic data (At least 193 bytes)
     */
    bool scanCharacteristic(uint8_t characteristic[193]);
    /*!
      @brief Capture finger image
      @param[out] img Image vector (4 or 8 bits grayscale)
      @param raw Capture raw image(8bits) if true, Capture compressed image(4bits) if false
      @return True if successful
      @note Read 8 bits grayscale width x height byte pixel array if raw image
      @note Read 4 bits grayscale width/2 x height/2 nibble pixel array if NOT raw image
      @warning Some chips do not support capturing raw image
     */
    inline virtual bool captureImage(std::vector<uint8_t>& img, const bool raw = false)
    {
        return capture_image(img, raw);
    }
    /*!
      @brief Register characteristic
      @param user_id UserID
      @param permission Permission(1,2,3)
      @param characteristic Characteristic data
      @return True if successful
    */
    bool registerCharacteristic(const uint16_t user_id, const uint8_t permission, const uint8_t characteristic[193]);
    /*!
      @brief Verify specific user characteristic (1:1)
      @param[out] match Match if true
      @param user_id UserID
      @param characteristic Characteristic data
      @return True if successful
     */
    bool verifyCharacteristic(bool& match, const uint16_t user_id, const uint8_t characteristic[193]);
    /*!
      @brief Identify characteristic (1:N)
      @param[out] user_id Matching UserID, 0 if NoUser
      @param characteristic Characteristic data
      @return True if successful
     */
    bool identifyCharacteristic(uint16_t& user_id, const uint8_t characteristic[193]);
    /*!
      @brief Compare characteristic
      @param[out] match Match if true
      @param characteristic Characteristic data
      @return True if successful
     */
    bool compareCharacteristic(bool& match, const uint8_t characteristic[193]);
    ///@}

    /*!
      @brief Read the serial number (24 bits)
      @param[out] sno Serial number
      @return True if successful
     */
    bool readSerialNumber(uint32_t& no);
    /*!
      @brief Read the version string
      @param[out] str Output string buffer (At least 9 bytes)
      @return True if successful
     */
    bool readVersion(char str[9]);

    /*!
      @brief Go to deep sleep
      @return True if successful
      @warning Note that the unit must be physically reconnected in order to wake up from deep sleep
     */
    bool sleep(void);

protected:
    inline bool is_valid_user_id(const uint16_t user_id) const
    {
        return user_id >= minimumUserID() && user_id <= maximumUserID();
    }
    bool transceive_command(Frame& res, const uint8_t cmd, const uint8_t p1 = 0, const uint8_t p2 = 0,
                            const uint8_t p3 = 0);
    bool capture_image(std::vector<uint8_t>& img, const bool full = false);

private:
    config_t _cfg{};
};

/*!
  @class UnitFPC1020A
  @brief For FPC1020A
  @details
  |Resolution|DPI|Support capture full image?|
  |---|---|---|
  |160 x 160 | 504 dpi | OK |
 */
class UnitFPC1020A : public UnitFPC1XXX {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitFPC1020A, 0x00);

public:
    static constexpr uint16_t RESOLUTION_WIDTH{160};
    static constexpr uint16_t RESOLUTION_HEIGHT{160};
    static constexpr uint16_t MINIMUM_USER_ID{1};
    static constexpr uint16_t MAXIMUM_USER_ID{150};

    UnitFPC1020A() : UnitFPC1XXX()
    {
    }
    virtual ~UnitFPC1020A() = default;

    //! @brief Begin communication with the unit
    virtual bool begin() override;

    inline virtual uint16_t resolutionWidth() const override
    {
        return this->RESOLUTION_WIDTH;
    }
    inline virtual uint16_t resolutionHeight() const override
    {
        return this->RESOLUTION_HEIGHT;
    }
    inline virtual uint16_t minimumUserID() const override
    {
        return MINIMUM_USER_ID;
    }
    inline virtual uint16_t maximumUserID() const override
    {
        return MAXIMUM_USER_ID;
    }
};

namespace fpc1xxx {
///@cond
namespace command {

constexpr uint8_t CMD_REGISTER_FINGER_FIRST{0x01};
constexpr uint8_t CMD_REGISTER_FINGER_STEP{0x02};
constexpr uint8_t CMD_REGISTER_FINGER_LAST{0x03};
constexpr uint8_t CMD_VERIFY_FINGER{0x0B};
constexpr uint8_t CMD_IDENTIFY_FINGER{0x0C};

constexpr uint8_t CMD_DELETE_USER{0x04};
constexpr uint8_t CMD_DELETE_ALL_USERS{0x05};
constexpr uint8_t CMD_READ_REGISTERED_USER_COUNT{0x09};
constexpr uint8_t CMD_READ_USER_PERMISSION{0x0A};
constexpr uint8_t CMD_READ_ALL_USER_DATA{0x2B};
constexpr uint8_t CMD_FIND_UNREGISTERD_USER_ID{0x47};

constexpr uint8_t CMD_BAUD_RATE{0x21};
constexpr uint8_t CMD_READ_VERSION{0x26};
constexpr uint8_t CMD_COMPARISON_LEVEL{0x28};
constexpr uint8_t CMD_REGISTRATION_MODE{0x2D};
constexpr uint8_t CMD_TIMEOUT{0x2E};
constexpr uint8_t CMD_READ_SERIAL_NO{0x2A};

constexpr uint8_t CMD_SLEEP{0x2C};

constexpr uint8_t CMD_CAPTURE_IMAGE{0x24};

constexpr uint8_t CMD_SCAN_CHARACTERISTIC{0x23};
constexpr uint8_t CMD_READ_USER_CHARACTERISTIC{0x31};
constexpr uint8_t CMD_REGISTER_CHARACTERISTIC{0x41};
constexpr uint8_t CMD_VERIFY_CHARACTERISTIC{0x42};
constexpr uint8_t CMD_IDENTIFY_CHARACTERISTIC{0x43};
constexpr uint8_t CMD_COMPARE_CHARACTERISTIC{0x44};

}  // namespace command
///@endcond
///@cond
namespace detail {
using Frame = UnitFPC1XXX::Frame;

constexpr uint8_t MARKER{0xF5};  //!< Frame marker for head and tail

//! @brief XOR checksum
uint8_t xorSum(const uint8_t* data, const uint16_t len);
//! @brief Validate frame checksum (and optionally markers)
bool is_valid_sum(const Frame response, const bool check_marker = true);
//! @brief Validate variable-length payload checksum and markers
bool is_valid_payload(const uint8_t* data, const uint16_t len);
}  // namespace detail
///@endcond

}  // namespace fpc1xxx

}  // namespace unit
}  // namespace m5

#endif

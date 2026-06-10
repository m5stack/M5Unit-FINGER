/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_Finger2.hpp
  @brief Finger2 unit for M5UnitUnified
 */
#ifndef M5_UNIT_FINGER_UNIT_FINGER2_HPP
#define M5_UNIT_FINGER_UNIT_FINGER2_HPP

#include <M5UnitComponent.hpp>
#include <vector>
#include <array>

namespace m5 {
namespace unit {
/*!
  @namespace finger2
  @brief For UnitFinger2
 */
namespace finger2 {

/*!
  @enum WorkMode
  @brief Work mode
 */
enum class WorkMode : uint8_t {
    ScheduledSleep,  //!< Automatic activation upon fingerprint detection.
                     //!< Go to sleep mode if no fingerprint is detected for a certain period of time
    AlwaysActive,    //!< Always active
};

/*!
  @enum LEDMode
  @brief LED operation mode
 */
enum class LEDMode : uint8_t {
    None,             //!< None
    Breath,           //!< Roundtrip between light up and out gradually (as default)
    Blink,            //!< Blinking
    On,               //!< Light up
    Off,              //!< Light out
    Fadein,           //!< Light up gradually
    Fadeout,          //!< Light out gradually
    Rainbow,          //!< Roundtrip rainbow color
    Bleath = Breath,  //!< @deprecated Use Breath
};

/*!
  @enum LEDColor
  @brief Color for LED
 */
enum class LEDColor : uint8_t {
    Black,    //!< Lights off
    Blue,     //!< Blue (as default)
    Green,    //!< Green
    Cyan,     //!< Cyan
    Red,      //!< Red
    Magenta,  //!< Magenta
    Yellow,   //!< Yellow
    White     //!< White
};

/*!
  @enum RegisterID
  @brief PS_WriteReg target register
 */
enum class RegisterID : uint8_t {
    ScoreLevel = 5,  //!< 0x05:Score level for matching (1-5, 3 as default)
    PacketSize = 6,  //!< 0x06:Data packet size (0:32,1:64,2:128,3:256) (1 as default)
};

/*!
 @struct SystemBasicParams
 @brief The module’s basic parameters
*/
struct SystemBasicParams {
    uint16_t status{};             //!< System operational status
    uint16_t sensor_type{};        //!< Sensor type
    uint16_t database_capacity{};  //!< Fingerprint database capacity
    uint16_t score_level{};        //!< Match threshold (See also RegisterID::ScoreLevel)
    uint32_t address{};            //!< Device address
    uint16_t packet_size{};        //!< Packet size (See also RegisterID::PacketSize)
    uint16_t baud_rate{};          //!< Baud rate between STM32 and device
                                   //!< (The value multiplied by 9600 is the actual)
} __attribute__((packed));

using auto_enroll_flag_t   = uint16_t;  //!< Flags for autoEnroll
using auto_identify_flag_t = uint16_t;  //!< Flags for autoIdentify

/*!
  @namespace auto_enroll_flag
  @brief Flags for autoEnroll
 */
namespace auto_enroll_flag {
constexpr auto_enroll_flag_t DONT_RETURN_INTERMEDIATE_RESULTS{1U << 2};  //!< Do not return intermediate results
constexpr auto_enroll_flag_t ALLOW_OVERWRITE_PAGE{1U << 3};              //!< Allow overwriting of page_id
constexpr auto_enroll_flag_t PROHIBIT_DUPLICATE_TEMPLATE{1U << 4};       //!< Template Duplication prohibited
constexpr auto_enroll_flag_t NO_NEED_RELEASE_FINGER{1U << 5};            //!< No need to release finger

/*! @deprecated Use NO_NEED_RELEASE_FINGER */
[[deprecated("use NO_NEED_RELEASE_FINGER")]] constexpr auto_enroll_flag_t NO_NEED_RELAESE_FINGER =
    NO_NEED_RELEASE_FINGER;

}  // namespace auto_enroll_flag

/*!
  @namespace auto_identify_flag
  @brief Flags for autoIdentify
 */
namespace auto_identify_flag {
constexpr auto_identify_flag_t DONT_RETURN_INTERMEDIATE_RESULTS{1U << 2};  //!< Do not return intermediate results
}  // namespace auto_identify_flag

/*!
  @enum AutoEnrollStage
  @brief Interim stage of AutoEnroll
 */
enum class AutoEnrollStage : uint8_t {
    VerifyCommand,           //!< Command verification
    GetImage,                //!< PS_GetEnrollImage
    GenerateCharacteristic,  //!< PS_GenChar
    ReleasedFinger,          //!< Detected that the finger was released
    MergeTemplate,           //!< PS_RegModel
    Inspection,              //!< Inspection
    StoreTemplate,           //!< PS_StoreChar
};

/*!
  @enum AutoIdentifyStage
  @brief Interim stage of AutoIdentify
 */
enum class AutoIdentifyStage : uint8_t {
    VerifyCommand,  //!< Command verification
    GetImage,       //!< PS_GetImage
    Result = 0x05,  //!< Result
};

/*!
  @enum ConfirmCode
  @brief Confirmation code
 */
enum class ConfirmCode : uint8_t {
    OK,                  //!< 0x00:Instruction implementing end or OK
    PacketError,         //!< 0x01:Data packet receiving error
    NoFinger,            //!< 0x02:No finger on the sensor
    ImageFailed,         //!< 0x03:Getting fingerprint image failed
    ImageTooDry,         //!< 0x04:The fingerprint image is too dry or too light to generate feature
    ImageTooHumid,       //!< 0x05:The fingerprint image is too humid or too blurry to generate feature
    ImageTooAmorphous,   //!< 0x06:The fingerprint image is too amorphous to generate feature
    ImageTooFew,         //!< 0x07:The fingerprint image is in order, but with too few minutiae (or too small
                         //!< area) to generate feature
    Unmatched,           //!< 0x08:The fingerprint unmatched
    NotFound,            //!< 0x09:No fingerprint searched
    MergeFailed,         //!< 0x0A:The feature merging failed
    AddressOverflow,     //!< 0x0B:The address SN exceeding the range of fingerprint database when accessing to it
    ReadTemplateFailed,  //!< 0x0C:Template reading error or invalid from the fingerprint database
    UploadFailed,        //!< 0x0D:Feature uploading failed
    ReceiveFailed,       //!< 0x0E:The module cannot receive continue data packet
    UploadImageFailed,   //!< 0x0F:Image uploading failed
    DeleteFailed,        //!< 0x10:Module deleting failed
    ClearFailed,         //!< 0x11:The fingerprint database clearing failed
    LowPowerFailed,      //!< 0x12:Cannot be in low power consumption
    PasswordIncorrect,   //!< 0x13:The password incorrect
    ResetFailed,         //!< 0x14:The system reset failed
    ResettFailed = ResetFailed,  //!< @deprecated Use ResetFailed
    NoValidImage,                //!< 0x15:There is no valid original image in buffer to generate image
    UpgradeFailed,               //!< 0x16:On-line upgrading failed;
    IncompleteFinger,      //!< 0x17:There are incomplete fingerprint or finger stay still between twice image capturing
    FlashError,            //!< 0x18:Read-write FLASH error
    RandomError,           //!< 0x19:Random number generation failed
    InvalidRegister,       //!< 0x1A:Invalid register number
    RegisterError,         //!< 0x1B:Register distributing content wrong number
    NotepadPageError,      //!< 0x1C:Notepad page number appointing error
    PortError,             //!< 0x1D:Port operation failed
    AutoEnrollFailed,      //!< 0x1E:Automatic enroll failed
    DatabaseFull,          //!< 0x1F:Fingerprint database is full
    IncorrectAddress,      //!< 0x20:Incorrect device address
    VerifyPassword,        //!< 0x21:Must verify password first
    TemplateNotEmpty,      //!< 0x22:Fingerprint template is NOT empty
    TemplateEmpty,         //!< 0x23:Fingerprint template is empty
    DatabaseEmpty,         //!< 0x24:The fingerprint database is empty
    IncorrectEnrollCount,  //!< 0x25;Incorrect entry count setting
    Timeout,               //!< 0x26:Timeout
    AlreadyExists,         //!< 0x27:Fingerprint already exists
    FeatureAssociated,     //!< 0x28:Fingerprint features are associated
    InitializeFailed,      //!< 0x29:Sensor initialization failed
    initializeFailed = InitializeFailed,  //!< @deprecated Use InitializeFailed
    InformationNotEmpty,                  //!< 0x2A:Module information is NOT empty
    InformationEmpty,                     //!< 0x2B:Module information is empty
    OTPFailed,                            //!< 0x2C:OTP operation failed
    KeyGenerateFailed,                    //!< 0x2D:Key generation failed
    KeyNotExist,                          //!< 0x2E:The key does not exist
    AlgorithmFailed,                      //!< 0x2F:Security algorithm execution failed
    IncorrectResult,           //!< 0x30:The encryption/decryption results of the security algorithm are incorrect
    MismatchFunction,          //!< 0x31:Functionality does not match the encryption level
    KeyLocked,                 //!< 0x32:The key has been locked
    SmallImage,                //!< 0x33:Image too small
    StaticObjectInImage,       //!< 0x34:Static foreign object in the image (Orange)
    IllegalData,               //!< 0x35:The data is illegal
    PacketTimeout = 0xF9,      //!< 0xF9:Receive packet timeout
    PacketBad,                 //!< 0xFA:Error Packet (e.g., data not fully received, other packet received)
    PacketOverflow,            //!< 0xFB:Packet overflow (e.g., when a packet exceeds the maximum length)
    OperationBlocked,          //!< 0xFC:This operation has been blocked
    ParameterError,            //!< 0xFD:Parameter error
    NotActive,                 //!< 0xFE:The fingerprint module is not activated
    PassiveActivation = 0xFF,  //!< 0xFF:Passive activation
};

/*!
  @brief Callback function for autoEnroll
  @param call_times Number of callback invocations (zero origin)
  @param page_id page_id being attempted to register
  @param confirm ConfirmCode
  @param stage AutoEnrollStage
  @param state State value of the stage
  @retval true: Continue process
  @retval false: Abort process
*/
using auto_enroll_callback_t = bool (*)(const uint16_t call_times, const uint16_t page_id, const ConfirmCode confirm,
                                        const AutoEnrollStage stage, const uint8_t state);
/*!
  @brief Callback function for autoIdentify
  @param call_times Number of callback invocations (zero origin)
  @param confirm ConfirmCode
  @param stage AutoIdentifyStage
  @retval true: Continue process
  @retval false: Abort process
*/
using auto_identify_callback_t = bool (*)(const uint16_t call_times, const ConfirmCode confirm,
                                          const AutoIdentifyStage stage);

/*!
  @brief Callback for batch read/write
  @param call_times Number of callback invocations (zero origin)
  @param actual_size Size processed in a single batch operation
  @param batch_size Processing size per batch
  @param total_size Total processed size
  @param planned_size Planned size
  @param completed Is this the final step?
  @retval true: Continue process
  @retval false: Abort process (If completed == true, ignore)
 */
using batch_callback_t = bool (*)(const uint16_t call_times, const uint16_t actual_size, const uint16_t batch_size,
                                  const uint16_t total_size, const uint16_t planned_size, const bool completed);

}  // namespace finger2

/*!
  @class UnitFinger2
  @brief Fingerprint unit
 */
class UnitFinger2 : public Component {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitFinger2, 0x00);

public:
    static constexpr uint32_t DEFAULT_MODULE_ADDRESS{0xFFFFFFFF};  //!< Default device address
    using Packet = std::vector<uint8_t>;                           //!< Packet buffer for communication

    static constexpr uint16_t IMAGE_WIDTH{80};      //!< Capture image width
    static constexpr uint16_t IMAGE_HEIGHT{208};    //!< Captured image height
    static constexpr uint16_t TEMPLATE_SIZE{7262};  //!< Template size

protected:
    explicit UnitFinger2(const uint32_t device_address) : Component(DEFAULT_ADDRESS), _address{device_address}
    {
    }

public:
    UnitFinger2() : UnitFinger2(DEFAULT_MODULE_ADDRESS)
    {
    }

    virtual ~UnitFinger2() = default;

    /*!
      @brief Begin communication with the unit
      @return True if successful
     */
    virtual bool begin() override;

    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        uint32_t timeout_ms{1000 * 4};                                   //!< Serial I/O timeout (ms)
        finger2::WorkMode work_mode{finger2::WorkMode::ScheduledSleep};  //!< Work mode
        uint8_t sleep_time{10};                                          //!< Scheduled sleep time (10 - 254) sec
    };

    ///@name Settings for begin
    ///@{
    /*!
      @brief Gets the configuration
      @return Configuration
     */
    inline config_t config() const
    {
        return _cfg;
    }
    /*!
      @brief Set the configuration
      @param cfg Configuration
     */
    inline void config(const config_t& cfg)
    {
        _cfg = cfg;
    }
    ///@}

    ///@name Properties
    ///@{
    /*!
      @brief Get the device address
      @return Device address
      @note Changing the device address is not permitted
    */
    uint32_t deviceAddress() const
    {
        return _address;
    }
    /*!
      @brief Get the image width
      @return Image width
     */
    inline constexpr uint16_t imageWidth() const
    {
        return IMAGE_WIDTH;
    }
    /*!
      @brief Get the image height
      @return Image height
     */
    inline constexpr uint16_t imageHeight() const
    {
        return IMAGE_HEIGHT;
    }
    /*!
      @brief Get the capacity of templates
      @return Capacity of templates
      @warning Returns the correct value after begin() succeeds
    */
    inline uint16_t capacity() const
    {
        return _pageCapacity;
    }
    ///@}

    /*!
      @brief Read the module status
      @details PS_GetFingerprintModuleStatus
      @param[out] awake true if awake
      @return True if successful
     */
    bool readModuleStatus(bool& awake);

    /*!
      @brief Wakeup
      @details PS_ActivateModule
      @return True if successful
      @note If WorkMode is ScheduledSleep, the device goes to sleep if fingerprints remain undetected
     */
    bool wakeup();

    ///@name Settings
    ///@{
    /*!
      @brief Read the work mode
      @details PS_GetWorkMode
      @param[out] wm WorkMode
      @return True if successful
     */
    bool readWorkMode(finger2::WorkMode& wm);
    /*!
      @brief Write the work mode
      @details PS_SetWorkMode
      @param wm WorkMode
      @return True if successful
     */
    bool writeWorkMode(const finger2::WorkMode wm);
    /*!
      @brief Write the current work mode to internal flash
      @details PS_SaveConfigurationToFlash
      @return True if successful
      @note It will be the default work mode at startup
      @warning This API will affect the life of the device. Do not use it frequently.
     */
    bool saveWorkMode();

    /*!
      @brief Read the time to go to sleep (second)
      @details PS_GetSleepTime
      @param[out] sec Time (second)
      @return True if successful
     */
    bool readSleepTime(uint8_t& sec);
    /*!
      @brief Write the time to go to sleep (second)
      @details PS_SetSleepTime
      @param sec Time that valid range 10-254 (second)
      @return True if successful
     */
    bool writeSleepTime(const uint8_t sec);
    /*!
      @brief Write the current sleep time to internal flash
      @details PS_SaveConfigurationToFlash
      @return True if successful
      @note It will be the default sleep time at startup
      @warning This API will affect the life of the device. Do not use it frequently.
     */
    bool saveSleepTime();

    /*!
      @brief Read the system basic parameters
      @details PS_ReadSysPara
      @param[out] params SystemBasicParams
      @return True if successful
      @warning Returns an error when device is sleeping
     */
    bool readSystemParams(finger2::SystemBasicParams& params);

    /*!
      @brief Write the value to register for settings
      @details PS_WriteReg
      @param reg_id Target RegisterID
      @param value Value
      @return True if successful
      @note See also the protocol specification document
     */
    bool writeSystemRegister(const finger2::RegisterID reg_id, const uint8_t value);

    /*!
      @brief Write the LED control
      @details PS_ControlBLN
      @param mode LEDMode (Ignore LEDMode::Rainbow)
      @param clr Start LEDColor
      @param cycle Cycle count, infinity if zero. Valid for LEDMode::Breath, LEDMode::Blink
      @param eclr End LEDColor (Valid for LEDMode::Breath)
      @return True if successful
      @warning Returns an error when device is sleeping
     */
    bool writeControlLED(const finger2::LEDMode mode, const finger2::LEDColor clr, const uint8_t cycle = 0,
                         const finger2::LEDColor eclr = finger2::LEDColor::Black);

    /*!
      @brief Write the LED control for LEDMode::Rainbow
      @details PS_ControlBLN
      @param tm Color change time (decisecond)
      @param colors LEDColor array (maximum 10)
      @param colors_num Number of the colors (maximum 10)
      @param cycle Cycle count, infinity if zero
      @return True if successful
      @warning Returns an error when device is sleeping
     */
    bool writeControlLEDRainbow(const uint8_t tm, const finger2::LEDColor* colors, const uint8_t colors_num,
                                const uint8_t cycle);
    ///@}

    ///@warning Returns an error when device is sleeping
    ///@name Finger
    ///@{
    /*!
      @brief Capture the fingerprint image
      @details PS_GetImage or PS_GetEnrollImage
      @param[out] detected true if finger detected
      @param enroll true for register(PS_GetEnrollImage), false for detection(PS_GetImage)
      @return True if successful
     */
    bool capture(bool& detected, const bool enroll = false);

    /*!
      @brief Capture the fingerprint and read image information
      @details PS_GetImageInfo
      @param[out] percentage Image area
      @param[out] quality Image quality (true : pass, false: fail)
      @return True if successful
      @pre capture() succeeded
     */
    bool readImageInformation(uint8_t& percentage, bool& quality);

    /*!
      @brief Read the finger image
      @details PS_UpImage
      @details Uploading data in image buffer to the host
      @param[out] img Image vector (4 bits grayscale)
      @return True if successful
     */
    bool readImage(std::vector<uint8_t>& img);

    /*!
      @brief Generate characteristic from captured image
      @details PS_GenChar
      @details Generating the original image in ImageBuffer to fingerprint feature file and store it in CharBuffer
      @param buffer_id Buffer number (1 - 5)
      @return True if successful
      @pre capture() succeeded
     */
    bool generateCharacteristic(const uint8_t buffer_id = 1);

    /*!
      @brief Generate template from merged characteristics
      @details PS_RegModel
      @details After merging the characteristics, generate a template.
      @return True if successful
      @pre generateCharacteristic() succeeded
     */
    bool generateTemplate();

    /*!
      @brief Store template
      @details PS_StoreChar
      @details Store the template at the buffer_id position into the page_id position in the database
      @param page_id Page number
      @param buffer_id Buffer number (1 - 5)
      @return True if successful
      @pre generateTemplate() succeeded
      @warning This feature is supported when the security level is 0 or 1
    */
    bool storeTemplate(const uint16_t page_id, const uint8_t buffer_id = 1);

    /*!
      @brief Is match buffer 1 and 2? (1:1)
      @details PS_Match
      @param[out] matched true if matched
      @param[out] score Matching score (lowest: 0)
      @pre Buffer IDs 1 and 2 contain template
      @return True if successful
      @warning This feature is supported when the security level is 0 or 1
    */
    bool match(bool& matched, uint16_t& score);

    /*!
      @brief Search for matching template (1:N)
      @details PS_Search
      @param[out] matched true if matched
      @param[out] matching_page_id Matching page number
      @param[out] score Matching score (lowest: 0)
      @param buffer_id Buffer number (comparison target)
      @param start_page Search start page
      @param page_num Number of search pages (0: From start_page to end)
      @return True if successful
      @pre The data exists in the specified buffer
      @warning This feature is supported when the security level is 0 or 1
     */
    bool search(bool& matched, uint16_t& matching_page_id, uint16_t& score, const uint8_t buffer_id = 1,
                const uint16_t start_page = 0, const uint16_t page_num = 0);

    /*!
      @brief Search for matching template using the last extracted template (1:N)
      @details PS_SearchNow
      @param[out] matched true if matched
      @param[out] matching_page_id Matching page number
      @param[out] score Matching score (lowest: 0)
      @param start_page Search start page
      @param page_num Number of search pages (0: From start_page to end)
      @return True if successful
      @pre The data exists in the specified buffer
      @warning This feature is supported when the security level is 0 or 1
     */
    bool searchNow(bool& matched, uint16_t& matching_page_id, uint16_t& score, const uint16_t start_page = 0,
                   const uint16_t page_num = 0);

    ///@}

    ///@warning Returns an error when device is sleeping
    ///@name Template
    ///@{
    /*!
      @brief Load template to buffer
      @details PS_LoadChar
      @details Reading the fingerprint templates which appointed page_id in flash database to template buffer
      @param buffer_id Target buffer number (1 - 5)
      @param page_id Source page number
      @return True if successful
     */
    bool loadTemplate(const uint8_t buffer_id, const uint16_t page_id);

    /*!
      @brief Read the specific size template from the specific offset
      @details PS_UpTemplet
      @details Uploading the feature files in feature buffer to the host
      @param [out] actual_size Actual size
      @param[out] buf Output buffer (at least buf_size bytes)
      @param buf_size  size of buf
      @param offset Offset address
      @return True if successful
      @pre The template exists in the buffer
     */
    bool readTemplate(uint16_t& actual_size, uint8_t* buf, const uint16_t buf_size, const uint16_t offset);

    /*!
      @brief Read the template
      @param[out] actual_size Actual size
      @param[out] buf Output buffer (at least buf_size bytes)
      @param buf_size Size of buf (the full template is TEMPLATE_SIZE (7262) bytes)
      @param batch_size Processing size per batch
      @param callback Callback invoked for each batch processing
      @return True if successful
      @pre The template exists in the buffer
     */
    bool readTemplateAllBatches(uint16_t& actual_size, uint8_t* buf, const uint16_t buf_size,
                                const uint16_t batch_size = 128, finger2::batch_callback_t callback = nullptr);
    /*!
      @brief Write the specific size template to the specific offset
      @param offset Offset address
      @param buf Input buffer
      @param buf_size  size of buf
      @return True if successful
     */
    bool writeTemplate(const uint16_t offset, const uint8_t* buf, const uint16_t buf_size);
    /*!
      @brief Write the template
      @param buf Input buffer
      @param buf_size  size of buf
      @param batch_size Processing size per batch
      @param callback Callback invoked for each batch processing
      @return True if successful
     */
    bool writeTemplateAllBatches(const uint8_t* buf, const uint16_t buf_size, const uint16_t batch_size = 128,
                                 finger2::batch_callback_t callback = nullptr);

    /*!
      @brief Delete templates
      @details PS_DeletChar
      @param page_id First page to be deleted
      @param num How many to delete starting from the first target
      @return True if successful
     */
    bool deleteTemplate(const uint16_t page_id, const uint16_t num = 1);

    /*!
      @brief Clear fingerprint database (delete all templates)
      @details PS_Empty
      @details Deleting all fingerprint modules in flash database
      @return True if successful
     */
    bool clear();

    /*!
      @brief Read the number of valid templates
      @details PS_ValidTemplateNum
      @param[out] num Number
      @return True if successful
     */
    bool readValidTemplates(uint16_t& num);

    /*!
      @brief Read the index table (Bits of existing templates)
      @details PS_ReadIndexTable
      @details Bit-level information for registered templates is retrieved
      @param[out] table Table data (at least 32 bytes)
      @return True if successful
     */
    bool readIndexTable(uint8_t table[32]);

    /*!
      @brief Exists template?
      @param page_id Page number
      @retval True if exists
      @retval False if not exists or failed
      @note Internally calling readIndexTable()
      @note Therefore, when checking whether a large number of templates are existed,
      @note it is better to call readIndexTable once and verify table yourself
     */
    bool existsTemplate(const uint16_t page_id);

    /*!
      @brief Find the lowest available page number
      @param[out] page Page number (Not exists if 0xFFFF)
      @return True if successful
     */
    bool findLowestAvailablePage(uint16_t& page);

    /*!
      @brief Find the highest available page number
      @param[out] page Page number (Not exists if 0xFFFF)
      @return True if successful
     */
    bool findHighestAvailablePage(uint16_t& page);
    ///@}

    ///@warning Returns an error when device is sleeping
    ///@name Automatic function
    ///@{
    /*!
      @brief Automatic registration
      @details PS_AutoEnroll
      @details Perform capture and generate characteristic operations for the specified number of times, merge them,
      and store template to the specified page_id
      @param[out] confirm ConfirmCode
      @param page_id Page number
      @param capture_times Number of finger captures for registration
      @param flags Operation Control Flag
      @param callback Callback function that receives and processes intermediate results
      @return True if successful
      @warning This feature is supported when the security level is 0 or 1
    */
    bool autoEnroll(finger2::ConfirmCode& confirm, const uint16_t page_id, const uint8_t capture_times = 5,
                    const finger2::auto_enroll_flag_t flags = 0, finger2::auto_enroll_callback_t callback = nullptr);

    /*!
      @brief Automatic identify (1:1, 1:N)
      @details PS_AutoIdentify
      @param[out] matched true if matched
      @param[out] matching_page_id Matching page number
      @param[out] score Matching score (lowest: 0)
      @param page_id Verification target page number(1:1), All page if page_id is 0xFFFF (1:N)
      @param security_level Security level (0 -1)
      @param flags Operation Control Flag
      @param callback Callback function that receives and processes intermediate results
      @return True if successful
      @warning This feature is supported when the security level is 0 or 1
     */
    bool autoIdentify(bool& matched, uint16_t& matching_page_id, uint16_t& score, const uint16_t page_id = 0xFFFF,
                      const uint8_t security_level = 0, finger2::auto_identify_flag_t flags = 0,
                      const finger2::auto_identify_callback_t callback = nullptr);

    /*!
      @brief Abort automatic functions
      @return True if successful
      @warning This feature is supported when the security level is 0 or 1
     */
    bool cancel();
    ///@}

    ///@name Notepad
    ///@{
    /*!
      @brief Read the notepad(Flash) data
      @details PS_ReadNotepad
      @param[out] buf Buffer (at least 32 bytes)
      @param page Notepad page number (0 - 7)
      @return True if successful
      @note Notepad is an area of 8 pages totaling 256 bytes in 32 byte units
      @warning Returns an error when device is sleeping
     */
    bool readNotepad(uint8_t buf[32], const uint8_t page);
    /*!
      @brief Write the notepad(Flash) data
      @details PS_WriteNotepad
      @param page Notepad page number (0 - 7)
      @param buf Input buffer
      @param len buffer length (1 - 32) If larger than 32 bytes, write up to 32 bytes
      @return True if successful
      @note Notepad is an area of 8 pages totaling 256 bytes in 32 byte units
      @warning Returns an error when device is sleeping
      @warning When less than 32 bytes are written, the missing bytes are filled with zeros
     */
    bool writeNotepad(const uint8_t page, const uint8_t* buf, const uint8_t len);
    ///@}

    /*!
      @brief Read the information page in FLASH (512 bytes)
      @details PS_ReadINFpage
      @param info Buffer (at least 512 bytes)
      @return True if successful
     */
    bool readInformationPage(uint8_t info[512]);

    /*!
      @brief Read the random number (32bits)
      @details PS_GetRandomCode
      @details Making chip generate a random code and return to the host
      @param[out] value Number
      @return True if successful
      @warning Returns an error when device is sleeping
     */
    bool readRandomNumber(uint32_t& value);

    /*!
      @brief Verify that the module is working properly
      @details PS_HandShake
      @param[out] status OK if true
      @return True if successful
      @warning Returns an error when device is sleeping
     */
    bool handshake(bool& status);
    /*!
      @brief Verify whether the sensor is working properly
      @details PS_CheckSensor
      @param[out] status OK if true
      @return True if successful
      @warning Returns an error when device is sleeping
     */
    bool checkSensor(bool& status);

    /*!
      @brief Read the serial number (32 bytes)
      @details PS_GetChipSN
      @param[out] sn serial number buffer (at least 32 bytes)
      @return True if successful
      @warning Returns an error when device is sleeping
     */
    bool readSerialNumber(uint8_t sn[32]);

    /*!
      @brief Read the firmware version
      @details PS_GetFirmwareVersion
      @param[out] ver Version
      @return True if successful
     */
    bool readFirmwareVersion(uint8_t& ver);

protected:
    bool write_command(const uint8_t cmd, const uint32_t addr, const uint8_t* payload = nullptr,
                       const uint16_t payload_len = 0);
    finger2::ConfirmCode read_response(Packet& rbuf);
    bool transceive_command(Packet& rbuf, const uint8_t cmd, const uint32_t addr, const uint8_t* payload = nullptr,
                            const uint16_t payload_len = 0);
    bool transceive_command8(Packet& rbuf, const uint8_t cmd, const uint32_t addr, const uint8_t value);
    uint8_t read_data(Packet& rbuf);

private:
    config_t _cfg{};
    uint32_t _address{};
    uint16_t _pageCapacity{};
};

namespace finger2 {
namespace command {
///@cond
constexpr uint8_t CMD_GET_IMAGE{0x01};
constexpr uint8_t CMD_GET_ENROLL_IMAGE{0x29};
constexpr uint8_t CMD_GENERATE_CHARACTERISTIC{0x02};
constexpr uint8_t CMD_MATCH{0x03};
constexpr uint8_t CMD_SEARCH{0x04};
constexpr uint8_t CMD_SEARCH_NOW{0x3E};
constexpr uint8_t CMD_REGISTER_MODEL{0x05};
constexpr uint8_t CMD_STORE_TEMPLATE{0x06};
constexpr uint8_t CMD_LOAD_TEMPLATE{0x07};

constexpr uint8_t CMD_UPLOAD_IMAGE{0x0A};
constexpr uint8_t CMD_DELETE_TEMPLATE{0x0C};
constexpr uint8_t CMD_EMPTY{0x0D};

constexpr uint8_t CMD_WRITE_REGISTER{0x0E};
constexpr uint8_t CMD_READ_SYSTEM_PARAMETER{0x0F};
constexpr uint8_t CMD_GET_RANDOM{0x14};
constexpr uint8_t CMD_READ_FLASH_INFORMATION{0x16};
constexpr uint8_t CMD_WRITE_NOTEPAD{0x18};
constexpr uint8_t CMD_READ_NOTEPAD{0x19};

constexpr uint8_t CMD_READ_VALID_TEMPLATE_NUMBER{0x1D};
constexpr uint8_t CMD_READ_INDEX_TABLE{0x1F};

constexpr uint8_t CMD_CANCEL{0x30};
constexpr uint8_t CMD_AUTO_ENROLL{0x31};
constexpr uint8_t CMD_AUTO_IDENTIFY{0x32};

constexpr uint8_t CMD_GET_SERIAL{0x34};
constexpr uint8_t CMD_HANDSHAKE{0x35};
constexpr uint8_t CMD_CHECK_SENSOR{0x36};
constexpr uint8_t CMD_CONTROL_BLN{0x3C};
constexpr uint8_t CMD_GET_IMAGE_INFORMATION{0x3D};

constexpr uint8_t CMD_UPLOAD_TEMPLATE{0x7A};
constexpr uint8_t CMD_DOWNLOAD_TEMPLATE{0x7B};

constexpr uint8_t CMD_SET_SLEEP_TIME{0xD0};
constexpr uint8_t CMD_GET_SLEEP_TIME{0xD1};
constexpr uint8_t CMD_SET_WORK_MODE{0xD2};
constexpr uint8_t CMD_GET_WORK_MODE{0xD3};
constexpr uint8_t CMD_ACTIVATE_MODULE{0xD4};
constexpr uint8_t CMD_GET_MODULE_STATUS{0xD5};
constexpr uint8_t CMD_SAVE_CONFIGURATION{0xD6};
constexpr uint8_t CMD_GET_FIRMWARE_VERSION{0xD7};
///@endcond
}  // namespace command
}  // namespace finger2

}  // namespace unit
}  // namespace m5
#endif

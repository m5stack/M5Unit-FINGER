/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_FPC1xxx.cpp
  @brief FPC1xxx family with DSP unit for M5UnitUnified
 */
#include "unit_FPC1xxx.hpp"
#include <M5Utility.hpp>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::fpc1xxx;
using namespace m5::unit::fpc1xxx::command;

enum FrameOffset : uint8_t {
    OFFSET_HEAD,
    OFFSET_CMD,
    OFFSET_P1,
    OFFSET_P2,
    OFFSET_P3,
    OFFSET_RESERVE,
    OFFSET_CRC,
    OFFSET_TAIL,
    OFFSET_Q1    = OFFSET_P1,
    OFFSET_Q2    = OFFSET_P2,
    OFFSET_Q3    = OFFSET_P3,
    OFFSET_LEN_H = OFFSET_P1,
    OFFSET_LEN_L = OFFSET_P2,

};

namespace {
constexpr uint32_t TIMEOUT_MS{1000 * 4};

using Frame         = m5::unit::UnitFPC1XXX::Frame;
using VariableFrame = m5::unit::UnitFPC1XXX::VariableFrame;

constexpr uint8_t MARKER{0xF5};  // Frame marker for head and tail

uint8_t xorSum(const uint8_t* data, const uint16_t len)
{
    uint8_t sum{};
    for (uint_fast16_t i = 0; i < len; ++i) {
        sum ^= *data++;
    }
    return sum;
}

Frame make_frame(const uint8_t cmd, const uint8_t p1 = 0, const uint8_t p2 = 0, const uint8_t p3 = 0)
{
    Frame frame{};
    frame[OFFSET_HEAD] = frame[OFFSET_TAIL] = MARKER;
    frame[OFFSET_CMD]                       = cmd;
    frame[OFFSET_P1]                        = p1;
    frame[OFFSET_P2]                        = p2;
    frame[OFFSET_P3]                        = p3;
    frame[OFFSET_RESERVE]                   = 0;
    frame[OFFSET_CRC]                       = xorSum(frame.data() + 1, 5);
    return frame;
}

VariableFrame make_variable_frame(const uint8_t cmd, const uint16_t len, const uint8_t* data, const uint16_t dlen)
{
    VariableFrame frame{};
    frame.resize(8 /* header */ + dlen + 3 /* marker,chk,marker */);

    frame[OFFSET_HEAD] = frame[OFFSET_TAIL] = MARKER;
    frame[OFFSET_CMD]                       = cmd;
    frame[OFFSET_LEN_H]                     = len >> 8;
    frame[OFFSET_LEN_L]                     = len & 0xFF;
    frame[OFFSET_P3]                        = 0;
    frame[OFFSET_RESERVE]                   = 0;
    frame[OFFSET_CRC]                       = xorSum(frame.data() + 1, 5);

    if (!data || !dlen) {
        M5_LIB_LOGW("Payload empty");
        return frame;
    }

    auto ptr = frame.data() + 8;
    *ptr++   = MARKER;
    memcpy(ptr, data, dlen);
    ptr += dlen;
    *ptr++ = xorSum(data, dlen);
    *ptr++ = MARKER;

    assert(ptr - frame.data() == frame.size() && "Illegal size");
    return frame;
}

bool is_valid_sum(const Frame response, const bool check_marker = true)
{
    return (xorSum(response.data() + 1, 5) == response[OFFSET_CRC]) && check_marker
               ? (response[OFFSET_HEAD] == MARKER && response[OFFSET_TAIL] == MARKER)
               : true;
}

bool is_valid_payload(const uint8_t* data, const uint16_t len)
{
    // 0xF5 Data... CHK 0xF5
    return data && len >= 4 && data[0] == MARKER && data[len - 1] == MARKER &&
           xorSum(data + 1, len - 3) == data[len - 2];
}

bool is_valid_ACK(const Frame& f)
{
    if (f[OFFSET_Q3] == m5::stl::to_underlying(ACK::Success)) {
        return true;
    }
    M5_LIB_LOGE("ACK Error: %u", f[OFFSET_Q3]);
    return false;
}

inline bool is_valid_permission(const uint8_t p)
{
    return p >= 1 && p <= 3;
}

}  // namespace

namespace m5 {
namespace unit {

// class UnitFPC1XXX
const char UnitFPC1XXX::name[] = "UnitFPC1XXX";
const types::uid_t UnitFPC1XXX::uid{"UnitFPC1XXX"_mmh3};
const types::attr_t UnitFPC1XXX::attr{attribute::AccessUART};

bool UnitFPC1XXX::begin()
{
    m5::utility::delay(200);

    auto ad = asAdapter<AdapterUART>(Adapter::Type::UART);
    if (!ad) {
        M5_LIB_LOGE("Illegal adapter");
        return false;
    }
    ad->setTimeout(_cfg.timeout_ms ? _cfg.timeout_ms : TIMEOUT_MS);
    ad->flushRX();

    uint32_t sno{0xdeadbeef};
    if (!readSerialNumber(sno)) {
        M5_LIB_LOGE("Failed to readSerialNumber %x", sno);
        return false;
    }

    char ver[9]{};
    if (!readVersion(ver)) {
        M5_LIB_LOGE("Failed to readVersion [%s]", ver);
        return false;
    }
    M5_LIB_LOGI("Serial:%x Version:[%s]", sno, ver);

    return true;
}

bool UnitFPC1XXX::readRegistrationMode(fpc1xxx::Mode& mode)
{
    mode = Mode::AllowDuplicate;

    Frame res{};
    if (transceive_command(res, CMD_REGISTRATION_MODE, 0, 0, 1 /* read */) && is_valid_ACK(res)) {
        mode = static_cast<Mode>(res[OFFSET_Q2]);
        return true;
    }
    return false;
}

bool UnitFPC1XXX::writeRegistrationMode(const fpc1xxx::Mode mode)
{
    Frame res{};
    return transceive_command(res, CMD_REGISTRATION_MODE, 0, m5::stl::to_underlying(mode), 0 /* write */) &&
           is_valid_ACK(res);
}

bool UnitFPC1XXX::readComparisonLevel(uint8_t& lv)
{
    lv = 0xFF;

    Frame res{};
    if (transceive_command(res, CMD_COMPARISON_LEVEL, 0, 0, 1 /* read */) && is_valid_ACK(res)) {
        lv = res[OFFSET_Q2];
        return true;
    }
    return false;
}

bool UnitFPC1XXX::writeComparisonLevel(const uint8_t lv)
{
    Frame res{};
    if (lv > 9) {
        M5_LIB_LOGE("lv must be must be 0-9 (%u)", lv);
        return false;
    }
    return transceive_command(res, CMD_COMPARISON_LEVEL, 0, lv, 0 /* write */) && is_valid_ACK(res);
}

bool UnitFPC1XXX::writeBaudRate(const fpc1xxx::BaudRate baud)
{
    auto frame = make_frame(CMD_BAUD_RATE, 0, 0, m5::stl::to_underlying(baud));
    Frame res{};
    // No check ACK, because Q3 is old baud
    // Also, the baud rate is changed after the response CRC is sent, so the tail marker cannot be received correctly
    return writeWithTransaction(frame.data(), frame.size()) == m5::hal::error::error_t::OK &&
           readWithTransaction(res.data(), res.size()) == m5::hal::error::error_t::OK &&
           res[OFFSET_CMD] == CMD_BAUD_RATE && is_valid_sum(res, false);
}

bool UnitFPC1XXX::readTimeout(uint8_t& timeout)
{
    timeout = 0;

    Frame res{};
    if (transceive_command(res, CMD_TIMEOUT, 0, 0, 1 /* read */) && is_valid_ACK(res)) {
        timeout = res[OFFSET_Q2];
        return true;
    }
    return false;
}

bool UnitFPC1XXX::writeTimeout(const uint8_t timeout)
{
    Frame res{};
    return transceive_command(res, CMD_TIMEOUT, 0, timeout, 0 /* write */) && is_valid_ACK(res);
}

bool UnitFPC1XXX::readRegisteredUserCount(uint16_t& count)
{
    count = 0;

    Frame res{};
    if (transceive_command(res, CMD_READ_REGISTERED_USER_COUNT)) {
        count |= ((uint16_t)res[OFFSET_Q1]) << 8;
        count |= ((uint16_t)res[OFFSET_Q2]);
        return true;
    }
    return false;
}

bool UnitFPC1XXX::readUser(uint8_t& permission, const uint16_t user_id)
{
    if (!is_valid_user_id(user_id)) {
        M5_LIB_LOGE("Illegal user_id %u", user_id);
        return false;
    }

    permission = m5::stl::to_underlying(ACK::NoUser);

    Frame res{};
    if (transceive_command(res, CMD_READ_USER_PERMISSION, user_id >> 8, user_id & 0xFF)) {
        permission = res[OFFSET_Q3];
        // If there is no User, the operation terminates normally and ACK_NOUSER is returned
        return is_valid_permission(permission);
    }
    return false;
}

bool UnitFPC1XXX::readAllUser(std::vector<fpc1xxx::User>& v)
{
    v.clear();

    Frame res{};
    if (transceive_command(res, CMD_READ_ALL_USER_DATA) && is_valid_ACK(res)) {
        uint16_t vlen = (((uint16_t)res[OFFSET_Q1]) << 8) | res[OFFSET_Q2];
        uint8_t vbuf[vlen + 3]{};
        if (readWithTransaction(vbuf, vlen + 3) == m5::hal::error::error_t::OK && is_valid_payload(vbuf, vlen + 3)) {
            uint16_t user_count = ((uint16_t)vbuf[1] << 8) | vbuf[2];
            const uint8_t* data = vbuf + 3;
            for (uint_fast16_t i = 0; i < user_count; ++i) {
                uint16_t id = ((uint16_t)data[0] << 8) | data[1];
                uint8_t p   = data[2];
                data += 3;
                v.emplace_back(id, p);
            }
            return true;
        }
    }
    return false;
}

bool UnitFPC1XXX::readUserCharacteristic(uint8_t characteristic[193], const uint16_t user_id)
{
    memset(characteristic, 0x00, 193);

    if (!is_valid_user_id(user_id)) {
        M5_LIB_LOGE("Illegal user_id %u", user_id);
        return false;
    }

    Frame res{};
    if (transceive_command(res, CMD_READ_USER_CHARACTERISTIC, user_id >> 8, user_id & 0xFF) && is_valid_ACK(res)) {
        uint16_t vlen = (((uint16_t)res[OFFSET_Q1]) << 8) | res[OFFSET_Q2];
        if (vlen == 196) {
            uint8_t vbuf[vlen + 3]{};
            if (readWithTransaction(vbuf, vlen + 3) == m5::hal::error::error_t::OK &&
                is_valid_payload(vbuf, vlen + 3)) {
                //                m5::utility::log::dump(vbuf, vlen + 3, false);
                uint16_t read_user_id = (((uint16_t)vbuf[1]) << 8) | vbuf[2];
                if (user_id != read_user_id) {
                    M5_LIB_LOGE("Illegal user %u/%u", read_user_id, user_id);
                    return false;
                }
                memcpy(characteristic, vbuf + 4, vlen - 3);
                return true;
            }
        } else {
            M5_LIB_LOGE("Illegal request length %u", vlen);
        }
    }
    return false;
}

bool UnitFPC1XXX::seachUnregisterdUserID(uint16_t& user_id, const uint16_t low, const uint16_t high)
{
    user_id = 0;

    uint16_t low_id  = low ? low : minimumUserID();
    uint16_t high_id = high ? high : maximumUserID();

    std::array<uint8_t, 4> payload{};
    payload[0] = low_id >> 8;
    payload[1] = low_id & 0xFF;
    payload[2] = high_id >> 8;
    payload[3] = high_id & 0xFF;
    auto frame = make_variable_frame(CMD_SEARCH_UNREGISTERD_USER_ID, 4, payload.data(), payload.size());

    Frame res{};
    if (writeWithTransaction(frame.data(), frame.size()) == m5::hal::error::error_t::OK &&
        readWithTransaction(res.data(), res.size()) == m5::hal::error::error_t::OK &&
        res[OFFSET_CMD] == CMD_SEARCH_UNREGISTERD_USER_ID && is_valid_sum(res) && is_valid_ACK(res)) {
        user_id = (((uint16_t)res[OFFSET_Q1]) << 8) | (uint16_t)res[OFFSET_Q2];
        return true;
    }
    return false;
}

bool UnitFPC1XXX::deleteUser(const uint16_t user_id)
{
    if (!is_valid_user_id(user_id)) {
        M5_LIB_LOGE("Illegal user_id %u", user_id);
        return false;
    }

    Frame res{};
    return transceive_command(res, CMD_DELETE_USER, user_id >> 8, user_id & 0xFF) && is_valid_ACK(res);
}

bool UnitFPC1XXX::deleteAllUsers()
{
    Frame res{};
    return transceive_command(res, CMD_DELETE_ALL_USERS) && is_valid_ACK(res);
}

bool UnitFPC1XXX::registerFinger(const uint16_t user_id, const uint8_t permission, const uint8_t step)
{
    if (!is_valid_user_id(user_id)) {
        M5_LIB_LOGE("Illegal user_id %u", user_id);
        return false;
    }

    if (!is_valid_permission(permission)) {
        M5_LIB_LOGE("Illegal permission %u", permission);
        return false;
    }

    uint8_t id_h = user_id >> 8;
    uint8_t id_l = user_id & 0xFF;

    Frame res{};
    // First
    if (!transceive_command(res, CMD_REGISTER_FINGER_FIRST, id_h, id_l, permission) || !is_valid_ACK(res)) {
        M5_LIB_LOGE("Failed to register 1st %u", res[OFFSET_Q3]);
        if (res[OFFSET_Q3] == m5::stl::to_underlying(ACK::ExistUser)) {
            M5_LIB_LOGW("User already exists");
        }
        return false;
    }

    // 2nd steps
    for (uint_fast8_t i = 0; i < step; ++i) {
        res.fill(0);
        if (!transceive_command(res, CMD_REGISTER_FINGER_STEP, id_h, id_l, permission) || !is_valid_ACK(res)) {
            M5_LIB_LOGE("Failed to register 2nd[%u] %u", i, res[OFFSET_Q3]);
            return false;
        }
    }

    // Final
    res.fill(0);
    if (!transceive_command(res, CMD_REGISTER_FINGER_LAST, id_h, id_l, permission) || !is_valid_ACK(res)) {
        M5_LIB_LOGE("Failed to register 3rd %u", res[OFFSET_Q3]);
        return false;
    }
    return true;
}

bool UnitFPC1XXX::verifyFinger(bool& match, const uint16_t user_id)
{
    match = false;

    if (!is_valid_user_id(user_id)) {
        M5_LIB_LOGE("Illegal user_id %u", user_id);
        return false;
    }

    Frame res{};
    if (transceive_command(res, CMD_VERIFY_FINGER, user_id >> 8, user_id & 0xFF)) {
        match = is_valid_ACK(res);
        return true;
    }
    return false;
}

bool UnitFPC1XXX::identifyFinger(uint16_t& user_id, uint8_t& permission)
{
    user_id = permission = 0;

    Frame res{};
    if (transceive_command(res, CMD_IDENTIFY_FINGER)) {
        if (is_valid_permission(res[OFFSET_Q3])) {
            user_id    = (((uint16_t)res[OFFSET_Q1]) << 8) | (uint16_t)res[OFFSET_Q2];
            permission = res[OFFSET_Q3];
            return true;
        }
    }
    return false;
}

bool UnitFPC1XXX::scanCharacteristic(uint8_t characteristic[193])
{
    if (!characteristic) {
        return false;
    }

    memset(characteristic, 0x00, 193);

    Frame res{};
    if (transceive_command(res, CMD_SCAN_CHARACTERISTIC) && is_valid_ACK(res)) {
        uint16_t vlen = ((uint16_t)res[OFFSET_Q1] << 8) | (uint16_t)res[OFFSET_Q2];
        if (vlen != 196) {
            M5_LIB_LOGE("Illegal size %u", vlen);
            return false;
        }
        uint8_t vbuf[vlen + 3]{};
        if (readWithTransaction(vbuf, vlen + 3) == m5::hal::error::error_t::OK && is_valid_payload(vbuf, vlen + 3)) {
            memcpy(characteristic, vbuf + 4, vlen - 3);
            return true;
        }
    }
    return false;
}

bool UnitFPC1XXX::capture_image(std::vector<uint8_t>& img, const bool raw)
{
    img.clear();

    uint16_t sz = width() * height();
    if (!raw) {
        sz >>= 3;
    }

    Frame res{};
    if (transceive_command(res, CMD_CAPTURE_IMAGE, 0, 0, raw ? 0x20 : 0x00)) {
        uint16_t vlen = (((uint16_t)res[OFFSET_Q1]) << 8) | res[OFFSET_Q2];
        if (vlen == sz) {
            img.resize(vlen + 3);
            if (readWithTransaction(img.data(), img.size()) == m5::hal::error::error_t::OK &&
                is_valid_payload(img.data(), img.size())) {
                img.pop_back();                           // erase TAIL
                img.pop_back();                           // erase CRC
                img.erase(img.begin(), img.begin() + 1);  // erase HEAD
                return true;
            }
        } else {
            M5_LIB_LOGE("Illegal length raw:%u rlen:%u <-> %u", raw, vlen, sz);
        }
    }
    return false;
}

bool UnitFPC1XXX::registerCharacteristic(const uint16_t user_id, const uint8_t permission,
                                         const uint8_t characteristic[193])
{
    if (!is_valid_user_id(user_id)) {
        M5_LIB_LOGE("Illegal user_id %u", user_id);
        return false;
    }
    if (!is_valid_permission(permission)) {
        M5_LIB_LOGE("Illegal permission %u", permission);
        return false;
    }

    std::array<uint8_t, 193 + 3> payload{};
    payload[0] = user_id >> 8;
    payload[1] = user_id & 0xFF;
    payload[2] = permission;
    memcpy(payload.data() + 3, characteristic, 193);
    auto frame = make_variable_frame(CMD_REGISTER_CHARACTERISTIC, 196, payload.data(), payload.size());

    Frame res{};
    res.fill(0xff);

    if (writeWithTransaction(frame.data(), frame.size()) == m5::hal::error::error_t::OK &&
        readWithTransaction(res.data(), res.size()) == m5::hal::error::error_t::OK &&
        res[OFFSET_CMD] == CMD_REGISTER_CHARACTERISTIC && is_valid_sum(res) && is_valid_ACK(res)) {
        return true;
    }
    return false;
}

bool UnitFPC1XXX::verifyCharacteristic(bool& match, const uint16_t user_id, const uint8_t characteristic[193])
{
    match = false;

    if (!is_valid_user_id(user_id)) {
        M5_LIB_LOGE("Illegal user_id %u", user_id);
        return false;
    }

    std::array<uint8_t, 193 + 3> payload{};
    payload[0] = user_id >> 8;
    payload[1] = user_id & 0xFF;
    payload[2] = 0;
    memcpy(payload.data() + 3, characteristic, 193);
    auto frame = make_variable_frame(CMD_VERIFY_CHARACTERISTIC, 196, payload.data(), payload.size());

    Frame res{};
    if (writeWithTransaction(frame.data(), frame.size()) == m5::hal::error::error_t::OK &&
        readWithTransaction(res.data(), res.size()) == m5::hal::error::error_t::OK &&
        res[OFFSET_CMD] == CMD_VERIFY_CHARACTERISTIC && is_valid_sum(res)) {
        match = is_valid_ACK(res);
        return true;
    }
    return false;
}

bool UnitFPC1XXX::identifyCharacteristic(uint16_t& user_id, const uint8_t characteristic[193])
{
    user_id = 0;

    std::array<uint8_t, 193 + 3> payload{};
    memcpy(payload.data() + 3, characteristic, 193);  // [0...2] zero
    auto frame = make_variable_frame(CMD_IDENTIFY_CHARACTERISTIC, 196, payload.data(), payload.size());

    Frame res{};
    if (writeWithTransaction(frame.data(), frame.size()) == m5::hal::error::error_t::OK &&
        readWithTransaction(res.data(), res.size()) == m5::hal::error::error_t::OK &&
        res[OFFSET_CMD] == CMD_IDENTIFY_CHARACTERISTIC && is_valid_sum(res) && is_valid_ACK(res)) {
        user_id = (((uint16_t)res[OFFSET_Q1]) << 8) | (uint16_t)res[OFFSET_Q2];
        return true;
    }
    return false;
}

bool UnitFPC1XXX::compareCharacteristic(bool& match, const uint8_t characteristic[193])
{
    match = false;

    std::array<uint8_t, 193 + 3> payload{};  // [0...2] zero
    memcpy(payload.data() + 3, characteristic, 193);
    auto frame = make_variable_frame(CMD_COMPARE_CHARACTERISTIC, 196, payload.data(), payload.size());

    Frame res{};
    if (writeWithTransaction(frame.data(), frame.size()) == m5::hal::error::error_t::OK &&
        readWithTransaction(res.data(), res.size()) == m5::hal::error::error_t::OK &&
        res[OFFSET_CMD] == CMD_COMPARE_CHARACTERISTIC && is_valid_sum(res)) {
        match = is_valid_ACK(res);
        return true;
    }
    return false;
}

bool UnitFPC1XXX::readSerialNumber(uint32_t& no)
{
    no = 0xdeadbeef;

    Frame res{};
    if (transceive_command(res, CMD_READ_SERIAL_NO)) {
        no = 0;
        no |= ((uint32_t)res[OFFSET_Q1]) << 16;
        no |= ((uint32_t)res[OFFSET_Q2]) << 8;
        no |= ((uint32_t)res[OFFSET_Q3]);
        return true;
    }
    return false;
}

bool UnitFPC1XXX::readVersion(char str[9])
{
    if (str) {
        *str = '\0';

        Frame res{};
        if (transceive_command(res, CMD_READ_VERSION) && is_valid_ACK(res)) {
            uint16_t vlen = (((uint16_t)res[OFFSET_Q1]) << 8) | res[OFFSET_Q2];
            if (vlen == 9) {
                uint8_t vbuf[vlen + 3]{};
                if (readWithTransaction(vbuf, vlen + 3 /* markerx2, crc */) == m5::hal::error::error_t::OK &&
                    is_valid_payload(vbuf, vlen + 3)) {
                    memcpy((uint8_t*)str, vbuf + 1, vlen);
                    return true;
                }
            } else {
                M5_LIB_LOGE("Illegal length %u", vlen);
            }
        }
    }
    return false;
}

bool UnitFPC1XXX::sleep(void)
{
    Frame res{};
    return transceive_command(res, CMD_SLEEP) && is_valid_ACK(res);
}

bool UnitFPC1XXX::transceive_command(Frame& res, const uint8_t cmd, const uint8_t p1, const uint8_t p2,
                                     const uint8_t p3)
{
    Frame frame = make_frame(cmd, p1, p2, p3);

    // m5::utility::log::dump(frame.data(), frame.size());

    if (writeWithTransaction(frame.data(), frame.size()) == m5::hal::error::error_t::OK &&
        readWithTransaction(res.data(), res.size()) == m5::hal::error::error_t::OK && res[OFFSET_CMD] == cmd &&
        is_valid_sum(res)) {
        return true;
    }

    // m5::utility::log::dump(res.data(), res.size());

    return false;
}

// class UnitFPC1020A
const char UnitFPC1020A::name[] = "UnitFPC1020A";
const types::uid_t UnitFPC1020A::uid{"UnitFPC1020A"_mmh3};
const types::attr_t UnitFPC1020A::attr{attribute::AccessUART};

bool UnitFPC1020A::begin()
{
    return UnitFPC1XXX::begin();
}

}  // namespace unit
}  // namespace m5

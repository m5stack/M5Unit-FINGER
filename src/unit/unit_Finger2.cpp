/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_Finger2.cpp
  @brief Finger2 unit for M5UnitUnified
 */
#include "unit_Finger2.hpp"
#include <M5Utility.hpp>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::finger2;
using namespace m5::unit::finger2::command;
using m5::types::big_uint16_t;
using m5::types::big_uint32_t;
using Packet = m5::unit::UnitFinger2::Packet;

namespace {
constexpr uint32_t TIMEOUT_MS{1000 * 4};
constexpr uint32_t AUTO_STEP_TIMEOUT_MS{1000 * 30};
constexpr uint8_t SLEEP_TIME_MIN{10};
constexpr uint8_t SLEEP_TIME_MAX{254};
constexpr uint16_t PACKET_SIZE_MAX{4096};
constexpr uint8_t BUFFER_ID_MIN{1};
constexpr uint8_t BUFFER_ID_MAX{5};
constexpr uint8_t NOTEPAD_PAGE_MAX{15};
constexpr uint8_t SCORE_LEVEL_MIN{1};
constexpr uint8_t SCORE_LEVEL_MAX{5};

constexpr uint16_t PACKET_HEADER{0xEF01};
enum PID : uint8_t {
    PID_CMD = 1,
    PID_DATA,
    PID_ACK = 7,
    PID_TERMINATE_DATA,
};

uint16_t sum16(const uint8_t* p, const uint32_t n)
{
    uint16_t s{};
    for (uint32_t i = 0; i < n; ++i) {
        s += p[i];
    }
    return s;
}

// Response accepts PID_ACK only, data accepts PID_DATA and PID_TERMINATE_DATA too
bool is_valid_packet(const Packet& p, const bool accept_data)
{
    return p.size() >= 9 && p[0] == ((PACKET_HEADER >> 8) & 0xFF) && p[1] == (PACKET_HEADER & 0xFF) &&
           (p[6] == PID_ACK || (accept_data && (p[6] == PID_DATA || p[6] == PID_TERMINATE_DATA)));
}

bool is_known_confirm_code(const uint8_t c)
{
    return c <= m5::stl::to_underlying(ConfirmCode::IllegalData) ||
           c == m5::stl::to_underlying(ConfirmCode::StaticObjectInFeature) ||
           c == m5::stl::to_underlying(ConfirmCode::PoorTemplateQuality) ||
           c >= m5::stl::to_underlying(ConfirmCode::PacketTimeout);
}

// The module waits for the next capture by itself when GenerateCharacteristic fails with these
bool is_retryable_enroll_error(const ConfirmCode c)
{
    return c == ConfirmCode::ImageTooFew || c == ConfirmCode::Unmatched || c == ConfirmCode::MergeFailed ||
           c == ConfirmCode::FeatureAssociated;
}

// Sets the serial timeout while it is alive, and restores it on scope exit
class TimeoutScope {
public:
    TimeoutScope(m5::unit::AdapterUART* ad, const uint32_t timeout_ms, const uint32_t restore_ms)
        : _ad{ad}, _restore_ms{restore_ms}
    {
        if (_ad) {
            _ad->setTimeout(timeout_ms);
        }
    }
    ~TimeoutScope()
    {
        if (_ad) {
            _ad->setTimeout(_restore_ms);
        }
    }

private:
    m5::unit::AdapterUART* _ad{};
    uint32_t _restore_ms{};
};

Packet make_packet(const uint8_t cmd, const uint32_t addr, const uint8_t* payload = nullptr,
                   const uint16_t payload_len = 0)
{
    Packet pkt{};

    if (3 + payload_len > 65535) {
        M5_LIB_LOGE("payload too large %u", payload_len);
        return pkt;
    }

    pkt.resize(12 + payload_len);
    // Header[2]
    pkt[0] = PACKET_HEADER >> 8;
    pkt[1] = PACKET_HEADER & 0xFF;
    // Address[4]
    const big_uint32_t a{addr};
    memcpy(pkt.data() + 2, a.data(), a.size());
    // PID[1]
    pkt[6] = PID_CMD;
    // Length[2]
    const big_uint16_t len{static_cast<uint16_t>(3 /* cmd[1] + sum[2] */ + payload_len)};
    pkt[7] = len.high();
    pkt[8] = len.low();
    // CMD[1]
    pkt[9] = cmd;
    // Payload[n]
    uint16_t offset{10};
    if (payload) {
        memcpy(pkt.data() + offset, payload, payload_len);
        offset += payload_len;
    }

    // Sum[2]
    const big_uint16_t sum{sum16(&pkt[6], pkt.size() - 8)};
    pkt[offset++] = sum.high();
    pkt[offset++] = sum.low();

    return pkt;
}

}  // namespace

namespace m5 {
namespace unit {

// class UnitFinger2
const char UnitFinger2::name[] = "UnitFinger2";
const types::uid_t UnitFinger2::uid{"UnitFinger2"_mmh3};
const types::attr_t UnitFinger2::attr{attribute::AccessUART};

bool UnitFinger2::begin()
{
    auto ad = asAdapter<AdapterUART>(Adapter::Type::UART);
    if (!ad) {
        M5_LIB_LOGE("Illegal adapter");
        return false;
    }
    ad->setTimeout(serial_timeout());
    ad->flushRX();

    // Retry for NessoN1 first-transaction failure (Flash instruction cache timing)
    uint8_t ver{};
    for (int retry = 0; retry < 3; ++retry) {
        if (wakeup() && readFirmwareVersion(ver) && ver != 0x00) {
            break;
        }
        M5_LIB_LOGW("Retry wakeup (%d)", retry);
        m5::utility::delay(100);
        ad->flushRX();
        ver = 0;
    }
    if (ver == 0x00) {
        M5_LIB_LOGE("UnitFinger2 was not detected %02X", ver);
        return false;
    }
    M5_LIB_LOGI("Firmware version: %02X", ver);

    // Set config param
    if (!writeWorkMode(_cfg.work_mode)) {
        M5_LIB_LOGE("Failed to writeWorkMode %u", _cfg.work_mode);
        return false;
    }
    if (!writeSleepTime(_cfg.sleep_time)) {
        M5_LIB_LOGE("Failed to writeSleepTime %u", _cfg.sleep_time);
        return false;
    }

    // Get maximum page id
    SystemBasicParams params{};
    if (!wakeup() || !readSystemParams(params)) {
        M5_LIB_LOGE("Failed to ReadSysPara");
        return false;
    }
    _pageCapacity = params.database_capacity;
    M5_LIB_LOGD("Capacity:%u", _pageCapacity);

    M5_LIB_LOGD("---SystemBasicParams %zu---", sizeof(params));
    M5_LIB_LOGD("Status:  %04X", params.status);
    M5_LIB_LOGD("Type:    %04X", params.sensor_type);
    M5_LIB_LOGD("DBCap:   %04X", params.database_capacity);
    M5_LIB_LOGD("ScoreLv: %04X", params.score_level);
    M5_LIB_LOGD("Address: %08X", params.address);
    M5_LIB_LOGD("PktSz:   %04X", params.packet_size);
    M5_LIB_LOGD("Baud:    %04x", params.baud_rate);

    return true;
}

bool UnitFinger2::readModuleStatus(bool& awake)
{
    awake = false;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_MODULE_STATUS, _address) && pkt.size() == 13) {
        awake = pkt[10];
        return true;
    }
    return false;
}

bool UnitFinger2::readWorkMode(finger2::WorkMode& wm)
{
    wm = WorkMode::ScheduledSleep;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_WORK_MODE, _address) && pkt.size() == 13) {
        const uint8_t v = pkt[10];
        if (v > m5::stl::to_underlying(WorkMode::AlwaysActive)) {
            M5_LIB_LOGE("Illegal work mode %u", v);
            return false;
        }
        wm = static_cast<WorkMode>(v);
        return true;
    }
    return false;
}

bool UnitFinger2::writeWorkMode(const finger2::WorkMode wm)
{
    Packet pkt{};
    return transceive_command8(pkt, CMD_SET_WORK_MODE, _address, m5::stl::to_underlying(wm));
}

bool UnitFinger2::saveWorkMode()
{
    Packet pkt{};
    return transceive_command8(pkt, CMD_SAVE_CONFIGURATION, _address, 0x01 /* save work mode */);
}

bool UnitFinger2::readSystemParams(finger2::SystemBasicParams& params)
{
    params = {};

    Packet pkt{};
    if (transceive_command(pkt, CMD_READ_SYSTEM_PARAMETER, _address) && pkt.size() == 28) {
        params.status            = big_uint16_t(pkt[10], pkt[11]).get();
        params.sensor_type       = big_uint16_t(pkt[12], pkt[13]).get();
        params.database_capacity = big_uint16_t(pkt[14], pkt[15]).get();
        params.score_level       = big_uint16_t(pkt[16], pkt[17]).get();
        params.address           = big_uint32_t(pkt[18], pkt[19], pkt[20], pkt[21]).get();
        params.packet_size       = big_uint16_t(pkt[22], pkt[23]).get();
        params.baud_rate         = big_uint16_t(pkt[24], pkt[25]).get();
        return true;
    }
    return false;
}

bool UnitFinger2::readSleepTime(uint8_t& sec)
{
    sec = 0xFF;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_SLEEP_TIME, _address) && pkt.size() == 13) {
        sec = pkt[10];
        return true;
    }
    return false;
}

bool UnitFinger2::writeSleepTime(const uint8_t sec)
{
    if (sec < SLEEP_TIME_MIN || sec > SLEEP_TIME_MAX) {
        M5_LIB_LOGE("Sleep time must be between %u and %u (%u)", SLEEP_TIME_MIN, SLEEP_TIME_MAX, sec);
        return false;
    }
    Packet pkt{};
    return transceive_command8(pkt, CMD_SET_SLEEP_TIME, _address, sec);
}

bool UnitFinger2::saveSleepTime()
{
    Packet pkt{};
    return transceive_command8(pkt, CMD_SAVE_CONFIGURATION, _address, 0x00 /* save sleep time */);
}

bool UnitFinger2::capture(bool& detected, const bool enroll)
{
    detected = false;

    Packet pkt{};
    if (write_command((enroll ? CMD_GET_ENROLL_IMAGE : CMD_GET_IMAGE), _address)) {
        auto confirm = read_response(pkt);
        if (confirm == ConfirmCode::OK || confirm == ConfirmCode::PassiveActivation ||
            confirm == ConfirmCode::NoFinger) {
            detected = (confirm != ConfirmCode::NoFinger);
            return true;
        }
    }
    return false;
}

bool UnitFinger2::readImageInformation(uint8_t& percentage, bool& quality)
{
    percentage = 0;
    quality    = false;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_IMAGE_INFORMATION, _address) && pkt.size() == 14) {
        percentage = pkt[10];
        quality    = (pkt[11] == 0);
        return true;
    }
    return false;
}

bool UnitFinger2::readImage(std::vector<uint8_t>& img)
{
    img.clear();
    img.reserve((imageWidth() * imageHeight()) >> 1);

    Packet pkt{};
    if (!transceive_command(pkt, CMD_UPLOAD_IMAGE, _address)) {
        return false;
    }

    // Data
    Packet rbuf{};
    for (;;) {
        auto confirm = read_data(rbuf);
        if (!(confirm == PID_DATA || confirm == PID_TERMINATE_DATA)) {
            break;
        }
        // read_data guarantees sz >= 2 (sum[2])
        const uint16_t sz{big_uint16_t(rbuf[7], rbuf[8]).get()};
        img.insert(img.end(), rbuf.begin() + 9, rbuf.begin() + 9 + sz - 2);

        if (confirm == PID_TERMINATE_DATA) {
            return true;
        }
    }
    return false;
}

bool UnitFinger2::generateCharacteristic(const uint8_t buffer_id)
{
    if (buffer_id < BUFFER_ID_MIN || buffer_id > BUFFER_ID_MAX) {
        M5_LIB_LOGE("buffer_id must be between %u and %u (%u)", BUFFER_ID_MIN, BUFFER_ID_MAX, buffer_id);
        return false;
    }

    Packet pkt{};
    return transceive_command8(pkt, CMD_GENERATE_CHARACTERISTIC, _address, buffer_id);
}

bool UnitFinger2::generateTemplate()
{
    Packet pkt{};
    return transceive_command(pkt, CMD_REGISTER_MODEL, _address);
}

bool UnitFinger2::storeTemplate(const uint16_t page_id, const uint8_t buffer_id)
{
    if (buffer_id < BUFFER_ID_MIN || buffer_id > BUFFER_ID_MAX) {
        M5_LIB_LOGE("buffer_id must be between %u and %u (%u)", BUFFER_ID_MIN, BUFFER_ID_MAX, buffer_id);
        return false;
    }
    if (page_id >= capacity()) {
        M5_LIB_LOGE("page_id must be 0 - %u (%u)", capacity() - 1, page_id);
        return false;
    }

    Packet pkt{};
    const big_uint16_t pid{page_id};
    uint8_t params[3]{buffer_id, pid.high(), pid.low()};
    return transceive_command(pkt, CMD_STORE_TEMPLATE, _address, params, sizeof(params));
}

bool UnitFinger2::loadTemplate(const uint8_t buffer_id, const uint16_t page_id)
{
    if (buffer_id < BUFFER_ID_MIN || buffer_id > BUFFER_ID_MAX) {
        M5_LIB_LOGE("buffer_id must be between %u and %u (%u)", BUFFER_ID_MIN, BUFFER_ID_MAX, buffer_id);
        return false;
    }
    if (page_id >= capacity()) {
        M5_LIB_LOGE("page_id must be 0 - %u (%u)", capacity() - 1, page_id);
        return false;
    }

    Packet pkt{};
    const big_uint16_t pid{page_id};
    uint8_t params[3]{buffer_id, pid.high(), pid.low()};
    return transceive_command(pkt, CMD_LOAD_TEMPLATE, _address, params, sizeof(params));
}

bool UnitFinger2::match(bool& matched, uint16_t& score)
{
    matched = false;
    score   = 0;

    Packet pkt{};
    if (write_command(CMD_MATCH, _address)) {
        auto confirm = read_response(pkt);
        if (pkt.size() == 14 && (confirm == ConfirmCode::OK || confirm == ConfirmCode::Unmatched)) {
            matched = (confirm == ConfirmCode::OK);
            score   = big_uint16_t(pkt[10], pkt[11]).get();
            return true;
        }
    }
    return false;
}

bool UnitFinger2::search(bool& matched, uint16_t& matching_page_id, uint16_t& score, const uint8_t buffer_id,
                         const uint16_t start_page, const uint16_t page_num)
{
    matched          = false;
    matching_page_id = 0xFFFF;
    score            = 0;

    if (buffer_id < BUFFER_ID_MIN || buffer_id > BUFFER_ID_MAX) {
        M5_LIB_LOGE("buffer_id must be between %u and %u (%u)", BUFFER_ID_MIN, BUFFER_ID_MAX, buffer_id);
        return false;
    }

    if (!capacity() || start_page >= capacity()) {
        M5_LIB_LOGE("Illegal start_page %u/%u", start_page, capacity());
        return false;
    }
    uint16_t pages = (page_num > 0) ? page_num : capacity() - start_page;
    if (start_page + pages > capacity()) {
        M5_LIB_LOGE("Illegal pages %u %u/%u/%u", pages, start_page, page_num, capacity());
        return false;
    }

    Packet pkt{};
    const big_uint16_t sp{start_page};
    const big_uint16_t np{pages};
    uint8_t params[5]{buffer_id, sp.high(), sp.low(), np.high(), np.low()};

    if (write_command(CMD_SEARCH, _address, params, sizeof(params))) {
        auto confirm = read_response(pkt);
        if (pkt.size() == 16 && (confirm == ConfirmCode::OK || confirm == ConfirmCode::NotFound)) {
            matched          = (confirm == ConfirmCode::OK);
            matching_page_id = big_uint16_t(pkt[10], pkt[11]).get();
            score            = big_uint16_t(pkt[12], pkt[13]).get();
            return true;
        }
    }
    return false;
}

bool UnitFinger2::searchNow(bool& matched, uint16_t& matching_page_id, uint16_t& score, const uint16_t start_page,
                            const uint16_t page_num)
{
    matched          = false;
    matching_page_id = 0xFFFF;
    score            = 0;

    if (!capacity() || start_page >= capacity()) {
        M5_LIB_LOGE("Illegal start_page %u/%u", start_page, capacity());
        return false;
    }
    uint16_t pages = (page_num > 0) ? page_num : capacity() - start_page;
    if (start_page + pages > capacity()) {
        M5_LIB_LOGE("Illegal pages %u %u/%u/%u", pages, start_page, page_num, capacity());
        return false;
    }

    Packet pkt{};
    const big_uint16_t sp{start_page};
    const big_uint16_t np{pages};
    uint8_t params[4]{sp.high(), sp.low(), np.high(), np.low()};

    if (write_command(CMD_SEARCH_NOW, _address, params, sizeof(params))) {
        auto confirm = read_response(pkt);
        if (pkt.size() == 16 && (confirm == ConfirmCode::OK || confirm == ConfirmCode::NotFound)) {
            matched          = (confirm == ConfirmCode::OK);
            matching_page_id = big_uint16_t(pkt[10], pkt[11]).get();
            score            = big_uint16_t(pkt[12], pkt[13]).get();
            return true;
        }
    }
    return false;
}

bool UnitFinger2::readTemplate(uint16_t& actual_size, uint8_t* buf, const uint16_t buf_size, const uint16_t offset)
{
    if (!buf || !buf_size) {
        return false;
    }
    if (buf_size + 14 > PACKET_SIZE_MAX) {
        M5_LIB_LOGE("buf_size too large %u/%u", buf_size, PACKET_SIZE_MAX - 14);
        return false;
    }

    actual_size = 0;

    Packet pkt{};
    const big_uint16_t ofs{offset};
    const big_uint16_t bsz{buf_size};
    uint8_t params[4]{ofs.high(), ofs.low(), bsz.high(), bsz.low()};
    // Header[9] + Confirm[1] + Size[2] + Data[n] + Sum[2]
    if (transceive_command(pkt, CMD_UPLOAD_TEMPLATE, _address, params, sizeof(params)) && pkt.size() >= 14) {
        const uint16_t sz{big_uint16_t(pkt[10], pkt[11]).get()};
        if (sz > buf_size || pkt.size() < 14U + sz) {
            M5_LIB_LOGE("Illegal size %u/%u (%zu)", sz, buf_size, pkt.size());
            return false;
        }
        memcpy(buf, pkt.data() + 12, sz);
        actual_size = sz;
        return true;
    }
    return false;
}

bool UnitFinger2::readTemplateAllBatches(uint16_t& actual_size, uint8_t* buf, const uint16_t buf_size,
                                         const uint16_t batch_size, batch_callback_t callback)
{
    actual_size = 0;
    if (!buf || !buf_size || !batch_size) {
        return false;
    }

    uint16_t offset{};
    uint16_t call_count{};
    while (offset < buf_size) {
        const uint16_t req{std::min<uint16_t>(batch_size, buf_size - offset)};
        uint16_t actual{};
        if (!readTemplate(actual, buf + offset, req, offset)) {
            return false;
        }
        offset += actual;
        actual_size += actual;

        const bool completed{actual < req || offset >= buf_size};
        if (callback && !callback(call_count, actual, batch_size, actual_size, buf_size, completed)) {
            // Abort by callback
            return true;
        }
        ++call_count;

        if (actual < req) {  // No more data
            break;
        }
    }
    return true;
}

bool UnitFinger2::writeTemplate(const uint16_t offset, const uint8_t* buf, const uint16_t buf_size)
{
    if (!buf || !buf_size) {
        return false;
    }
    if (buf_size + 16 > PACKET_SIZE_MAX) {
        M5_LIB_LOGE("buf_size too large %u/%u", buf_size, PACKET_SIZE_MAX - 16);
        return false;
    }

    Packet pkt{};
    std::vector<uint8_t> params{};
    params.resize(4 + buf_size);
    const big_uint16_t ofs{offset};
    const big_uint16_t bsz{buf_size};
    params[0] = ofs.high();
    params[1] = ofs.low();
    params[2] = bsz.high();
    params[3] = bsz.low();
    memcpy(params.data() + 4, buf, buf_size);
    return transceive_command(pkt, CMD_DOWNLOAD_TEMPLATE, _address, params.data(), params.size());
}

bool UnitFinger2::writeTemplateAllBatches(const uint8_t* buf, const uint16_t buf_size, const uint16_t batch_size,
                                          batch_callback_t callback)
{
    if (!buf || !buf_size || !batch_size) {
        return false;
    }

    uint16_t offset{}, call_count{};
    uint16_t remain = buf_size;
    while (offset < buf_size) {
        uint16_t sz = std::min(remain, batch_size);
        if (!writeTemplate(offset, buf + offset, sz)) {
            return false;
        }
        offset += sz;
        remain -= sz;

        if (callback && !callback(call_count, sz, batch_size, offset, buf_size, offset == buf_size)) {
            // Abort by callback
            return true;
        }
        ++call_count;
    }
    return offset == buf_size;
}

bool UnitFinger2::deleteTemplate(const uint16_t page_id, const uint16_t num)
{
    if (page_id >= capacity()) {
        M5_LIB_LOGE("page_id must be 0 - %u (%u)", capacity() - 1, page_id);
        return false;
    }
    if (!num || page_id + num > capacity()) {
        M5_LIB_LOGE("num must be 1 - %u (%u)", capacity() - page_id, num);
        return false;
    }

    Packet pkt{};
    const big_uint16_t pid{page_id};
    const big_uint16_t n{num};
    uint8_t params[4]{pid.high(), pid.low(), n.high(), n.low()};
    return transceive_command(pkt, CMD_DELETE_TEMPLATE, _address, params, sizeof(params));
}

bool UnitFinger2::clear()
{
    Packet pkt{};
    return transceive_command(pkt, CMD_EMPTY, _address);
}

bool UnitFinger2::writeSystemRegister(const finger2::RegisterID reg_id, const uint8_t value)
{
    // Check args
    switch (reg_id) {
        case RegisterID::ScoreLevel:
            if (value < SCORE_LEVEL_MIN || value > SCORE_LEVEL_MAX) {
                M5_LIB_LOGE("ScoreLevel must be %u - %u (%u)", SCORE_LEVEL_MIN, SCORE_LEVEL_MAX, value);
                return false;
            }
            break;
        case RegisterID::PacketSize:
            if (value > 3) {
                M5_LIB_LOGE("PacketSize must be 0 - 3 (%u)", value);
                return false;
            }
            break;
        default:
            M5_LIB_LOGE("Invalid reg_id %u", m5::stl::to_underlying(reg_id));
            return false;
    }

    Packet pkt{};
    uint8_t params[2]{m5::stl::to_underlying(reg_id), value};
    return transceive_command(pkt, CMD_WRITE_REGISTER, _address, params, sizeof(params));
}

bool UnitFinger2::readInformationPage(uint8_t info[512])
{
    if (!info) {
        return false;
    }
    memset(info, 0x00, 512);

    Packet pkt{};
    if (!transceive_command(pkt, CMD_READ_FLASH_INFORMATION, _address)) {
        return false;
    }

    // Data
    Packet rbuf{};
    uint32_t pos{};
    while (pos < 512) {
        auto confirm = read_data(rbuf);
        if (!(confirm == PID_DATA || confirm == PID_TERMINATE_DATA)) {
            break;
        }
        uint16_t sz = big_uint16_t(rbuf[7], rbuf[8]).get();
        if (sz <= 2) {
            break;
        }
        sz -= 2;
        if (pos + sz <= 512) {
            memcpy(info + pos, rbuf.data() + 9, sz);
        }
        pos += sz;
        if (confirm == PID_TERMINATE_DATA) {
            return pos == 512;
        }
    }
    return false;
}

bool UnitFinger2::readValidTemplates(uint16_t& num)
{
    num = 0;

    Packet pkt{};
    if (transceive_command(pkt, CMD_READ_VALID_TEMPLATE_NUMBER, _address) && pkt.size() == 14) {
        num = big_uint16_t(pkt[10], pkt[11]).get();
        return true;
    }
    return false;
}

bool UnitFinger2::readIndexTable(uint8_t table[32])
{
    const auto cap = capacity();

    if (table && cap <= 256) {
        Packet pkt{};
        // NOTE: Only index page 0 (entries 0-255) is read; a 32-byte table covers up to 256 templates
        if (transceive_command8(pkt, CMD_READ_INDEX_TABLE, _address, 0) && pkt.size() == 44) {
            // Copy table and clear exceeding the capacity
            memset(table, 0x00, 32);
            uint32_t copy_len = cap / 8;
            uint8_t remain    = cap & 0x07;
            memcpy(table, pkt.data() + 10, copy_len);
            if (remain) {
                uint8_t mask    = (1U << remain) - 1;
                table[copy_len] = pkt[10 + copy_len] & mask;
            }
            return true;
        }
    }
    return false;
}

bool UnitFinger2::existsTemplate(const uint16_t page_id)
{
    if (page_id >= capacity()) {
        M5_LIB_LOGE("page_id must be 0 - %u (%u)", capacity() - 1, page_id);
        return false;
    }

    uint8_t table[32]{};
    if (readIndexTable(table)) {
        uint32_t idx   = page_id >> 3;
        uint32_t shift = page_id & 0x07;
        return table[idx] & (1U << shift);
    }
    return false;
}

bool UnitFinger2::handshake(bool& status)
{
    status = false;

    Packet pkt{};
    status = transceive_command(pkt, CMD_HANDSHAKE, _address);
    return status;
}

bool UnitFinger2::checkSensor(bool& status)
{
    status = false;

    Packet pkt{};
    status = transceive_command(pkt, CMD_CHECK_SENSOR, _address);
    return status;
}

bool UnitFinger2::writeControlLED(const finger2::LEDMode mode, const finger2::LEDColor clr, const uint8_t cycle,
                                  const finger2::LEDColor eclr)
{
    if (mode < LEDMode::Breath || mode > LEDMode::Fadeout) {
        M5_LIB_LOGE("mode must be between Breath and Fadeout %u", m5::stl::to_underlying(mode));
        return false;
    }

    Packet pkt{};
    uint8_t params[4]{};
    params[0] = m5::stl::to_underlying(mode);
    params[1] = m5::stl::to_underlying(clr);
    params[2] = m5::stl::to_underlying(mode == LEDMode::Breath ? eclr : clr);
    params[3] = cycle;
    return transceive_command(pkt, CMD_CONTROL_BLN, _address, params, sizeof(params));
}

bool UnitFinger2::writeControlLEDRainbow(const uint8_t tm, const LEDColor* colors, const uint8_t colors_num,
                                         const uint8_t cycle)
{
    if (colors && colors_num) {
        Packet pkt{};
        uint8_t params[8]{};
        params[0] = m5::stl::to_underlying(LEDMode::Rainbow);
        params[1] = tm;

        uint8_t c{};
        for (uint_fast8_t i = 0; i < 10; ++i) {
            if (!(i & 1)) {
                // High nibble
                c = (i < colors_num) ? (m5::stl::to_underlying(colors[i]) << 4) | 0x80 : 0x00;
            } else {
                // Low nibble
                c |= (i < colors_num) ? (m5::stl::to_underlying(colors[i]) | 0x08) : 0x00;
                params[2 + i / 2] = c;
            }
        }
        params[7] = cycle;
        return transceive_command(pkt, CMD_CONTROL_BLN, _address, params, sizeof(params));
    }
    return false;
}

bool UnitFinger2::autoEnroll(ConfirmCode& confirm, const uint16_t page_id, const uint8_t capture_times,
                             const finger2::auto_enroll_flag_t flags, auto_enroll_callback_t callback)

{
    if (!capture_times || capture_times > 5) {
        M5_LIB_LOGE("capture times must be between 1 and 5 (%u)", capture_times);
        return false;
    }
    if (page_id >= capacity()) {
        M5_LIB_LOGE("page_id must be 0 - %u (%u)", capacity() - 1, page_id);
        return false;
    }

    Packet pkt{};
    const big_uint16_t pid{page_id};
    const big_uint16_t flg{flags};
    uint8_t params[5]{pid.high(), pid.low(), capture_times, flg.high(), flg.low()};

    AutoEnrollStage stage{};
    uint8_t state{};
    uint8_t process_times{};
    confirm = ConfirmCode::PacketError;
    if (write_command(CMD_AUTO_ENROLL, _address, params, sizeof(params))) {
        // Each response waits for the user operation (placing / releasing a finger)
        TimeoutScope ts(asAdapter<AdapterUART>(Adapter::Type::UART), auto_step_timeout(), serial_timeout());
        // Depending on the flags specified, multiple responses may be returned
        do {
            confirm = read_response(pkt);
            // An OK or PassiveActivation packet without a payload (not size 14) may arrive when passive activation
            if (pkt.size() != 14) {
                if (confirm != ConfirmCode::OK && confirm != ConfirmCode::PassiveActivation) {
                    break;
                }
                continue;
            }

            // 0:Verify command 1:GetEnrollImage 2:GenChar 3:Release finger 4:RegModel 5:Inspection 6:Store
            stage = static_cast<AutoEnrollStage>(pkt[10]);
            state = pkt[11];

            bool abort = (callback && stage < AutoEnrollStage::StoreTemplate)
                             ? !callback(process_times, page_id, confirm, stage, state)
                             : false;
            ++process_times;

            if (abort) {
                confirm = ConfirmCode::OperationBlocked;
                break;
            }
            if (confirm != ConfirmCode::OK &&
                !(stage == AutoEnrollStage::GenerateCharacteristic && is_retryable_enroll_error(confirm))) {
                break;
            }

        } while (stage < AutoEnrollStage::StoreTemplate);

        // Store successful?
        if ((confirm == ConfirmCode::OK) && (stage == AutoEnrollStage::StoreTemplate) && (state == 0xF2)) {
            return true;
        }
    }
    cancel();  // Cancel the ongoing operation
    return false;
}

bool UnitFinger2::autoIdentify(bool& matched, uint16_t& matching_page_id, uint16_t& score, const uint16_t page_id,
                               const uint8_t security_level, const finger2::auto_identify_flag_t flags,
                               auto_identify_callback_t callback)
{
    matched          = false;
    matching_page_id = 0xFFFF;
    score            = 0;

    if (page_id != 0xFFFF && page_id >= capacity()) {
        M5_LIB_LOGE("page_id must be 0 - %u (%u) for Matching", capacity() - 1, page_id);
        return false;
    }
    if (security_level > 1) {
        M5_LIB_LOGE("security_level must be between 0 and 1 (%u)", security_level);
        return false;
    }

    Packet pkt{};
    const big_uint16_t pid{page_id};
    const big_uint16_t flg{flags};
    uint8_t params[5]{security_level, pid.high(), pid.low(), flg.high(), flg.low()};

    AutoIdentifyStage stage{};
    auto confirm = ConfirmCode::PacketError;
    uint8_t process_times{};
    if (write_command(CMD_AUTO_IDENTIFY, _address, params, sizeof(params))) {
        // Each response waits for the user operation (placing a finger)
        TimeoutScope ts(asAdapter<AdapterUART>(Adapter::Type::UART), auto_step_timeout(), serial_timeout());
        // Depending on the flags specified, multiple responses may be returned
        do {
            confirm = read_response(pkt);
            // An OK or PassiveActivation packet without a payload (not size 17) may arrive when passive activation
            if (pkt.size() != 17) {
                if (confirm != ConfirmCode::OK && confirm != ConfirmCode::PassiveActivation) {
                    break;
                }
                continue;
            }
            // 0:Verify command 1:GetImage 5:Registered fingerprint comparison
            stage = static_cast<AutoIdentifyStage>(pkt[10]);

            bool abort =
                (callback && (stage == AutoIdentifyStage::VerifyCommand || stage == AutoIdentifyStage::GetImage))
                    ? !callback(process_times, confirm, stage)
                    : false;

            if (abort || confirm != ConfirmCode::OK) {
                confirm = abort ? ConfirmCode::OperationBlocked : confirm;
                break;
            }

            ++process_times;
        } while (stage < AutoIdentifyStage::Result);

        // Identify successful?
        if ((stage == AutoIdentifyStage::Result) &&
            (confirm == ConfirmCode::OK || confirm == ConfirmCode::NotFound || confirm == ConfirmCode::Unmatched)) {
            matched          = (confirm == ConfirmCode::OK);
            matching_page_id = big_uint16_t(pkt[11], pkt[12]).get();
            score            = big_uint16_t(pkt[13], pkt[14]).get();
            return true;
        }
    }
    cancel();  // Cancel the ongoing operation
    return false;
}

bool UnitFinger2::cancel()
{
    Packet pkt{};
    return transceive_command(pkt, CMD_CANCEL, _address);
}

bool UnitFinger2::readNotepad(uint8_t buf[32], const uint8_t page)
{
    if (page > NOTEPAD_PAGE_MAX) {
        M5_LIB_LOGE("Page must be between 0 and %u (%u)", NOTEPAD_PAGE_MAX, page);
        return false;
    }

    Packet pkt{};
    if (buf && transceive_command8(pkt, CMD_READ_NOTEPAD, _address, page) && pkt.size() == 44) {
        memcpy(buf, pkt.data() + 10, 32);
        return true;
    }
    return false;
}

bool UnitFinger2::writeNotepad(const uint8_t page, const uint8_t* buf, const uint8_t len)
{
    if (page > NOTEPAD_PAGE_MAX) {
        M5_LIB_LOGE("Page must be between 0 and %u (%u)", NOTEPAD_PAGE_MAX, page);
        return false;
    }
    if (!buf || len == 0) {
        return false;
    }

    uint8_t wbuf[32 + 1]{};  // page + buf
    wbuf[0] = page;
    memcpy(wbuf + 1, buf, std::min<uint8_t>(len, 32));

    Packet pkt{};
    return transceive_command(pkt, CMD_WRITE_NOTEPAD, _address, wbuf, sizeof(wbuf));
}

bool UnitFinger2::readRandomNumber(uint32_t& value)
{
    value = 0;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_RANDOM, _address) && pkt.size() == 16) {
        value = big_uint32_t(pkt[10], pkt[11], pkt[12], pkt[13]).get();
        return true;
    }
    return false;
}

bool UnitFinger2::wakeup()
{
    Packet pkt{};
    return transceive_command(pkt, CMD_ACTIVATE_MODULE, _address);
}

bool UnitFinger2::readSerialNumber(uint8_t sn[32])
{
    Packet pkt{};
    if (sn && transceive_command8(pkt, CMD_GET_SERIAL, _address, 0 /* reserved */) && pkt.size() == 44) {
        memcpy(sn, pkt.data() + 10, 32);
        return true;
    }
    return false;
}

bool UnitFinger2::readFirmwareVersion(uint8_t& ver)
{
    ver = 0x00;
    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_FIRMWARE_VERSION, _address) && pkt.size() == 13) {
        ver = pkt[10];
        return true;
    }
    return false;
}

bool UnitFinger2::findLowestAvailablePage(uint16_t& page)
{
    page = 0xFFFF;

    uint16_t no{0xFFFF};
    uint8_t table[32]{};
    auto cap          = capacity();
    uint_fast8_t eidx = std::min(32, (cap >> 3) + 1);
    if (!cap || !eidx) {
        M5_LIB_LOGE("Illegal status %u:%u", cap, eidx);
        return false;
    }

    if (readIndexTable(table)) {
        for (uint_fast8_t i = 0; i < eidx; ++i) {
            if (table[i] == 0xFF) {
                continue;
            }
            uint16_t v = table[i];
            auto pos   = __builtin_ctz(++v);
            if (pos < 8) {
                no = 8 * i + pos;
                if (no < cap) {
                    break;
                }
            }
            no = 0xFFFF;
        }
        page = no;
        return true;
    }
    return false;
}

bool UnitFinger2::findHighestAvailablePage(uint16_t& page)
{
    page = 0xFFFF;

    uint16_t no{0xFFFF};
    uint8_t table[32]{};

    auto cap         = capacity();
    uint8_t remain   = (cap & 0x07);
    int_fast8_t sidx = (cap >> 3) - (remain == 0);

    if (!cap || sidx < 0) {
        M5_LIB_LOGE("Illegal status %u:%d", cap, sidx);
        return false;
    }
    if (readIndexTable(table)) {
        if (remain) {
            uint8_t mask = (1U << remain) - 1;
            uint8_t inv  = ~table[sidx] & mask;
            if (inv) {
                auto pos = 7 - (__builtin_clz(inv) - (sizeof(unsigned int) * 8 - 8));
                no       = 8 * sidx + pos;
                if (no < cap) {
                    page = no;
                    return true;
                }
            }
            --sidx;
        }

        for (int_fast8_t i = sidx; i >= 0; --i) {
            if (table[i] == 0xFF) {
                continue;
            }
            uint8_t inv = ~table[i];  // Always non-zero since 0xFF is skipped above
            if (inv) {
                auto pos = 7 - (__builtin_clz(inv) - (sizeof(unsigned int) * 8 - 8));
                no       = 8 * i + pos;
                if (no < cap) {
                    break;
                }
            }
            no = 0xFFFF;
        }
        page = no;
        return true;
    }
    return false;
}

uint32_t UnitFinger2::serial_timeout() const
{
    return _cfg.timeout_ms ? _cfg.timeout_ms : TIMEOUT_MS;
}

uint32_t UnitFinger2::auto_step_timeout() const
{
    return _cfg.auto_step_timeout_ms ? _cfg.auto_step_timeout_ms : AUTO_STEP_TIMEOUT_MS;
}

//
bool UnitFinger2::write_command(const uint8_t cmd, const uint32_t addr, const uint8_t* payload,
                                const uint16_t payload_len)
{
    auto pkt = make_packet(cmd, addr, payload, payload_len);
    // Discard stale data (e.g. rest of a broken frame, unsolicited passive activation packet) to keep in sync
    auto ad = asAdapter<AdapterUART>(Adapter::Type::UART);
    if (ad) {
        ad->flushRX();
    }
    return writeWithTransaction(pkt.data(), pkt.size()) == m5::hal::error::error_t::OK;
}

// Return PID
uint8_t UnitFinger2::read_data(Packet& rbuf)
{
    rbuf.clear();
    rbuf.reserve(256);
    rbuf.resize(9);
    if (readWithTransaction(rbuf.data(), rbuf.size()) == m5::hal::error::error_t::OK && is_valid_packet(rbuf, true)) {
        // Length: Data[n] + Sum[2]
        const uint16_t sz{big_uint16_t(rbuf[7], rbuf[8]).get()};
        if (sz < 2 || sz > PACKET_SIZE_MAX - 9) {
            M5_LIB_LOGE("Illegal length %u", sz);
            return 0x00;
        }
        rbuf.resize(rbuf.size() + sz);
        if (readWithTransaction(rbuf.data() + 9, sz) == m5::hal::error::error_t::OK) {
            const auto sum = sum16(rbuf.data() + 6, rbuf.size() - 8);
            const uint16_t rsum{big_uint16_t(rbuf[rbuf.size() - 2], rbuf[rbuf.size() - 1]).get()};
            return sum == rsum ? rbuf[6] : 0x00;
        }
    }
    return 0x00;
}

// Return Confirm code
ConfirmCode UnitFinger2::read_response(Packet& rbuf)
{
    rbuf.clear();
    rbuf.reserve(128);
    rbuf.resize(9);
    if (readWithTransaction(rbuf.data(), rbuf.size()) == m5::hal::error::error_t::OK && is_valid_packet(rbuf, false)) {
        // Length: Confirm[1] + Params[n] + Sum[2]
        const uint16_t sz{big_uint16_t(rbuf[7], rbuf[8]).get()};
        if (sz < 3 || sz > PACKET_SIZE_MAX - 9) {
            M5_LIB_LOGE("Illegal length %u", sz);
            return ConfirmCode::PacketError;
        }
        rbuf.resize(rbuf.size() + sz);
        if (readWithTransaction(rbuf.data() + 9, sz) == m5::hal::error::error_t::OK) {
            const auto sum = sum16(rbuf.data() + 6, rbuf.size() - 8);
            const uint16_t rsum{big_uint16_t(rbuf[rbuf.size() - 2], rbuf[rbuf.size() - 1]).get()};

            M5_LIB_LOGV("CONFIRM:%02X Sum:%04X/%04X", rbuf[9], sum, rsum);

            if (sum == rsum) {
                const uint8_t c = rbuf[9];
                if (!is_known_confirm_code(c)) {
                    M5_LIB_LOGE("Unknown confirm code %02X", c);
                    return ConfirmCode::PacketError;
                }
                return static_cast<ConfirmCode>(c);
            }
        }
    }

    M5_DUMPV(rbuf.data(), rbuf.size());
    return ConfirmCode::PacketError;
}

// Only confirm code 0x00 and 0xFF is permitted
bool UnitFinger2::transceive_command(Packet& rbuf, const uint8_t cmd, const uint32_t addr, const uint8_t* payload,
                                     const uint16_t payload_len)
{
    if (write_command(cmd, addr, payload, payload_len)) {
        auto confirm = read_response(rbuf);
        return confirm == ConfirmCode::OK || confirm == ConfirmCode::PassiveActivation;
    }
    return false;
}

// Only confirm code 0x00 and 0xFF is permitted
bool UnitFinger2::transceive_command8(Packet& rbuf, const uint8_t cmd, const uint32_t addr, const uint8_t value)
{
    return transceive_command(rbuf, cmd, addr, &value, 1);
}

}  // namespace unit
}  // namespace m5

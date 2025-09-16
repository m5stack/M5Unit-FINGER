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
using Packet = m5::unit::UnitFinger2::Packet;

namespace {
constexpr uint32_t TIMEOUT_MS{1000 * 4};
constexpr uint8_t SLEEP_TIME_MIN{10};
constexpr uint8_t SLEEP_TIME_MAX{254};
constexpr uint16_t PACKET_SIZE_MAX{4096};
constexpr uint8_t BUFFER_ID_MIN{1};
constexpr uint8_t BUFFER_ID_MAX{5};

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

bool is_valid_ack(const Packet& p)
{
    return p.size() >= 7 && p[0] == ((PACKET_HEADER >> 8) & 0xFF) && p[1] == (PACKET_HEADER & 0xFF) &&
           (p[6] == PID_ACK || p[6] == PID_DATA || p[6] == PID_TERMINATE_DATA);
}

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
    pkt[2] = addr >> 24;
    pkt[3] = addr >> 16;
    pkt[4] = addr >> 8;
    pkt[5] = addr & 0xFF;
    // PID[1]
    pkt[6] = PID_CMD;
    // Length[2]
    uint16_t len = 3 /* cmd[1] + sum[2] */ + payload_len;
    pkt[7]       = len >> 8;
    pkt[8]       = len & 0xFF;
    // CMD[1]
    pkt[9] = cmd;
    // Payload[n]
    uint16_t offset{10};
    if (payload) {
        memcpy(pkt.data() + offset, payload, payload_len);
        offset += payload_len;
    }

    // Sum[2]
    const auto sum = sum16(&pkt[6], pkt.size() - 8);
    pkt[offset++]  = sum >> 8;
    pkt[offset++]  = sum & 0xFF;

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
    ad->setTimeout(_cfg.timeout_ms ? _cfg.timeout_ms : TIMEOUT_MS);

#if 0    
    // ad->flushRX();
    auto serial = ad->impl()->getSerial();
    if (!serial) {
        M5_LIB_LOGE("Serial not available");
        return false;
    }

    auto timeout_at = m5::utility::millis() + 2000;
    bool beacon{};
    do {
        while (serial->available()) {
            auto d = serial->read();
            beacon |= (d == 0x55);
            M5_LIB_LOGE(">>>>> [%02X]", d);
        }
    } while (!beacon && m5::utility::millis() <= timeout_at);
    if (!beacon) {
        M5_LIB_LOGE("Failed to detect the startup of the unit");
        return false;
    }
#endif

#if 0    
    bool awake{};
    auto timeout_at = m5::utility::millis() + 2000;
    do {
        wakeup();
        readModuleStatus(awake);
        m5::utility::delay(400);
    } while (!awake && m5::utility::millis() <= timeout_at);
    if (!awake) {
        M5_LIB_LOGE("Unit NOT awaken");
        return false;
    }
#endif
    uint8_t ver{};
    if (!readFirmwareVersion(ver) || ver == 0x00) {
        M5_LIB_LOGE("UnitFinger2 was not deteced %02X", ver);
        return false;
    }
    M5_LIB_LOGI("Firmware version: %02X", ver);

    // Set config param
    if (!writeWorkMode(_cfg.work_mode)) {
        M5_LIB_LOGE("Failed to writeWorkMode");
        return false;
    }

    if (!wakeup() || !writeSystemRegister(RegisterID::ScoreLevel, _cfg.score_level)) {
        M5_LIB_LOGE("Failed to writeSystemRegister");
        return false;
    }

    // Get maximum page id
    SystemBasicParams params{};
    if (!wakeup() || !readSystemParams(params)) {
        M5_LIB_LOGE("Failed to ReadSysPara");
        return false;
    }
    _pageCapacity = params.capacity;
    M5_LIB_LOGD("Capacity:%u", _pageCapacity);

    M5_LIB_LOGD("---SystemBasicParams %zu---", sizeof(params));
    M5_LIB_LOGD("Status:  %04X", params.status);
    M5_LIB_LOGD("TmpSize: %04X", params.template_size);
    M5_LIB_LOGD("Capacity:%04X", params.capacity);
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
    if (transceive_command(pkt, CMD_GET_MODULE_STATUS, _address)) {
        awake = pkt[10];
        return true;
    }
    return false;
}

bool UnitFinger2::readWorkMode(finger2::WorkMode& wm)
{
    wm = WorkMode::ScheduledSleep;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_WORK_MODE, _address)) {
        wm = static_cast<WorkMode>(pkt[10]);
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
        params.status        = ((uint16_t)pkt[10] << 8) | pkt[11];
        params.template_size = ((uint16_t)pkt[12] << 8) | pkt[13];
        params.capacity      = ((uint16_t)pkt[14] << 8) | pkt[15];
        params.score_level   = ((uint16_t)pkt[16] << 8) | pkt[17];
        params.address     = ((uint16_t)pkt[18] << 24) | ((uint16_t)pkt[19] << 16) | ((uint16_t)pkt[20] << 8) | pkt[21];
        params.packet_size = ((uint16_t)pkt[22] << 8) | pkt[23];
        params.baud_rate   = ((uint16_t)pkt[24] << 8) | pkt[25];
        return true;
    }
    return false;
}

bool UnitFinger2::readSleepTime(uint8_t& sec)
{
    sec = 0xFF;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_SLEEP_TIME, _address)) {
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

bool UnitFinger2::readImageInformation(uint8_t& percentage, bool& quarity)
{
    percentage = 0;
    quarity    = false;

    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_IMAGE_INFORMATION, _address) && pkt.size() == 14) {
        percentage = pkt[10];
        quarity    = (pkt[11] == 0);
        return true;
    }
    return false;
}

bool UnitFinger2::readImage(std::vector<uint8_t>& img)
{
    img.clear();

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
        uint16_t sz = (((uint16_t)rbuf[7]) << 8) | (uint16_t)rbuf[8];
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

bool UnitFinger2::storeTemplate(const uint8_t page_id, const uint8_t buffer_id)
{
    if (buffer_id < BUFFER_ID_MIN || buffer_id > BUFFER_ID_MAX) {
        M5_LIB_LOGE("buffer_id must be between %u and %u (%u)", BUFFER_ID_MIN, BUFFER_ID_MAX, buffer_id);
        return false;
    }

    Packet pkt{};
    uint8_t params[3]{};
    params[0] = buffer_id;
    params[1] = page_id >> 8;
    params[2] = page_id & 0xFF;
    return transceive_command(pkt, CMD_STORE_CHARACTERISTIC, _address, params, sizeof(params));
}

bool UnitFinger2::loadTemplate(const uint8_t buffer_id, const uint16_t page_id)
{
    if (buffer_id < BUFFER_ID_MIN || buffer_id > BUFFER_ID_MAX) {
        M5_LIB_LOGE("buffer_id must be between %u and %u (%u)", BUFFER_ID_MIN, BUFFER_ID_MAX, buffer_id);
        return false;
    }

    Packet pkt{};
    uint8_t params[3]{};
    params[0] = buffer_id;
    params[1] = page_id >> 8;
    params[2] = page_id & 0xFF;
    return transceive_command(pkt, CMD_LOAD_CHARACTERISTIC, _address, params, sizeof(params));
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
            score   = (((uint16_t)pkt[10]) << 8) | ((uint16_t)pkt[11]);
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

    uint8_t pages = (page_num > 0) ? page_num : capacity() - start_page - 1;
    if (!capacity() || pages > capacity() - 1) {
        M5_LIB_LOGE("Illegal pages %u %u/%u/%u", pages, start_page, page_num, capacity());
        return false;
    }

    Packet pkt{};
    uint8_t params[5]{};
    params[0] = buffer_id;
    params[1] = start_page >> 8;
    params[2] = start_page & 0xFF;
    params[3] = pages >> 8;
    params[4] = pages & 0xFF;

    if (write_command(CMD_SEARCH, _address, params, sizeof(params))) {
        auto confirm = read_response(pkt);
        if (pkt.size() == 16 && (confirm == ConfirmCode::OK || confirm == ConfirmCode::NotFound)) {
            matched          = (confirm == ConfirmCode::OK);
            matching_page_id = (((uint16_t)pkt[10]) << 8) | ((uint16_t)pkt[11]);
            score            = (((uint16_t)pkt[12]) << 8) | ((uint16_t)pkt[13]);
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

    uint8_t pages = (page_num > 0) ? page_num : capacity() - start_page - 1;
    if (!capacity() || pages > capacity() - 1) {
        M5_LIB_LOGE("Illegal pages %u %u/%u/%u", pages, start_page, page_num, capacity());
        return false;
    }

    Packet pkt{};
    uint8_t params[4]{};
    params[0] = start_page >> 8;
    params[1] = start_page & 0xFF;
    params[2] = pages >> 8;
    params[3] = pages & 0xFF;

    if (write_command(CMD_SEARCH_NOW, _address, params, sizeof(params))) {
        auto confirm = read_response(pkt);
        if (pkt.size() == 16 && (confirm == ConfirmCode::OK || confirm == ConfirmCode::NotFound)) {
            matched          = (confirm == ConfirmCode::OK);
            matching_page_id = (((uint16_t)pkt[10]) << 8) | ((uint16_t)pkt[11]);
            score            = (((uint16_t)pkt[12]) << 8) | ((uint16_t)pkt[13]);
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
    uint8_t params[4]{};
    params[0] = offset >> 8;
    params[1] = offset & 0xFF;
    params[2] = buf_size >> 8;
    params[3] = buf_size & 0xFF;
    if (transceive_command(pkt, CMD_UPLOAD_TEMPLATE, _address, params, sizeof(params)) && pkt.size() >= 13) {
        actual_size = ((uint16_t)pkt[10] << 8) | pkt[11];
        if (actual_size > buf_size) {
            actual_size = buf_size;
        }
        memcpy(buf, pkt.data() + 12, actual_size);
        return true;
    }
    return false;
}

bool UnitFinger2::readTemplateAllBatches(uint16_t& actual_size, uint8_t* buf, const uint16_t buf_size,
                                         const uint16_t batch_size)
{
    if (!buf || !buf_size || !batch_size) {
        return false;
    }

    uint16_t offset{};
    while (offset < buf_size) {
        uint16_t actual{};
        if (!readTemplate(actual, buf + offset, batch_size, offset)) {
            return false;
        }
        offset += actual;
        actual_size += actual;
        if (actual < batch_size) {  // No more data
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
    params[0] = offset >> 8;
    params[1] = offset & 0xFF;
    params[2] = buf_size >> 8;
    params[3] = buf_size & 0xFF;
    memcpy(params.data() + 4, buf, buf_size);
    return transceive_command(pkt, CMD_DOWNLOAD_TEMPLATE, _address, params.data(), params.size());
}

bool UnitFinger2::writeTemplateAllBatches(const uint8_t* buf, const uint16_t buf_size, const uint16_t batch_size)
{
    if (!buf || !buf_size || !batch_size) {
        return false;
    }

    uint16_t offset{};
    uint16_t remain = buf_size;
    while (offset < buf_size) {
        uint16_t sz = std::min(remain, batch_size);
        if (!writeTemplate(offset, buf + offset, sz)) {
            return false;
        }
        offset += sz;
        remain -= sz;
    }
    return offset == buf_size;
}

bool UnitFinger2::deleteTemplate(const uint16_t page_id, const uint16_t num)
{
    Packet pkt{};
    uint8_t params[4]{};
    params[0] = page_id >> 8;
    params[1] = page_id & 0xFF;
    params[2] = num >> 8;
    params[3] = num & 0xFF;
    return transceive_command(pkt, CMD_DELETE_CHARACTERISTIC, _address, params, sizeof(params));
}

bool UnitFinger2::clear()
{
    Packet pkt{};
    return transceive_command(pkt, CMD_EMPTY, _address);
}

bool UnitFinger2::writeSystemRegister(const finger2::RegisterID reg_id, const uint8_t value)
{
    auto reg = m5::stl::to_underlying(reg_id);
    if (reg == m5::stl::to_underlying(RegisterID::Prohibited) || reg > 11) {
        M5_LIB_LOGE("Invalid reg_id %u", reg_id);
        return false;
    }

    if (reg >= 10) {
        reg = 0x10 + (reg - 10);  // DEC 10,11... => HEX 0x10, 0x11,...
    }

    Packet pkt{};
    uint8_t params[2]{reg, value};
    return transceive_command(pkt, CMD_WRITE_REGISTER, _address, params, sizeof(params));
}

bool UnitFinger2::readInformationPage(uint8_t inf[512])
{
    if (!inf) {
        return false;
    }
    memset(inf, 0x00, 512);

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
        uint16_t sz = (((uint16_t)rbuf[7]) << 8) | (uint16_t)rbuf[8];
        memcpy(inf + pos, rbuf.data() + 9, sz - 2);
        pos += sz - 2;
        if (confirm == PID_TERMINATE_DATA) {
            return true;
        }
    }
    return false;
}

bool UnitFinger2::readValidTemplates(uint16_t& num)
{
    num = 0;

    Packet pkt{};
    if (transceive_command(pkt, CMD_READ_VALID_TEMPLATE_NUMBER, _address) && pkt.size() == 14) {
        num = (((uint16_t)pkt[10]) << 8) | ((uint16_t)pkt[11]);
        return true;
    }
    return false;
}

bool UnitFinger2::readIndexTable(uint8_t table[32])
{
    const auto cap = capacity();

    if (table && cap / 8 <= 32) {
        Packet pkt{};
        if (transceive_command8(pkt, CMD_READ_INDEX_TABLE, _address, 0 /* only zero */) && pkt.size() == 44) {
            // Copy table and clear exceeding the capacity
            uint32_t copy_len = cap / 8;
            uint8_t remain    = cap & 0x07;
            memcpy(table, pkt.data() + 10, copy_len);
            if (remain) {
                uint8_t mask    = (1U << remain) - 1;
                table[copy_len] = pkt[10 + copy_len] & mask;
            }
            if (copy_len + 1 < 32) {
                memset(&table[copy_len + 1], 0x00, 32 - (copy_len + 1));
            }
            return true;
        }
    }
    return false;
}

bool UnitFinger2::existsTemplate(const uint8_t page_id)
{
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
    if (mode < LEDMode::Bleath || mode > LEDMode::Fadeout) {
        M5_LIB_LOGE("mode must be between Bleath and Fadeout %u", mode);
        return false;
    }

    Packet pkt{};
    uint8_t params[4]{};
    params[0] = m5::stl::to_underlying(mode);
    params[1] = m5::stl::to_underlying(clr);
    params[2] = m5::stl::to_underlying(mode == LEDMode::Bleath ? eclr : clr);
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

    Packet pkt{};
    uint8_t params[5]{};
    params[0] = page_id >> 8;
    params[1] = page_id & 0xFF;
    params[2] = capture_times;
    params[3] = flags >> 8;
    params[4] = flags & 0xFF;

    AutoEnrollStage stage{};
    uint8_t state{};
    confirm = ConfirmCode::PacketError;
    if (write_command(CMD_AUTO_ENROLL, _address, params, sizeof(params))) {
        // Depending on the flags specified, multiple responses may be returned
        do {
            confirm = read_response(pkt);
            // An OK packet without a payload (not size 14) may arrive when passive activation
            if (pkt.size() != 14) {
                if (confirm != ConfirmCode::OK) {
                    break;
                }
                continue;
            }

            // 0:Verify command 1:GetEnrollImage 2:GenChar 3:Release finger 4:RegModel 5:Inspection 6:Store
            stage = static_cast<AutoEnrollStage>(pkt[10]);
            state = pkt[11];

            bool abort =
                (stage < AutoEnrollStage::StoreTemplate && callback) ? !callback(confirm, stage, state) : false;

            // M5_LIB_LOGD(">>>> Confirm:%02x Stage:%02u State:%02X abort:%u", confirm, stage, state, abort);
            if (abort || confirm != ConfirmCode::OK) {
                confirm = abort ? ConfirmCode::OperationBlocked : confirm;
                break;
            }

        } while (stage < AutoEnrollStage::StoreTemplate);

        // Store successful?
        if ((confirm == ConfirmCode::OK) && (stage == AutoEnrollStage::StoreTemplate) && (state == 0xF2)) {
            return true;
        }
    }
    cancel();  // Cancel and purge packets
    return false;
}

bool UnitFinger2::autoIdentify(bool& matched, uint16_t& matching_page_id, uint16_t& score, uint16_t page_id,
                               const uint8_t security_level, const finger2::auto_identify_flag_t flags,
                               auto_identify_callback_t callback)
{
    matched          = false;
    matching_page_id = 0xFFFF;
    score            = 0;

    if (security_level > 1) {
        M5_LIB_LOGE("security_level must be between 0 and 1 (%u)", security_level);
        return false;
    }

    Packet pkt{};
    uint8_t params[5]{};
    params[0] = security_level;
    params[1] = page_id >> 8;
    params[2] = page_id & 0xFF;
    params[3] = flags >> 8;
    params[4] = flags & 0xFF;

    AutoIdentifyStage stage{};
    auto confirm = ConfirmCode::PacketError;
    if (write_command(CMD_AUTO_IDENTIFY, _address, params, sizeof(params))) {
        // Depending on the flags specified, multiple responses may be returned
        do {
            confirm = read_response(pkt);
            // An OK packet without a payload (not size 17) may arrive when passive activation
            if (pkt.size() != 17) {
                if (confirm != ConfirmCode::OK) {
                    break;
                }
                continue;
            }
            // 0:Verify command 1:GetImage 5:Registered fingerprint comparison
            stage = static_cast<AutoIdentifyStage>(pkt[10]);

            bool abort = (stage < AutoIdentifyStage::Result && callback) ? !callback(confirm, stage) : false;

            // M5_LIB_LOGD(">>>> Confirm:%02x Stage:%02u abort:%u", confirm, stage, abort);
            if (abort || confirm != ConfirmCode::OK) {
                confirm = abort ? ConfirmCode::OperationBlocked : confirm;
                break;
            }
        } while (stage < AutoIdentifyStage::Result);

        // Identify successful?
        if ((stage == AutoIdentifyStage::Result) &&
            (confirm == ConfirmCode::OK || confirm == ConfirmCode::NotFound || confirm == ConfirmCode::Unmatched)) {
            matched          = (confirm == ConfirmCode::OK);
            matching_page_id = ((uint16_t)pkt[11] << 8) | pkt[12];
            score            = ((uint16_t)pkt[13] << 8) | pkt[14];
            return true;
        }
    }
    cancel();  // Cancel and purge packets
    return false;
}

bool UnitFinger2::cancel()
{
    Packet pkt{};
    return transceive_command(pkt, CMD_CANCEL, _address);
}

bool UnitFinger2::readNotepad(uint8_t buf[32], const uint8_t page)
{
    if (page > 7) {
        M5_LIB_LOGE("Page must be between 0 and 7 (%u)", page);
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
    if (page > 7) {
        M5_LIB_LOGE("Page must be between 0 and 7 (%u)", page);
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
        value = (((uint32_t)pkt[10]) << 24) | (((uint32_t)pkt[11]) << 16) | (((uint32_t)pkt[12]) << 8) |
                ((uint32_t)pkt[13]);
        return true;
    }
    return false;
}

bool UnitFinger2::wakeup()
{
    Packet pkt{};
    if (write_command(CMD_ACTIVATE_MODULE, _address)) {
        auto confirm = read_response(pkt);
        return (confirm == ConfirmCode::OK || confirm == ConfirmCode::PassiveActivation);
    }
    return false;
}

bool UnitFinger2::readSerialNumber(uint8_t sn[32])
{
    Packet pkt{};
    if (sn && transceive_command(pkt, CMD_GET_SERIAL, _address) && pkt.size() == 44) {
        memcpy(sn, pkt.data() + 10, 32);
        return true;
    }
    return false;
}

bool UnitFinger2::readFirmwareVersion(uint8_t& ver)
{
    ver = 0x00;
    Packet pkt{};
    if (transceive_command(pkt, CMD_GET_FIRMWRE_VERSION, _address)) {
        ver = pkt[10];
        return true;
    }
    return false;
}

uint16_t UnitFinger2::findLowestAvailablePage()
{
    uint16_t no{0xFFFF};
    uint8_t table[32]{};
    auto cap          = capacity();
    uint_fast8_t eidx = std::min(32, (cap >> 3) + 1);
    if (cap && eidx && readIndexTable(table)) {
        for (uint_fast8_t i = 0; i < eidx; ++i) {
            if (table[i] == 0xFF) {
                continue;
            }
            uint16_t v = table[i];
            auto pos   = __builtin_ctz(++v);

            no = 8 * i + pos;
            if (no >= cap) {
                no = 0xFFFF;
            }
            break;
        }
    }
    return no;
}

uint16_t UnitFinger2::findHighestAvailablePage()
{
    uint16_t no{0xFFFF};
    uint8_t table[32]{};

    auto cap = capacity();
    //    int_fast8_t sidx = (cap >> 3) + ((cap & 0x07) != 0);
    uint8_t remain   = (cap & 0x07);
    int_fast8_t sidx = (cap >> 3) - (remain == 0);

    if (cap && sidx >= 0 && readIndexTable(table)) {
        if (remain) {
            uint8_t mask = (1U << remain) - 1;
            uint8_t inv  = ~table[sidx] & mask;
            auto pos     = 7 - (__builtin_clz(inv) - (sizeof(unsigned int) * 8 - 8));
            no           = 8 * sidx + pos;
            if (no < cap) {
                return no;
            }
            --sidx;
        }

        for (int_fast8_t i = sidx; i >= 0; --i) {
            if (table[i] == 0xFF) {
                continue;
            }

            uint8_t inv = ~table[i];
            auto pos    = 7 - (__builtin_clz(inv) - (sizeof(unsigned int) * 8 - 8));
            no          = 8 * i + pos;
            if (no < cap) {
                break;
            }
            no = 0xFFFF;
        }
    }
    return no;
}

//
bool UnitFinger2::write_command(const uint8_t cmd, const uint32_t addr, const uint8_t* payload,
                                const uint16_t payload_len)
{
    auto pkt = make_packet(cmd, addr, payload, payload_len);
    // M5_LIB_LOGD(">>>> SEND");
    // m5::utility::log::dump(pkt.data(), pkt.size(), false);
    return writeWithTransaction(pkt.data(), pkt.size()) == m5::hal::error::error_t::OK;
}

// Return PID
uint8_t UnitFinger2::read_data(Packet& rbuf)
{
    rbuf.clear();
    rbuf.reserve(256);
    rbuf.resize(9);
    if (readWithTransaction(rbuf.data(), rbuf.size()) == m5::hal::error::error_t::OK && is_valid_ack(rbuf)) {
        uint16_t sz = (((uint16_t)rbuf[7]) << 8) | (uint16_t)rbuf[8];
        rbuf.resize(rbuf.size() + sz);
        if (readWithTransaction(rbuf.data() + 9, sz) == m5::hal::error::error_t::OK) {
            auto sum      = sum16(rbuf.data() + 6, rbuf.size() - 8);
            uint16_t rsum = (rbuf[rbuf.size() - 2] << 8) | rbuf[rbuf.size() - 1];
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
    if (readWithTransaction(rbuf.data(), rbuf.size()) == m5::hal::error::error_t::OK && is_valid_ack(rbuf)) {
        uint16_t sz = (((uint16_t)rbuf[7]) << 8) | (uint16_t)rbuf[8];
        rbuf.resize(rbuf.size() + sz);
        if (readWithTransaction(rbuf.data() + 9, sz) == m5::hal::error::error_t::OK) {
            auto sum      = sum16(rbuf.data() + 6, rbuf.size() - 8);
            uint16_t rsum = (rbuf[rbuf.size() - 2] << 8) | rbuf[rbuf.size() - 1];

            // M5_LIB_LOGD(">>>> CONFIRM:%02X Sum:%04X/%04X", rbuf[9], sum, rsum);

            if (sum == rsum) {
                return static_cast<ConfirmCode>(rbuf[9]);
            }
        }
    }

    // m5::utility::log::dump(rbuf.data(), rbuf.size(), false);

    return ConfirmCode::PacketError;
}

// Only confirm code 0x00 is permitted
bool UnitFinger2::transceive_command(Packet& rbuf, const uint8_t cmd, const uint32_t addr, const uint8_t* payload,
                                     const uint16_t payload_len)
{
    if (write_command(cmd, addr, payload, payload_len)) {
        return read_response(rbuf) == ConfirmCode::OK;
    }
    return false;
}

// Only confirm code 0x00 is permitted
bool UnitFinger2::transceive_command8(Packet& rbuf, const uint8_t cmd, const uint32_t addr, const uint8_t value)
{
    return transceive_command(rbuf, cmd, addr, &value, 1);
}

}  // namespace unit
}  // namespace m5

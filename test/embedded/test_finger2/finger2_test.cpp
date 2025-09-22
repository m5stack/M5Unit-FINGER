/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitFinger2
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_Finger2.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include <random>
#include <algorithm>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::finger2;
using namespace m5::unit::finger2::command;
using m5::unit::types::elapsed_time_t;

extern const uint8_t template_data[];    // template_data.cpp
extern const size_t template_data_size;  // template_data.cpp

const ::testing::Environment* global_fixture = ::testing::AddGlobalTestEnvironment(new GlobalFixture<400000U>());

class TestFinger2 : public UARTComponentTestBase<UnitFinger2, bool> {
protected:
    virtual UnitFinger2* get_instance() override
    {
        auto ptr = new m5::unit::UnitFinger2();
        return ptr;
    }

    virtual bool is_using_hal() const override
    {
        return GetParam();
    };
    virtual HardwareSerial* init_serial() override
    {
        auto pin_num_in  = M5.getPin(m5::pin_name_t::port_c_rxd);
        auto pin_num_out = M5.getPin(m5::pin_name_t::port_c_txd);
        if (pin_num_in < 0 || pin_num_out < 0) {
            // M5_LOGW("PortC is not available");
            Wire.end();
            pin_num_in  = M5.getPin(m5::pin_name_t::port_a_pin1);
            pin_num_out = M5.getPin(m5::pin_name_t::port_a_pin2);
        }

        // clang-format off
#if defined(CONFIG_IDF_TARGET_ESP32C6)
    auto& s = Serial1;
#elif SOC_UART_NUM > 2
    auto& s = Serial2;
#elif SOC_UART_NUM > 1
    auto& s = Serial1;
#else
#error "Not enough Serial"
#endif
        // clang-format on

        // M5_LOGI("getPin: %d,%d", pin_num_in, pin_num_out);
        s.end();
        s.begin(115200, SERIAL_8N1, pin_num_in, pin_num_out);
        return &s;
    }

    void reset_serial(const uint32_t baud = 19200)
    {
        auto pin_num_in  = M5.getPin(m5::pin_name_t::port_c_rxd);
        auto pin_num_out = M5.getPin(m5::pin_name_t::port_c_txd);
        if (pin_num_in < 0 || pin_num_out < 0) {
            // M5_LOGW("PortC is not available");
            Wire.end();
            pin_num_in  = M5.getPin(m5::pin_name_t::port_a_pin1);
            pin_num_out = M5.getPin(m5::pin_name_t::port_a_pin2);
        }
        // M5_LOGI("%u getPin: %d,%d", baud, pin_num_in, pin_num_out);
        serial->end();
        serial->begin(baud, SERIAL_8N1, pin_num_in, pin_num_out);
        while (serial->available()) {
            serial->read();
        }
    }
};

// INSTANTIATE_TEST_SUITE_P(ParamValues, TestFinger2, ::testing::Values(false, true));
// INSTANTIATE_TEST_SUITE_P(ParamValues, TestFinger2, ::testing::Values(true));
INSTANTIATE_TEST_SUITE_P(ParamValues, TestFinger2, ::testing::Values(false));

namespace {

constexpr WorkMode workmode_table[] = {
    WorkMode::ScheduledSleep,
    WorkMode::AlwaysActive,
};

constexpr LEDMode led_table[] = {
    LEDMode::Bleath, LEDMode::Blink, LEDMode::On, LEDMode::Off, LEDMode::Fadein, LEDMode::Fadeout,
};
constexpr auto_enroll_flag_t enroll_flags_table[] = {
    0,
    auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS,
    auto_enroll_flag::ALLOW_OVERWRITE_PAGE,
    auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS | auto_enroll_flag::ALLOW_OVERWRITE_PAGE,
    auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE,
    auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE | auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS,
    auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE | auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE,
    auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE | auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS |
        auto_enroll_flag::ALLOW_OVERWRITE_PAGE,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER | auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER | auto_enroll_flag::ALLOW_OVERWRITE_PAGE,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER | auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS |
        auto_enroll_flag::ALLOW_OVERWRITE_PAGE,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER | auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER | auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE |
        auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER | auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE |
        auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE,
    auto_enroll_flag::NO_NEED_RELAESE_FINGER | auto_enroll_flag::PROHIBIT_DUPLICATE_TEMPLATE |
        auto_enroll_flag::DONT_RETURN_INTERMEDIATE_RESULTS | auto_enroll_flag::ALLOW_OVERWRITE_PAGE,
};

constexpr auto_identify_flag_t identify_flags_table[] = {
    0,
    auto_identify_flag::DONT_RETURN_INTERMEDIATE_RESULTS,
};

auto rng = std::default_random_engine{};

void wait_sleep(UnitFinger2* u)
{
    m5::utility::delay(10 * 1000 + 100);
    bool awake{true};
    while (awake) {
        bool b{};
        if (u->readModuleStatus(b)) {
            // M5_LOGW("Awake? %u", b);
            awake = b;
        }
        m5::utility::delay(100);
    }
}

}  // namespace

TEST_P(TestFinger2, Basic)
{
    SCOPED_TRACE(ustr);

    /////
    // return;

    auto cfg = unit->config();

    EXPECT_EQ(unit->deviceAddress(), 0xFFFFFFFFU);
    EXPECT_EQ(unit->capacity(), 100U);
    EXPECT_EQ(unit->imageWidth(), 80);
    EXPECT_EQ(unit->imageHeight(), 208);

    bool awake{};
    EXPECT_TRUE(unit->readModuleStatus(awake));
    EXPECT_TRUE(awake);

    WorkMode mode{};
    EXPECT_TRUE(unit->readWorkMode(mode));
    EXPECT_EQ(mode, cfg.work_mode);

    for (auto&& wm : workmode_table) {
        auto s = m5::utility::formatString("WorkMode:%u", wm);
        SCOPED_TRACE(s.c_str());

        EXPECT_TRUE(unit->writeWorkMode(wm));
        EXPECT_TRUE(unit->readWorkMode(mode));
        EXPECT_EQ(mode, wm);

        //
        uint8_t sec{};
        EXPECT_TRUE(unit->readSleepTime(sec));
        EXPECT_EQ(sec, cfg.sleep_time);

        EXPECT_FALSE(unit->writeSleepTime(0));
        EXPECT_FALSE(unit->writeSleepTime(9));
        EXPECT_FALSE(unit->writeSleepTime(255));
        EXPECT_TRUE(unit->readSleepTime(sec));
        EXPECT_EQ(sec, cfg.sleep_time);

        EXPECT_TRUE(unit->writeSleepTime(254));
        EXPECT_TRUE(unit->readSleepTime(sec));
        EXPECT_EQ(sec, 254);

        EXPECT_TRUE(unit->writeSleepTime(128));
        EXPECT_TRUE(unit->readSleepTime(sec));
        EXPECT_EQ(sec, 128);

        EXPECT_TRUE(unit->writeSleepTime(10));
        EXPECT_TRUE(unit->readSleepTime(sec));
        EXPECT_EQ(sec, 10);

        EXPECT_TRUE(unit->writeSleepTime(cfg.sleep_time));
        EXPECT_TRUE(unit->readSleepTime(sec));
        EXPECT_EQ(sec, cfg.sleep_time);

        SystemBasicParams params{};
        EXPECT_TRUE(unit->readSystemParams(params));
        EXPECT_NE(params.status, 0U);
        // EXPECT_EQ(params.template_size, 0x2EC8u);
        // EXPECT_EQ(params.sensor_type,0x????);
        EXPECT_EQ(params.database_capacity, 100u);
        EXPECT_EQ(params.score_level, cfg.score_level);
        EXPECT_EQ(params.address, 0xFFFFFFFFu);
        EXPECT_GE(params.packet_size, 0u);
        EXPECT_LE(params.packet_size, 3u);
        EXPECT_EQ(params.address, 0xFFFFFFFFu);
        EXPECT_EQ(params.baud_rate, 6u);  // 57600 only (STM32 <-> Chip)

        for (uint8_t lv = 1; lv <= 5; ++lv) {
            EXPECT_TRUE(unit->writeSystemRegister(RegisterID::ScoreLevel, lv));
            EXPECT_TRUE(unit->readSystemParams(params));
            EXPECT_EQ(params.score_level, lv);
        }
        EXPECT_FALSE(unit->writeSystemRegister(RegisterID::ScoreLevel, 0));
        EXPECT_FALSE(unit->writeSystemRegister(RegisterID::ScoreLevel, 6));
        EXPECT_FALSE(unit->writeSystemRegister(RegisterID::ScoreLevel, 255));

        EXPECT_TRUE(unit->writeSystemRegister(RegisterID::ScoreLevel, cfg.score_level));
        EXPECT_TRUE(unit->readSystemParams(params));
        EXPECT_EQ(params.score_level, cfg.score_level);

        for (uint8_t ps = 0; ps <= 3; ++ps) {
            EXPECT_TRUE(unit->writeSystemRegister(RegisterID::PacketSize, ps));
            EXPECT_TRUE(unit->readSystemParams(params));
            EXPECT_EQ(params.packet_size, ps);
        }
        EXPECT_FALSE(unit->writeSystemRegister(RegisterID::PacketSize, 4));
        EXPECT_FALSE(unit->writeSystemRegister(RegisterID::PacketSize, 255));

        EXPECT_TRUE(unit->writeSystemRegister(RegisterID::PacketSize, 1));
        EXPECT_TRUE(unit->readSystemParams(params));
        EXPECT_EQ(params.packet_size, 1);

        uint8_t info[512]{};
        uint8_t info_empty[512]{};
        EXPECT_TRUE(unit->readInformationPage(info));
        EXPECT_TRUE(memcmp(info, info_empty, sizeof(info)) != 0);

        std::vector<uint32_t> rval{};
        for (uint_fast8_t i = 0; i < 100; ++i) {
            uint32_t v{};
            EXPECT_TRUE(unit->readRandomNumber(v));
            rval.push_back(v);
        }
        std::set<uint32_t> rset(rval.begin(), rval.end());
        EXPECT_NE(rset.size(), 1);

        bool state{}, state2{};
        EXPECT_TRUE(unit->handshake(state));
        EXPECT_TRUE(state);
        EXPECT_TRUE(unit->checkSensor(state2));
        EXPECT_TRUE(state2);

        uint8_t sn[32]{};
        uint8_t sn_empty[32]{};
        EXPECT_TRUE(unit->readSerialNumber(sn));
        EXPECT_TRUE(memcmp(sn, sn_empty, sizeof(sn)) != 0);

        uint8_t ver{};
        EXPECT_TRUE(unit->readFirmwareVersion(ver));
        EXPECT_NE(ver, 0u);
    }
}

TEST_P(TestFinger2, LED)
{
    SCOPED_TRACE(ustr);

    /////
    // return;

    EXPECT_TRUE(unit->writeSleepTime(10));

    for (auto&& wm : workmode_table) {
        auto s = m5::utility::formatString("WorkMode:%u", wm);
        SCOPED_TRACE(s.c_str());
        EXPECT_TRUE(unit->writeWorkMode(wm));

        // In sleep
        if (wm == WorkMode::ScheduledSleep) {
            wait_sleep(unit.get());

            for (auto&& mode : led_table) {
                EXPECT_FALSE(unit->writeControlLED(mode, LEDColor::White, 2, LEDColor::Blue));
            }
            LEDColor clr2[2] = {LEDColor::Red, LEDColor::Yellow};
            EXPECT_FALSE(unit->writeControlLEDRainbow(5 /*0.5sec*/, clr2, 2, 3));
            continue;
        }

        // In active
        for (auto&& mode : led_table) {
            M5_LOGI("LED:%u", mode);
            M5.Speaker.tone(2000, 20);
            EXPECT_TRUE(unit->writeControlLED(mode, LEDColor::White, 2, LEDColor::Blue));
            m5::utility::delay(3000);
        }

        LEDColor clr2[2]   = {LEDColor::Red, LEDColor::Yellow};
        LEDColor clr5[5]   = {LEDColor::Red, LEDColor::Yellow, LEDColor::Blue, LEDColor::Cyan, LEDColor::Green};
        LEDColor clr10[10] = {LEDColor::Red,   LEDColor::Yellow,  LEDColor::Blue,  LEDColor::Cyan,  LEDColor::Green,
                              LEDColor::White, LEDColor::Magenta, LEDColor::White, LEDColor::Green, LEDColor::Cyan};

        M5.Speaker.tone(3000, 20);
        EXPECT_TRUE(unit->writeControlLEDRainbow(5 /*0.5sec*/, clr2, 2, 3));
        m5::utility::delay(2 * 1000);

        M5.Speaker.tone(3000, 20);
        EXPECT_TRUE(unit->writeControlLEDRainbow(2 /*0.2sec*/, clr5, 5, 3));
        m5::utility::delay(2 * 1000);

        M5.Speaker.tone(3000, 20);
        EXPECT_TRUE(unit->writeControlLEDRainbow(1 /*0.1sec*/, clr10, 10, 1));
        m5::utility::delay(2 * 1000);

        M5.Speaker.tone(4000, 20);
        EXPECT_TRUE(unit->writeControlLED(LEDMode::Bleath, LEDColor::Blue, 36, LEDColor::Blue));
    }
}

TEST_P(TestFinger2, Notepad)
{
    SCOPED_TRACE(ustr);

    /////
    // return;

    uint8_t buf[32]{};
    constexpr uint8_t empty[32]{};
    constexpr uint8_t write1[1]   = {0x52};
    constexpr uint8_t write9[9]   = {0x99, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A};
    constexpr uint8_t write32[32] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                                     0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                                     0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    constexpr uint8_t write33[33] = {0x11, 0x12, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                                     0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                                     0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21};

    for (auto&& wm : workmode_table) {
        auto s = m5::utility::formatString("WorkMode:%u", wm);
        SCOPED_TRACE(s.c_str());
        EXPECT_TRUE(unit->writeWorkMode(wm));

        // In sleep
        if (wm == WorkMode::ScheduledSleep) {
            wait_sleep(unit.get());

            for (uint8_t page = 0; page < 8; ++page) {
                EXPECT_FALSE(unit->writeNotepad(page, write1, sizeof(write1)));
                EXPECT_FALSE(unit->writeNotepad(page, write9, sizeof(write9)));
                EXPECT_FALSE(unit->writeNotepad(page, write32, sizeof(write32)));
                EXPECT_FALSE(unit->writeNotepad(page, write33, sizeof(write33)));
                EXPECT_FALSE(unit->readNotepad(buf, page));
            }
            EXPECT_FALSE(unit->writeNotepad(8, write1, sizeof(write1)));
            EXPECT_FALSE(unit->writeNotepad(255, write1, sizeof(write1)));
            EXPECT_FALSE(unit->readNotepad(buf, 8));
            EXPECT_FALSE(unit->readNotepad(buf, 255));

            continue;
        }

        // In active
        for (uint8_t page = 0; page < 8; ++page) {
            auto s = m5::utility::formatString("Page:%u", page);
            SCOPED_TRACE(s.c_str());

            // Clear
            EXPECT_TRUE(unit->writeNotepad(page, empty, sizeof(empty)));
            EXPECT_TRUE(unit->readNotepad(buf, page));
            EXPECT_TRUE(memcmp(buf, empty, 32) == 0);

            // Write...
            EXPECT_TRUE(unit->writeNotepad(page, write1, sizeof(write1)));
            EXPECT_TRUE(unit->readNotepad(buf, page));
            EXPECT_EQ(buf[0], write1[0]);
            EXPECT_TRUE(memcmp(buf + 1, empty + 1, 31) == 0);

            EXPECT_TRUE(unit->writeNotepad(page, write9, sizeof(write9)));
            EXPECT_TRUE(unit->readNotepad(buf, page));
            EXPECT_TRUE(memcmp(buf, write9, 9) == 0);
            EXPECT_TRUE(memcmp(buf + 9, empty + 9, 32 - 9) == 0);

            EXPECT_TRUE(unit->writeNotepad(page, write32, sizeof(write32)));
            EXPECT_TRUE(unit->readNotepad(buf, page));
            EXPECT_TRUE(memcmp(buf, write32, 32) == 0);

            EXPECT_TRUE(unit->writeNotepad(page, write33, sizeof(write33)));
            EXPECT_TRUE(unit->readNotepad(buf, page));
            EXPECT_TRUE(memcmp(buf, write33, 32) == 0);

            EXPECT_TRUE(unit->writeNotepad(page, write9, sizeof(write9)));
            EXPECT_TRUE(unit->readNotepad(buf, page));
            EXPECT_TRUE(memcmp(buf, write9, 9) == 0);
            EXPECT_TRUE(memcmp(buf + 9, empty + 9, 32 - 9) == 0);
        }
    }
}

TEST_P(TestFinger2, Template)
{
    SCOPED_TRACE(ustr);

    /////
    // return;

    std::vector<uint8_t> tbuf{};
    tbuf.resize(UnitFinger2::TEMPLATE_SIZE);

    for (auto&& wm : workmode_table) {
        auto s = m5::utility::formatString("WorkMode:%u", wm);
        SCOPED_TRACE(s.c_str());
        EXPECT_TRUE(unit->writeWorkMode(wm));

        // In sleep
        if (wm == WorkMode::ScheduledSleep) {
            wait_sleep(unit.get());

            EXPECT_FALSE(unit->clear());
            uint16_t num{};
            EXPECT_FALSE(unit->readValidTemplates(num));
            uint16_t low{}, high{};

            uint8_t table[32]{};
            EXPECT_FALSE(unit->readIndexTable(table));

            EXPECT_FALSE(unit->findLowestAvailablePage(low));
            EXPECT_FALSE(unit->findHighestAvailablePage(high));

            EXPECT_FALSE(unit->existsTemplate(0));
            EXPECT_FALSE(unit->existsTemplate(100));
            EXPECT_FALSE(unit->existsTemplate(65535));

            EXPECT_FALSE(unit->writeTemplateAllBatches(template_data, template_data_size));
            uint16_t actual{};
            EXPECT_FALSE(unit->storeTemplate(100));
            EXPECT_FALSE(unit->storeTemplate(65535));
            EXPECT_FALSE(unit->deleteTemplate(50));

            EXPECT_FALSE(unit->readTemplateAllBatches(actual, tbuf.data(), tbuf.size()));

            continue;
        }
        // In active

        EXPECT_TRUE(unit->clear());  // all clear

        uint16_t num{};
        EXPECT_TRUE(unit->readValidTemplates(num));
        EXPECT_EQ(num, 0);

        uint16_t low{}, high{};
        EXPECT_TRUE(unit->findLowestAvailablePage(low));
        EXPECT_TRUE(unit->findHighestAvailablePage(high));
        EXPECT_EQ(low, 0);
        EXPECT_EQ(high, 99);

        //
        EXPECT_TRUE(unit->writeTemplateAllBatches(template_data, template_data_size));

        EXPECT_TRUE(unit->storeTemplate(50));
        EXPECT_TRUE(unit->readValidTemplates(num));
        EXPECT_EQ(num, 1);
        EXPECT_TRUE(unit->findLowestAvailablePage(low));
        EXPECT_TRUE(unit->findHighestAvailablePage(high));
        EXPECT_EQ(low, 0);
        EXPECT_EQ(high, 99);

        EXPECT_TRUE(unit->deleteTemplate(50));
        EXPECT_TRUE(unit->readValidTemplates(num));
        EXPECT_EQ(num, 0);
        EXPECT_TRUE(unit->findLowestAvailablePage(low));
        EXPECT_TRUE(unit->findHighestAvailablePage(high));
        EXPECT_EQ(low, 0);
        EXPECT_EQ(high, 99);

        //
        EXPECT_TRUE(unit->storeTemplate(0));
        EXPECT_TRUE(unit->readValidTemplates(num));
        EXPECT_EQ(num, 1);
        EXPECT_TRUE(unit->findLowestAvailablePage(low));
        EXPECT_TRUE(unit->findHighestAvailablePage(high));
        EXPECT_EQ(low, 1);
        EXPECT_EQ(high, 99);

        EXPECT_TRUE(unit->storeTemplate(99));
        EXPECT_TRUE(unit->readValidTemplates(num));
        EXPECT_EQ(num, 2);
        EXPECT_TRUE(unit->findLowestAvailablePage(low));
        EXPECT_TRUE(unit->findHighestAvailablePage(high));
        EXPECT_EQ(low, 1);
        EXPECT_EQ(high, 98);

        for (uint8_t i = 1; i < 50; ++i) {
            auto prev_num  = num;
            auto prev_low  = low;
            auto prev_high = high;

            M5_LOGI("Reg:%u, %u / %u", low, high, num);

            EXPECT_FALSE(unit->existsTemplate(low));
            EXPECT_TRUE(unit->storeTemplate(low));
            EXPECT_TRUE(unit->existsTemplate(low));
            EXPECT_TRUE(unit->readValidTemplates(num));
            // M5_LOGW("L:%u", num);
            EXPECT_EQ(num, prev_num + 1);

            EXPECT_FALSE(unit->existsTemplate(high));
            EXPECT_TRUE(unit->storeTemplate(high));
            EXPECT_TRUE(unit->existsTemplate(high));
            EXPECT_TRUE(unit->readValidTemplates(num));
            // M5_LOGW("H:%u", num);
            EXPECT_EQ(num, prev_num + 2);

            EXPECT_TRUE(unit->findLowestAvailablePage(low));
            EXPECT_TRUE(unit->findHighestAvailablePage(high));

            if (num < 100) {
                // M5_LOGW("(%u,%u) (%u,%u)", low, prev_low, high, prev_high);
                EXPECT_EQ(low, prev_low + 1);
                EXPECT_EQ(high, prev_high - 1);
            }
        }

        EXPECT_TRUE(unit->existsTemplate(0));
        EXPECT_TRUE(unit->existsTemplate(99));
        EXPECT_FALSE(unit->existsTemplate(100));
        EXPECT_FALSE(unit->existsTemplate(65535));

        EXPECT_TRUE(unit->readValidTemplates(num));
        EXPECT_EQ(num, 100u);
        EXPECT_TRUE(unit->findLowestAvailablePage(low));
        EXPECT_TRUE(unit->findHighestAvailablePage(high));
        EXPECT_EQ(low, 0xFFFFu);
        EXPECT_EQ(high, 0xFFFFu);

        EXPECT_FALSE(unit->storeTemplate(100u));
        EXPECT_FALSE(unit->storeTemplate(65535u));

        uint16_t actual{};
        EXPECT_TRUE(unit->readTemplateAllBatches(actual, tbuf.data(), tbuf.size()));
        EXPECT_EQ(actual, tbuf.size());

        auto all = num;
        EXPECT_FALSE(unit->deleteTemplate(100));
        EXPECT_FALSE(unit->deleteTemplate(65535));
        for (uint_fast8_t i = 0; i < 50; ++i) {
            // del 49-0
            M5_LOGI("del:%u", i);

            EXPECT_TRUE(unit->deleteTemplate(49 - i));
            // EXPECT_FALSE(unit->deleteTemplate(49 - i));
            EXPECT_TRUE(unit->readValidTemplates(num));
            --all;
            EXPECT_EQ(num, all);
            EXPECT_TRUE(unit->findLowestAvailablePage(low));
            EXPECT_TRUE(unit->findHighestAvailablePage(high));
            EXPECT_EQ(low, 49 - i);
            EXPECT_EQ(high, (i == 0) ? 49 : 50 + i - 1);

            // del 50-99
            M5_LOGI("del:%u", i + 50);
            EXPECT_TRUE(unit->deleteTemplate(50 + i));
            // EXPECT_FALSE(unit->deleteTemplate(50 + i));
            EXPECT_TRUE(unit->readValidTemplates(num));
            --all;
            EXPECT_EQ(num, all);
            EXPECT_TRUE(unit->findLowestAvailablePage(low));
            EXPECT_TRUE(unit->findHighestAvailablePage(high));
            EXPECT_EQ(low, 49 - i);
            EXPECT_EQ(high, 50 + i);
        }
    }
}

TEST_P(TestFinger2, Finger)
{
    SCOPED_TRACE(ustr);

    /////
    // return;

    for (auto&& wm : workmode_table) {
        auto s = m5::utility::formatString("WorkMode:%u", wm);
        SCOPED_TRACE(s.c_str());
        EXPECT_TRUE(unit->writeWorkMode(wm));

        // In sleep
        if (wm == WorkMode::ScheduledSleep) {
            wait_sleep(unit.get());

            bool detected{};
            EXPECT_FALSE(unit->capture(detected, false));
            EXPECT_FALSE(unit->capture(detected, true));

            uint8_t percentage{};
            bool quality{};
            EXPECT_FALSE(unit->readImageInformation(percentage, quality));

            std::vector<uint8_t> img{};
            EXPECT_FALSE(unit->readImage(img));

            for (uint8_t bid = 1; bid <= 5; ++bid) {
                EXPECT_FALSE(unit->generateCharacteristic(bid));
            }
            EXPECT_FALSE(unit->generateCharacteristic(0));
            EXPECT_FALSE(unit->generateCharacteristic(6));
            EXPECT_FALSE(unit->generateCharacteristic(255));

            EXPECT_FALSE(unit->generateTemplate());

            bool matched{};
            uint16_t score{}, page{};
            EXPECT_FALSE(unit->match(matched, score));
            EXPECT_FALSE(unit->search(matched, page, score));
            EXPECT_FALSE(unit->searchNow(matched, page, score));

            for (uint8_t bid = 1; bid <= 5; ++bid) {
                EXPECT_FALSE(unit->loadTemplate(bid, 50));
            }
            EXPECT_FALSE(unit->loadTemplate(0, 50));
            EXPECT_FALSE(unit->loadTemplate(6, 50));
            EXPECT_FALSE(unit->loadTemplate(255, 50));

            continue;
        }

        bool detected{};
        EXPECT_TRUE(unit->capture(detected, false));
        EXPECT_FALSE(detected);
        EXPECT_TRUE(unit->capture(detected, true));
        EXPECT_FALSE(detected);

        uint8_t percentage{};
        bool quality{};
        EXPECT_FALSE(unit->readImageInformation(percentage, quality));

        std::vector<uint8_t> img{};
        EXPECT_TRUE(unit->readImage(img));  // Get empty image
        // m5::utility::log::dump(img.data(), img.size(), false);

        for (uint8_t bid = 1; bid <= 5; ++bid) {
            EXPECT_FALSE(unit->generateCharacteristic(bid));
        }
        EXPECT_FALSE(unit->generateCharacteristic(0));
        EXPECT_FALSE(unit->generateCharacteristic(6));
        EXPECT_FALSE(unit->generateCharacteristic(255));

        EXPECT_FALSE(unit->generateTemplate());
        for (uint8_t bid = 1; bid <= 5; ++bid) {
            EXPECT_FALSE(unit->storeTemplate(52, bid));
        }
        EXPECT_FALSE(unit->storeTemplate(52, 0));
        EXPECT_FALSE(unit->storeTemplate(52, 6));
        EXPECT_FALSE(unit->storeTemplate(52, 255));
        for (uint8_t bid = 1; bid <= 5; ++bid) {
            EXPECT_FALSE(unit->storeTemplate(100, bid));
            EXPECT_FALSE(unit->storeTemplate(65535, bid));
        }

        bool matched{};
        uint16_t score{}, page{};
        EXPECT_TRUE(unit->match(matched, score));
        EXPECT_FALSE(matched);
        // M5_LOGW(">>>> %u %u", matched, score);

        EXPECT_TRUE(unit->search(matched, page, score));
        EXPECT_FALSE(matched);
        EXPECT_EQ(score, 0u);
        EXPECT_EQ(page, 0u);
        // M5_LOGW(">>>> %u %u %u", matched, score, page);

        EXPECT_TRUE(unit->searchNow(matched, page, score));
        EXPECT_FALSE(matched);
        EXPECT_EQ(score, 0u);
        EXPECT_EQ(page, 0u);
        // M5_LOGW(">>>> %u %u %u", matched, score, page);

        //
        EXPECT_TRUE(unit->writeTemplateAllBatches(template_data, template_data_size));
        EXPECT_TRUE(unit->storeTemplate(50));

        for (uint8_t bid = 1; bid <= 5; ++bid) {
            EXPECT_TRUE(unit->loadTemplate(bid, 50));
        }
        EXPECT_FALSE(unit->loadTemplate(0, 50));
        EXPECT_FALSE(unit->loadTemplate(6, 50));
        EXPECT_FALSE(unit->loadTemplate(255, 50));
    }
}

TEST_P(TestFinger2, Automatic)
{
    SCOPED_TRACE(ustr);
    for (auto&& wm : workmode_table) {
        auto s = m5::utility::formatString("WorkMode:%u", wm);
        SCOPED_TRACE(s.c_str());
        EXPECT_TRUE(unit->writeWorkMode(wm));

        // In sleep
        if (wm == WorkMode::ScheduledSleep) {
            wait_sleep(unit.get());

            ConfirmCode confirm{};
            EXPECT_FALSE(unit->autoEnroll(confirm, 0));

            bool matched{};
            uint16_t page{}, score{};
            EXPECT_FALSE(unit->autoIdentify(matched, page, score));
            continue;
        }

        ConfirmCode confirm{};
        EXPECT_FALSE(unit->autoEnroll(confirm, 0));
        bool matched{};
        uint16_t page{}, score{};
        EXPECT_FALSE(unit->autoIdentify(matched, page, score));

        EXPECT_FALSE(unit->autoEnroll(confirm, 100));
        EXPECT_FALSE(unit->autoEnroll(confirm, 65535));

        EXPECT_FALSE(unit->autoEnroll(confirm, 0, 0));
        EXPECT_FALSE(unit->autoEnroll(confirm, 0, 6));
        EXPECT_FALSE(unit->autoEnroll(confirm, 0, 255));

        for (auto&& eflags : enroll_flags_table) {
            EXPECT_FALSE(unit->autoEnroll(confirm, 0, 5, eflags));
        }

        EXPECT_FALSE(unit->autoIdentify(matched, page, score, 100));
        EXPECT_FALSE(unit->autoIdentify(matched, page, score, 0xFFFE));
        EXPECT_FALSE(unit->autoIdentify(matched, page, score, 0xFFFF, 2));
        EXPECT_FALSE(unit->autoIdentify(matched, page, score, 0xFFFF, 255));

        for (auto&& iflags : identify_flags_table) {
            EXPECT_FALSE(unit->autoIdentify(matched, page, score, 0xFFFF, 0, iflags));
        }
    }
}

#if 0
// page:0
0x3ffb202c| 00 03 06 09 0C 0F 12 15 18 1B 1E 21 24 27 2A 2D |
0x3ffb203c| 30 33 36 39 3C 3F 42 45 48 4B 4E 51 54 57 5A 5D |
48, 49, 50
#endif

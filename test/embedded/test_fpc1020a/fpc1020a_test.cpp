/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for FPC1020A
*/
#include <gtest/gtest.h>
#include <Wire.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_FPC1xxx.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include <random>
#include <algorithm>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::fpc1xxx;
using namespace m5::unit::fpc1xxx::command;
using m5::unit::types::elapsed_time_t;

const ::testing::Environment* global_fixture = ::testing::AddGlobalTestEnvironment(new GlobalFixture<400000U>());

class TestFPC1020A : public UARTComponentTestBase<UnitFPC1020A, bool> {
protected:
    virtual UnitFPC1020A* get_instance() override
    {
        auto ptr = new m5::unit::UnitFPC1020A();
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

#if SOC_UART_NUM > 2
        auto& s = Serial2;
#elif SOC_UART_NUM > 1
        auto& s = Serial1;
#else
#error "Not enough Serial"
#endif

        // M5_LOGI("getPin: %d,%d", pin_num_in, pin_num_out);
        s.end();
        // s.setRxBufferSize(1024);
        // s.begin(9600, SERIAL_8N1, pin_num_in, pin_num_out);
        s.begin(19200, SERIAL_8N1, pin_num_in, pin_num_out);
        // s.begin(38400, SERIAL_8N1, pin_num_in, pin_num_out);
        // s.begin(57600, SERIAL_8N1, pin_num_in, pin_num_out);
        // s.begin(115200, SERIAL_8N1, pin_num_in, pin_num_out);
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

// INSTANTIATE_TEST_SUITE_P(ParamValues, TestFPC1020A, ::testing::Values(false, true));
// INSTANTIATE_TEST_SUITE_P(ParamValues, TestFPC1020A, ::testing::Values(true));
INSTANTIATE_TEST_SUITE_P(ParamValues, TestFPC1020A, ::testing::Values(false));

namespace {

auto rng = std::default_random_engine{};

void print_all_users(UnitFPC1020A* unit)
{
    std::vector<User> v{};
    if (unit->readAllUser(v)) {
        M5.Log.printf("All user data (%u):\n", v.size());
        uint16_t idx{};
        for (auto&& u : v) {
            M5.Log.printf("  [%3d] ID:%5u, PERMISSION:%u\n", idx++, u.id, u.permission);
        }
    }
}

constexpr BaudRate br_table[5] = {
    BaudRate::Baud9600, BaudRate::Baud19200, BaudRate::Baud38400, BaudRate::Baud57600, BaudRate::Baud115200,
};
constexpr uint32_t brv_table[5] = {9600, 19200, 38400, 57600, 115200};

constexpr uint8_t ch_data[193] = {
    0x7C, 0x8C, 0x37, 0xDF, 0xC1, 0xAD, 0xA5, 0xD1, 0x33, 0xD1, 0x3A, 0xBE, 0x03, 0xF0, 0x21, 0xE9, 0xB1, 0xB7,
    0x8C, 0xCB, 0xD8, 0x2F, 0x7F, 0xF2, 0xB3, 0x8C, 0x6D, 0x48, 0xD0, 0x1E, 0x48, 0x1B, 0x2D, 0x4F, 0xAF, 0x71,
    0x71, 0x80, 0x5F, 0xD7, 0xF2, 0xD3, 0x9E, 0xF4, 0xC4, 0xF1, 0x9B, 0x94, 0x96, 0xE8, 0x1D, 0xAB, 0x81, 0x93,
    0xB3, 0x73, 0x7E, 0x1B, 0x27, 0xD9, 0xC4, 0x39, 0x57, 0x16, 0x64, 0x41, 0xB9, 0x35, 0x15, 0xE8, 0xF0, 0x3C,
    0x95, 0xD8, 0xE8, 0xCE, 0x1E, 0x18, 0x64, 0xFA, 0xAD, 0x68, 0xDD, 0xFC, 0x59, 0x32, 0x13, 0x01, 0x09, 0x39,
    0x0B, 0x0F, 0x1F, 0xE5, 0xCA, 0x71, 0x68, 0x05, 0xF8, 0x36, 0x2E, 0x98, 0xDC, 0xCA, 0xAD, 0xC8, 0x6A, 0xDB,
    0xED, 0x25, 0x80, 0x1A, 0x9A, 0x9D, 0xCF, 0xA6, 0x26, 0x43, 0x19, 0xDD, 0xAF, 0xE8, 0x3A, 0x89, 0xC5, 0x1F,
    0x3C, 0x6D, 0x19, 0x9D, 0x38, 0xDE, 0x10, 0xE6, 0x60, 0xC3, 0x7B, 0xE8, 0x72, 0xC3, 0xF2, 0xB3, 0x16, 0x60,
    0xDE, 0x8B, 0xC9, 0x59, 0x02, 0xB9, 0x10, 0x32, 0x62, 0xCD, 0xB9, 0x41, 0xF7, 0x73, 0x76, 0xF5, 0xD3, 0xDB,
    0xB7, 0xA3, 0xD5, 0xA3, 0x87, 0x79, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0x1A,
};

}  // namespace

TEST_P(TestFPC1020A, Baud)
{
    uint32_t idx{};
    for (auto&& br : br_table) {
        M5_LOGI("%u/%u", br, brv_table[idx]);
        EXPECT_TRUE(unit->writeBaudRate(br));
        reset_serial(brv_table[idx]);

        uint32_t sno{0xdeadbeef};
        EXPECT_TRUE(unit->readSerialNumber(sno));
        EXPECT_NE(sno, 0xdeadbeef);

        ++idx;
    }

    EXPECT_TRUE(unit->writeBaudRate(BaudRate::Baud19200));
    reset_serial(19200);
    uint32_t sno{0xdeadbeef};
    EXPECT_TRUE(unit->readSerialNumber(sno));
    EXPECT_NE(sno, 0xdeadbeef);
}

TEST_P(TestFPC1020A, Basic)
{
    SCOPED_TRACE(ustr);

    uint32_t sno{0xdeadbeef};
    char ver[9]{};
    EXPECT_TRUE(unit->readSerialNumber(sno));
    EXPECT_NE(sno, 0xdeadbeef);
    EXPECT_TRUE(unit->readVersion(ver));
    EXPECT_EQ(strlen(ver), 8);
}

TEST_P(TestFPC1020A, Settings)
{
    SCOPED_TRACE(ustr);

    {
        uint16_t users{};
        EXPECT_TRUE(unit->deleteAllUsers());
        EXPECT_TRUE(unit->readRegisteredUserCount(users));
        EXPECT_EQ(users, 0);

        Mode m{};
        EXPECT_TRUE(unit->readRegistrationMode(m));

        // Allow
        EXPECT_TRUE(unit->writeRegistrationMode(Mode::AllowDuplicate));
        EXPECT_TRUE(unit->readRegistrationMode(m));
        EXPECT_EQ(m, Mode::AllowDuplicate);

        uint8_t perm = rng() % 3 + 1;
        std::array<uint8_t, 193> characteristic{};
        std::generate(characteristic.begin(), characteristic.end(),
                      []() { return static_cast<uint8_t>(rng() & 0xFF); });

        EXPECT_TRUE(unit->registerCharacteristic(unit->maximumUserID(), perm, characteristic.data()));
        EXPECT_TRUE(unit->registerCharacteristic(unit->maximumUserID() - 1, perm, characteristic.data()));

        // Prohibit (Applies to fingerprint registration only)
        EXPECT_TRUE(unit->writeRegistrationMode(Mode::ProhibitDuplicate));
        EXPECT_TRUE(unit->readRegistrationMode(m));
        EXPECT_EQ(m, Mode::ProhibitDuplicate);
        EXPECT_TRUE(unit->registerCharacteristic(unit->maximumUserID() - 2, perm, characteristic.data()));
    }

    {
        for (uint8_t lv = 0; lv < 10; ++lv) {
            EXPECT_TRUE(unit->writeComparisonLevel(lv));
            uint8_t lv2{};
            EXPECT_TRUE(unit->readComparisonLevel(lv2));
            EXPECT_EQ(lv2, lv);
        }
        EXPECT_FALSE(unit->writeComparisonLevel(10));
        EXPECT_FALSE(unit->writeComparisonLevel(100));
        EXPECT_FALSE(unit->writeComparisonLevel(255));

        EXPECT_TRUE(unit->writeComparisonLevel(5));
        uint8_t lv2{};
        EXPECT_TRUE(unit->readComparisonLevel(lv2));
        EXPECT_EQ(lv2, 5);
    }

    {
        uint32_t count{8};
        while (count--) {
            uint8_t to = rng() & 0xFF;
            EXPECT_TRUE(unit->writeTimeout(to));
            uint8_t to2{};
            EXPECT_TRUE(unit->readTimeout(to2));
            EXPECT_EQ(to2, to);
        }
        EXPECT_TRUE(unit->writeTimeout(0));
        uint8_t to2{};
        EXPECT_TRUE(unit->readTimeout(to2));
        EXPECT_EQ(to2, 0);
    }
}

TEST_P(TestFPC1020A, User)
{
    uint16_t users{};
    EXPECT_TRUE(unit->deleteAllUsers());
    EXPECT_TRUE(unit->readRegisteredUserCount(users));
    EXPECT_EQ(users, 0);
    {
        uint16_t id{};
        EXPECT_TRUE(unit->findAvailableUserID(id));
        EXPECT_EQ(id, 1);

        EXPECT_TRUE(unit->findAvailableUserID(id, 1, 2));
        EXPECT_EQ(id, 1);

        EXPECT_TRUE(unit->findAvailableUserID(id, 100, 110));
        EXPECT_EQ(id, 100);

        EXPECT_TRUE(unit->findAvailableUserID(id, unit->maximumUserID(), unit->maximumUserID()));
        EXPECT_EQ(id, unit->maximumUserID());
    }

    // Make random users
    //    for (uint_fast8_t i = unit->minimumUserID(); i <= unit->maximumUserID(); ++i) {
    for (uint_fast8_t i = 1; i <= 10; ++i) {
        uint8_t perm = rng() % 3 + 1;
        std::array<uint8_t, 193> characteristic{};
        std::generate(characteristic.begin(), characteristic.end(),
                      []() { return static_cast<uint8_t>(rng() & 0xFF); });
        EXPECT_TRUE(unit->registerCharacteristic(i, perm, characteristic.data()));
    }

    {
        uint16_t id{};
        EXPECT_TRUE(unit->findAvailableUserID(id));
        EXPECT_EQ(id, 11);

        EXPECT_FALSE(unit->findAvailableUserID(id, 1, 10));
        EXPECT_EQ(id, 0);
    }

    EXPECT_TRUE(unit->readRegisteredUserCount(users));
    EXPECT_NE(users, 0);

    std::vector<User> uv;
    EXPECT_TRUE(unit->readAllUser(uv));
    EXPECT_EQ(uv.size(), users);

    for (uint16_t id = 1; id <= users / 2; ++id) {
        uint8_t perm{};
        uint8_t characteristic[193]{};
        EXPECT_TRUE(unit->readUser(perm, id)) << id;
        EXPECT_TRUE(unit->readUserCharacteristic(characteristic, id)) << id;
        EXPECT_GE(perm, 1) << id;
        EXPECT_LE(perm, 3) << id;

        EXPECT_TRUE(unit->deleteUser(id));
        EXPECT_FALSE(unit->readUser(perm, id)) << id;
        EXPECT_FALSE(unit->readUserCharacteristic(characteristic, id)) << id;
        EXPECT_FALSE(unit->deleteUser(id));
    }

    uint16_t users2{};
    EXPECT_TRUE(unit->readRegisteredUserCount(users2));
    EXPECT_EQ(users2, users - users / 2);

    EXPECT_TRUE(unit->deleteAllUsers());
    EXPECT_TRUE(unit->readRegisteredUserCount(users2));
    EXPECT_EQ(users2, 0);
}

TEST_P(TestFPC1020A, Finger)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->deleteAllUsers());

    EXPECT_FALSE(unit->registerFinger(1, 1));

    bool match{};
    EXPECT_TRUE(unit->verifyFinger(match, 1));
    EXPECT_FALSE(match);

    uint16_t id{};
    uint8_t perm{};
    EXPECT_FALSE(unit->identifyFinger(id, perm));
    EXPECT_EQ(id, 0);
    EXPECT_EQ(perm, 0);

    uint8_t characteristic[193]{};
    EXPECT_FALSE(unit->scanCharacteristic(characteristic));

    std::vector<uint8_t> img{};
    EXPECT_FALSE(unit->captureImage(img));

    EXPECT_TRUE(unit->registerCharacteristic(1, 2, ch_data));
    EXPECT_TRUE(unit->verifyCharacteristic(match, 1, ch_data));
    EXPECT_TRUE(match);
    EXPECT_TRUE(unit->verifyCharacteristic(match, 2, ch_data));
    EXPECT_FALSE(match);

    EXPECT_TRUE(unit->identifyCharacteristic(id, ch_data));
    EXPECT_EQ(id, 1);

    EXPECT_FALSE(unit->compareCharacteristic(match, ch_data));
    EXPECT_FALSE(match);
}

TEST_P(TestFPC1020A, Sleep)
{
    SCOPED_TRACE(ustr);

    // Deep sleep
    uint32_t sno{0xdeadbeef};
    char ver[9]{};
    EXPECT_TRUE(unit->sleep());
    EXPECT_FALSE(unit->readSerialNumber(sno));
    EXPECT_FALSE(unit->readVersion(ver));
}

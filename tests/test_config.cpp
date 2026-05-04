// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
// Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
//
// Tests for tmf8829_get_configuration, tmf8829_set_configuration,
// tmf8829_read_device_info, and tmf8829_set_i2c_address.

#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstring>

#include "support/test_fixture.hpp"

using tmf8829_test::FakeBus;
using tmf8829_test::Fixture;

// --- get_configuration / set_configuration ---

TEST_CASE("get_configuration rejects uninitialised driver", "[tmf8829][config]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_get_configuration(&drv) == TMF8829_E_PARAM);
}

TEST_CASE("get_configuration: happy path loads and reads config page", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // CMD_STAT must go to OK when LOAD_CONFIG_PAGE is issued.
  f.bus.preload(TMF8829_REG_CMD_STAT, TMF8829_STAT_OK);
  f.bus.write_auto_response[TMF8829_REG_CMD_STAT] = TMF8829_STAT_OK;
  // CID_RID must match the expected poll value (LOAD_CONFIG_PAGE opcode).
  f.bus.preload(TMF8829_REG_CID_RID, TMF8829_CMD_LOAD_CONFIG_PAGE);

  // Pre-fill config registers with a recognizable pattern.
  for (std::uint16_t i = 0; i < TMF8829_CFG_PAGE_SIZE; ++i) {
    f.bus.preload(static_cast<std::uint8_t>(TMF8829_REG_CFG_PERIOD_MS_LSB + i), static_cast<std::uint8_t>(i & 0xFFu));
  }

  REQUIRE(tmf8829_get_configuration(&f.drv) == TMF8829_OK);

  // Verify the config array was populated.
  REQUIRE(f.drv.config[0] == 0x00u); // offset 0
  REQUIRE(f.drv.config[5] == 0x05u); // offset 5
}

TEST_CASE("set_configuration rejects uninitialised driver", "[tmf8829][config]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_set_configuration(&drv) == TMF8829_E_PARAM);
}

TEST_CASE("set_configuration: happy path writes config and issues WRITE_PAGE", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // Fill config with test data.
  std::memset(f.drv.config, 0xAA, sizeof(f.drv.config));

  // CMD_STAT returns OK when polled after WRITE_PAGE.
  f.bus.preload(TMF8829_REG_CMD_STAT, TMF8829_STAT_OK);
  f.bus.write_auto_response[TMF8829_REG_CMD_STAT] = TMF8829_STAT_OK;

  REQUIRE(tmf8829_set_configuration(&f.drv) == TMF8829_OK);

  // Verify a write to the config register range occurred.
  bool found_config_write = false;
  for (const auto& op : f.bus.ops) {
    if (op.kind == FakeBus::OpKind::Write && op.reg == TMF8829_REG_CFG_PERIOD_MS_LSB) {
      found_config_write = true;
      REQUIRE(op.data.size() == TMF8829_CFG_PAGE_SIZE);
      REQUIRE(op.data[0] == 0xAA);
      break;
    }
  }
  REQUIRE(found_config_write);
}

TEST_CASE("set_configuration: bus error propagates", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.bus.write_error_after_n = 0;

  REQUIRE(tmf8829_set_configuration(&f.drv) == TMF8829_E_BUS);
}

// --- read_device_info ---

TEST_CASE("read_device_info rejects uninitialised driver", "[tmf8829][config]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_read_device_info(&drv) == TMF8829_E_PARAM);
}

TEST_CASE("read_device_info: returns E_STATE in bootloader mode", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  f.bus.preload(TMF8829_REG_ID, TMF8829_CHIP_ID);
  f.bus.preload(static_cast<std::uint8_t>(TMF8829_REG_ID + 1u), static_cast<std::uint8_t>(0x01));
  // APP_ID = BOOTLOADER
  f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__BOOTLOADER);

  REQUIRE(tmf8829_read_device_info(&f.drv) == TMF8829_E_STATE);
}

TEST_CASE("read_device_info: happy path in application mode", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  f.bus.preload(TMF8829_REG_ID, TMF8829_CHIP_ID);
  f.bus.preload(static_cast<std::uint8_t>(TMF8829_REG_ID + 1u), static_cast<std::uint8_t>(0x02));
  f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__APPLICATION);
  f.bus.preload(TMF8829_REG_APP_VER_MAJOR, static_cast<std::uint8_t>(3));
  f.bus.preload(TMF8829_REG_APP_VER_MINOR, static_cast<std::uint8_t>(7));
  // Serial number (4 bytes at 0x1C)
  f.bus.preload(TMF8829_REG_SERIAL_NUMBER_0, { 0x78, 0x56, 0x34, 0x12 });

  REQUIRE(tmf8829_read_device_info(&f.drv) == TMF8829_OK);
  REQUIRE(f.drv.chip_version[0] == TMF8829_CHIP_ID);
  REQUIRE(f.drv.device_serial_number == 0x12345678u);
}

// --- set_i2c_address ---

TEST_CASE("set_i2c_address rejects uninitialised driver", "[tmf8829][config]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_set_i2c_address(&drv, 0x42) == TMF8829_E_PARAM);
}

TEST_CASE("set_i2c_address rejects SPI bus", "[tmf8829][config]") {
  Fixture f(TMF8829_BUS_SPI, 0);
  REQUIRE(f.init() == TMF8829_OK);
  REQUIRE(tmf8829_set_i2c_address(&f.drv, 0x42) == TMF8829_E_PARAM);
}

TEST_CASE("set_i2c_address rejects invalid address", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  REQUIRE(tmf8829_set_i2c_address(&f.drv, 0xFF) == TMF8829_E_PARAM);
}

TEST_CASE("set_i2c_address: happy path", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  REQUIRE(tmf8829_set_i2c_address(&f.drv, 0x42) == TMF8829_OK);
  REQUIRE(f.drv.i2c_addr == 0x42);

  // Verify the write was issued to the correct register.
  bool found = false;
  for (const auto& op : f.bus.ops) {
    if (op.kind == FakeBus::OpKind::Write && op.reg == TMF8829_REG_I2C_DEVADDR) {
      found = true;
      REQUIRE(op.data.size() == 1);
      REQUIRE(op.data[0] == 0x42);
    }
  }
  REQUIRE(found);
}

TEST_CASE("set_i2c_address: bus error propagates", "[tmf8829][config]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.bus.write_error_after_n = 0;

  REQUIRE(tmf8829_set_i2c_address(&f.drv, 0x42) == TMF8829_E_BUS);
  // Address must NOT be updated on failure.
  REQUIRE(f.drv.i2c_addr == TMF8829_DEFAULT_I2C_ADDR);
}

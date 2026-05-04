// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
// Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
//
// Tests for tmf8829_download_firmware (FIFO and WR_RAM paths).

#include <array>
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

#include "support/test_fixture.hpp"

using tmf8829_test::FakeBus;
using tmf8829_test::Fixture;

namespace {

// Small fake firmware image (64 bytes).
static const std::uint8_t kFakeImage[] = {
  0x00, 0xC0, 0x01, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};
static constexpr std::uint32_t kFakeImageSize = sizeof(kFakeImage);

int fake_fw_read(tmf8829_driver_t*, std::uint32_t offset, std::uint8_t* buf, std::uint32_t len, std::uint32_t* total) {
  if (total != nullptr) {
    *total = kFakeImageSize;
  }
  if (buf == nullptr && len == 0u) {
    return 0;
  }
  if (buf == nullptr) {
    return TMF8829_E_PARAM;
  }
  if (offset >= kFakeImageSize) {
    return 0;
  }
  std::uint32_t avail = kFakeImageSize - offset;
  std::uint32_t n     = (len < avail) ? len : avail;
  std::memcpy(buf, &kFakeImage[offset], n);
  return static_cast<int>(n);
}

int failing_fw_read(tmf8829_driver_t*, std::uint32_t, std::uint8_t*, std::uint32_t, std::uint32_t*) {
  return -10;
}

int short_fw_read(tmf8829_driver_t*, std::uint32_t offset, std::uint8_t* buf, std::uint32_t len, std::uint32_t* total) {
  if (total != nullptr) {
    *total = kFakeImageSize;
  }
  if (buf == nullptr && len == 0u) {
    return 0;
  }
  if (buf == nullptr) {
    return TMF8829_E_PARAM;
  }
  // Return fewer bytes than requested (short read).
  if (len > 1u) {
    std::memcpy(buf, &kFakeImage[offset], 1u);
    return 1;
  }
  std::memcpy(buf, &kFakeImage[offset], len);
  return static_cast<int>(len);
}

void setup_bootloader_ready(Fixture& f) {
  // Bootloader mode: APP_ID = 0x80, CMD_STAT = READY (0x00)
  f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__BOOTLOADER);
  f.bus.preload(TMF8829_REG_CMD_STAT, TMF8829_BL_STAT_READY);
  // After any write to CMD_STAT, auto-respond with READY to satisfy polling.
  f.bus.write_auto_response[TMF8829_REG_CMD_STAT] = TMF8829_BL_STAT_READY;
  // FIFO register is non-incrementing (single address).
  f.bus.non_incrementing[TMF8829_REG_FIFO] = true;
}

} // namespace

TEST_CASE("download_firmware rejects uninitialised driver", "[tmf8829][download]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_download_firmware(&drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0) == TMF8829_E_PARAM);
}

TEST_CASE("download_firmware rejects null fw_image_read", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.drv.fw_image_read = nullptr;
  REQUIRE(tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0) == TMF8829_E_PARAM);
}

TEST_CASE("download_firmware propagates fw_image_read probe failure", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.drv.fw_image_read = failing_fw_read;
  REQUIRE(tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0) == -10);
}

TEST_CASE("download_firmware WR_RAM: happy path", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;

  // After download completes, bl_start_ram_app polls APP_ID for APPLICATION.
  f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__APPLICATION);
  // ENABLE register for download_set_enable_ram read-modify-write.
  f.bus.preload(TMF8829_REG_ENABLE, TMF8829_ENABLE_PON_MASK);

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0);
  REQUIRE(rc == TMF8829_OK);
}

TEST_CASE("download_firmware FIFO: happy path", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;

  f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__APPLICATION);
  f.bus.preload(TMF8829_REG_ENABLE, TMF8829_ENABLE_PON_MASK);

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 1);
  REQUIRE(rc == TMF8829_OK);
}

TEST_CASE("download_firmware WR_RAM: short read yields E_BOOTLOADER", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = short_fw_read;

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0);
  REQUIRE(rc == TMF8829_E_BOOTLOADER);
}

TEST_CASE("download_firmware FIFO: short read yields E_BOOTLOADER", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = short_fw_read;

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 1);
  REQUIRE(rc == TMF8829_E_BOOTLOADER);
}

TEST_CASE("download_firmware WR_RAM: bus write failure mid-download", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;

  // Let the first few writes succeed (address set), then fail during data.
  f.bus.write_error_after_n = 3;

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0);
  REQUIRE(rc < 0);
}

TEST_CASE("download_firmware: bl_start_ram_app timeout", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;

  // APP_ID stays at BOOTLOADER — bl_start_ram_app will time out.
  // (Do NOT preload APPLICATION.)

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 1);
  REQUIRE(rc == TMF8829_E_TIMEOUT);
}

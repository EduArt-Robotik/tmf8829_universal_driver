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

std::vector<std::uint8_t> ram_response(const std::uint8_t* data, std::uint8_t len) {
  std::vector<std::uint8_t> response{ TMF8829_BL_STAT_READY, len };
  response.insert(response.end(), data, data + len);
  return response;
}

void queue_write_status(Fixture& f) {
  f.bus.non_incrementing[TMF8829_REG_CMD_STAT] = true;
  f.bus.fifo_push(TMF8829_REG_CMD_STAT, { TMF8829_BL_STAT_READY, TMF8829_BL_STAT_READY });
}

void queue_readback(Fixture& f, const std::vector<std::uint8_t>& response) {
  // Address-command status, read-command status, then the full readback response.
  f.bus.fifo_push(TMF8829_REG_CMD_STAT, { TMF8829_BL_STAT_READY, TMF8829_BL_STAT_READY });
  f.bus.fifo_push(TMF8829_REG_CMD_STAT, response.data(), response.size());
}

void queue_ram_download(Fixture& f, const std::uint8_t* image, std::uint32_t size, std::uint8_t chunk_limit = 128u) {
  for (std::uint32_t offset = 0; offset < size;) {
    const auto len = static_cast<std::uint8_t>((size - offset < chunk_limit) ? size - offset : chunk_limit);
    queue_write_status(f);
    const auto response = ram_response(image + offset, len);
    queue_readback(f, response);
    queue_readback(f, response);
    offset += len;
  }
}

void require_no_app_start(const Fixture& f) {
  for (const auto& op : f.bus.ops) {
    if (op.kind == FakeBus::OpKind::Write && op.reg == TMF8829_REG_CMD_STAT) {
      REQUIRE(op.data[0] != TMF8829_BL_CMD_STAT_START_RAM_APP);
    }
    REQUIRE_FALSE((op.kind == FakeBus::OpKind::Write && op.reg == TMF8829_REG_ENABLE));
  }
}

const auto kLargeImage = [] {
  std::array<std::uint8_t, 273> image{};
  for (std::size_t i = 0; i < image.size(); ++i) {
    image[i] = static_cast<std::uint8_t>(i);
  }
  return image;
}();

int large_fw_read(tmf8829_driver_t*, std::uint32_t offset, std::uint8_t* buf, std::uint32_t len, std::uint32_t* total) {
  if (total != nullptr) {
    *total = static_cast<std::uint32_t>(kLargeImage.size());
  }
  if (buf == nullptr && len == 0u) {
    return 0;
  }
  if (buf == nullptr || offset > kLargeImage.size() || len > kLargeImage.size() - offset) {
    return TMF8829_E_PARAM;
  }
  std::memcpy(buf, kLargeImage.data() + offset, len);
  return static_cast<int>(len);
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
  Fixture f(GENERATE(TMF8829_BUS_I2C, TMF8829_BUS_SPI));
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;
  queue_ram_download(f, kFakeImage, kFakeImageSize);

  // After download completes, bl_start_ram_app polls APP_ID for APPLICATION.
  f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__APPLICATION);
  // ENABLE register for download_set_enable_ram read-modify-write.
  f.bus.preload(TMF8829_REG_ENABLE, TMF8829_ENABLE_PON_MASK);

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0);
  REQUIRE(rc == TMF8829_OK);
  REQUIRE(f.bus.fifo_read_queue[TMF8829_REG_CMD_STAT].empty());
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
  for (const auto& op : f.bus.ops) {
    if (op.kind == FakeBus::OpKind::Write && op.reg == TMF8829_REG_CMD_STAT) {
      REQUIRE(op.data[0] != TMF8829_BL_CMD_STAT_R_RAM);
    }
  }
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

  f.bus.write_error_after_n = 1;

  int rc = tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0);
  REQUIRE(rc == TMF8829_E_BUS);
  require_no_app_start(f);
}

TEST_CASE("download_firmware WR_RAM: restores addresses and verifies every chunk on both CPUs", "[tmf8829][download]") {
  Fixture f(GENERATE(TMF8829_BUS_I2C, TMF8829_BUS_SPI));
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read            = large_fw_read;
  const auto max_write           = GENERATE(0u, 18u, 6u);
  f.drv.max_write_len            = static_cast<std::uint16_t>(max_write);
  const std::uint8_t chunk_limit = (max_write == 0u) ? 128u : static_cast<std::uint8_t>(max_write - 2u);
  queue_ram_download(f, kLargeImage.data(), static_cast<std::uint32_t>(kLargeImage.size()), chunk_limit);
  f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__APPLICATION);

  const std::uint32_t start = TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT + 0x100u;
  REQUIRE(tmf8829_download_firmware(&f.drv, start, 0) == TMF8829_OK);
  std::vector<FakeBus::Op> writes;
  for (const auto& op : f.bus.ops) {
    if (op.kind == FakeBus::OpKind::Write && op.reg == TMF8829_REG_CMD_STAT) {
      writes.push_back(op);
      if (max_write != 0u) {
        REQUIRE(op.data.size() <= max_write);
      }
    }
  }
  std::size_t op_index = 0;
  for (std::size_t offset = 0; offset < kLargeImage.size();) {
    const auto len           = static_cast<std::uint8_t>((kLargeImage.size() - offset < chunk_limit) ? kLargeImage.size() - offset : chunk_limit);
    const std::uint32_t addr = start + static_cast<std::uint32_t>(offset);
    for (unsigned int stage = 0; stage < 3u; ++stage) {
      REQUIRE(op_index + 1u < writes.size());
      const auto& address = writes[op_index++].data;
      REQUIRE(address.size() == 6u);
      REQUIRE(address[0] == 0x43u);
      REQUIRE(address[1] == 4u);
      const std::uint32_t expected_addr = addr + ((stage == 2u) ? 0x01000000u : 0u);
      REQUIRE(tmf8829_get_uint32(address.data() + 2) == expected_addr);
      const auto& command = writes[op_index++].data;
      if (stage == 0u) {
        REQUIRE(command.size() == 2u + len);
        REQUIRE(command[0] == 0x42u);
        REQUIRE(command[1] == len);
        REQUIRE(std::memcmp(command.data() + 2, kLargeImage.data() + offset, len) == 0);
      } else {
        REQUIRE(command.size() == 3u);
        REQUIRE(command[0] == 0x40u);
        REQUIRE(command[1] == 1u);
        REQUIRE(command[2] == len);
      }
    }
    offset += len;
  }
  REQUIRE(op_index + 1u == writes.size());
  REQUIRE(writes.back().data[0] == TMF8829_BL_CMD_STAT_START_RAM_APP);
  REQUIRE(f.bus.fifo_read_queue[TMF8829_REG_CMD_STAT].empty());
}

TEST_CASE("download_firmware WR_RAM: mismatch identifies the CPU and prevents startup", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read       = fake_fw_read;
  const int cpu             = GENERATE(0, 1);
  const auto corrupt_offset = GENERATE(0u, kFakeImageSize - 1u);
  queue_write_status(f);
  for (int i = 0; i <= cpu; ++i) {
    auto response = ram_response(kFakeImage, kFakeImageSize);
    if (i == cpu) {
      response[TMF8829_BL_WR_HEADER + corrupt_offset] ^= 0x01u;
    }
    queue_readback(f, response);
  }
  const int expected_error = (cpu == 0) ? TMF8829_E_FW_VERIFY_CPU0 : TMF8829_E_FW_VERIFY_CPU1;
  REQUIRE(tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0) == expected_error);
  require_no_app_start(f);
}

TEST_CASE("download_firmware WR_RAM: invalid response header is a bootloader error", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;
  const int cpu       = GENERATE(0, 1);
  const int field     = GENERATE(0, 1);
  queue_write_status(f);
  for (int i = 0; i <= cpu; ++i) {
    auto response = ram_response(kFakeImage, kFakeImageSize);
    if (i == cpu) {
      response[static_cast<std::size_t>(field)] = 0xFFu;
    }
    queue_readback(f, response);
  }
  REQUIRE(tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0) == TMF8829_E_BOOTLOADER);
  require_no_app_start(f);
}

TEST_CASE("download_firmware WR_RAM: transport failures stop before startup", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;
  queue_ram_download(f, kFakeImage, kFakeImageSize);
  SECTION("each command write") {
    f.bus.write_error_after_n = GENERATE(0, 1, 2, 3, 4, 5);
  }
  SECTION("each status or payload read") {
    f.bus.read_error_after_n = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  }
  REQUIRE(tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0) == TMF8829_E_BUS);
  require_no_app_start(f);
}

TEST_CASE("download_firmware WR_RAM: readback waits for ready or times out", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  setup_bootloader_ready(f);
  f.drv.fw_image_read = fake_fw_read;
  const int cpu       = GENERATE(0, 1);
  const bool timeout  = GENERATE(false, true);
  queue_write_status(f);
  if (cpu == 1) {
    queue_readback(f, ram_response(kFakeImage, kFakeImageSize));
  }
  f.bus.fifo_push(TMF8829_REG_CMD_STAT, { TMF8829_BL_STAT_READY });
  const auto polls = timeout ? TMF8829_BL_R_RAM_TIMEOUT_MS + 1u : 1u;
  for (unsigned int i = 0; i < polls; ++i) {
    f.bus.fifo_push(TMF8829_REG_CMD_STAT, { 0x01u });
  }
  if (!timeout) {
    f.bus.fifo_push(TMF8829_REG_CMD_STAT, { TMF8829_BL_STAT_READY });
    const auto response = ram_response(kFakeImage, kFakeImageSize);
    f.bus.fifo_push(TMF8829_REG_CMD_STAT, response.data(), response.size());
    if (cpu == 0) {
      queue_readback(f, response);
    }
    f.bus.preload(TMF8829_REG_APP_ID, TMF8829_APP_ID__APPLICATION);
  }
  REQUIRE(tmf8829_download_firmware(&f.drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0) == (timeout ? TMF8829_E_TIMEOUT : TMF8829_OK));
  REQUIRE(f.clock.now_us == (timeout ? TMF8829_BL_R_RAM_TIMEOUT_MS * 1000u : 1000u));
  if (timeout) {
    require_no_app_start(f);
  }
}

TEST_CASE("download_firmware WR_RAM: rejects impossible address or write limits before transfers", "[tmf8829][download]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.drv.fw_image_read = fake_fw_read;
  std::uint32_t start = TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT;
  SECTION("address overflows when selecting CPU1") {
    start = UINT32_MAX;
  }
  SECTION("last chunk would overflow CPU1 address") {
    start = UINT32_MAX - TMF8829_BL_OTHER_CPU_RAM_OFFSET - kFakeImageSize + 2u;
  }
  SECTION("bus cannot send an address command") {
    f.drv.max_write_len = 5u;
  }
  SECTION("buffer cannot hold a complete readback") {
    f.drv.buffer_len = TMF8829_BL_WR_HEADER + kFakeImageSize - 1u;
  }
  REQUIRE(tmf8829_download_firmware(&f.drv, start, 0) == TMF8829_E_PARAM);
  REQUIRE(f.bus.ops.empty());
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

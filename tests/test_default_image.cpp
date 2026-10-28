// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
// Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
//
// Built only when tmf8829::default_image exists (TMF8829_INCLUDE_DEFAULT_IMAGE=ON).

#include <catch2/catch_all.hpp>
#include <cstdint>

extern "C" {
#include "tmf8829/tmf8829_types.h"

#include "tmf8829_fw_image.h"
#include "tmf8829_fw_source.h"
}

TEST_CASE("default_image_read: size-only probe", "[tmf8829][image]") {
  std::uint32_t sz = 0u;
  REQUIRE(tmf8829_fw_source_read(nullptr, 0u, nullptr, 0u, &sz) == 0);
  REQUIRE(sz == TMF8829_FW_IMAGE_NUM_BYTES);
  REQUIRE(sz == 0x386Cu);
}

TEST_CASE("default_image_read: first loader bytes", "[tmf8829][image]") {
  std::uint8_t chunk[4]{};
  REQUIRE(tmf8829_fw_source_read(nullptr, 0u, chunk, sizeof chunk, nullptr) == 4);
  REQUIRE(chunk[0] == 0x00u);
  REQUIRE(chunk[1] == 0xC0u);
  REQUIRE(chunk[2] == 0x01u);
  REQUIRE(chunk[3] == 0x00u);
}

TEST_CASE("default_image_read: offset past end returns zero", "[tmf8829][image]") {
  std::uint8_t one = 0u;
  REQUIRE(tmf8829_fw_source_read(nullptr, TMF8829_FW_IMAGE_NUM_BYTES, &one, 1u, nullptr) == 0);
}

TEST_CASE("default_image_read: firmware 1.2.200 tail is readable and clipped", "[tmf8829][image]") {
  std::uint8_t chunk[8]{};
  REQUIRE(tmf8829_fw_source_read(nullptr, 0x3868u, chunk, sizeof chunk, nullptr) == 4);
  REQUIRE(chunk[0] == 0xE5u);
  REQUIRE(chunk[1] == 0x03u);
  REQUIRE(chunk[2] == 0xC6u);
  REQUIRE(chunk[3] == 0x03u);
  REQUIRE(chunk[4] == 0u);
  REQUIRE(chunk[5] == 0u);
  REQUIRE(chunk[6] == 0u);
  REQUIRE(chunk[7] == 0u);
}

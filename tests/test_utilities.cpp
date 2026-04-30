// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors

#include <array>
#include <catch2/catch_all.hpp>
#include <cstdint>

#include "support/test_fixture.hpp"

extern "C" {
#include "tmf8829/tmf8829.h"
#include "tmf8829/tmf8829_regs.h"
}

using namespace tmf8829_test;

TEST_CASE("get_uint16 is little-endian payload order", "[tmf8829][util]") {
  std::array<std::uint8_t, 2> b{ 0x34, 0x12 };
  REQUIRE(tmf8829_get_uint16(b.data()) == 0x1234);
}

TEST_CASE("get_uint32 is little-endian", "[tmf8829][util]") {
  std::array<std::uint8_t, 4> b{ 0x78, 0x56, 0x34, 0x12 };
  REQUIRE(tmf8829_get_uint32(b.data()) == 0x12345678);
}

TEST_CASE("set_uint16 round-trips with get_uint16", "[tmf8829][util]") {
  std::array<std::uint8_t, 2> b{};
  tmf8829_set_uint16(0xCDEF, b.data());
  REQUIRE(tmf8829_get_uint16(b.data()) == 0xCDEF);
}

TEST_CASE("get_pixel_size matches peak and flag layout", "[tmf8829][util]") {
  /* 1 peak, flags off → 3 bytes per pixel */
  REQUIRE(tmf8829_get_pixel_size(1u) == 3u);
  /* 2 peaks + signal strength: 2 * (3 + 2) = 10 */
  std::uint8_t layout = static_cast<std::uint8_t>(2u | TMF8829_RESULT_FORMAT_SIG_STRENGTH_MASK);
  REQUIRE(tmf8829_get_pixel_size(layout) == 10u);
}

TEST_CASE("correct_distance with unity ratio is identity", "[tmf8829][util]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.drv._clk_corr_ratio_uq = static_cast<std::uint16_t>(1u << 15);
  REQUIRE(tmf8829_correct_distance(&f.drv, 1000) == 1000);
}

TEST_CASE("clk_correction_ratio_uq15 matches driver field after init", "[tmf8829][util]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  REQUIRE(tmf8829_clk_correction_ratio_uq15(&f.drv) == f.drv._clk_corr_ratio_uq);
}

// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
// Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
//
// Tests for tmf8829_get_and_clr_interrupts.

#include <catch2/catch_all.hpp>

#include "support/test_fixture.hpp"

using tmf8829_test::FakeBus;
using tmf8829_test::Fixture;

TEST_CASE("get_and_clr_interrupts: nothing pending issues only a read", "[tmf8829][irq]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.bus.preload(TMF8829_REG_INT_STATUS, 0x00);

  std::uint8_t pending = 0xAA;
  REQUIRE(tmf8829_get_and_clr_interrupts(&f.drv, TMF8829_INT_ALL, &pending) == TMF8829_OK);
  REQUIRE(pending == 0u);

  REQUIRE(f.bus.ops.size() == 1);
  REQUIRE(f.bus.ops[0].kind == FakeBus::OpKind::Read);
  REQUIRE(f.bus.ops[0].reg == TMF8829_REG_INT_STATUS);
}

TEST_CASE("get_and_clr_interrupts: pending bits are returned and acked", "[tmf8829][irq]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  const std::uint8_t raised = TMF8829_INT_RESULTS | TMF8829_INT_HISTOGRAMS;
  f.bus.preload(TMF8829_REG_INT_STATUS, raised);

  std::uint8_t pending = 0u;
  REQUIRE(tmf8829_get_and_clr_interrupts(&f.drv, TMF8829_INT_ALL, &pending) == TMF8829_OK);
  REQUIRE(pending == raised);

  // Read followed by a write-back of the *same* bits to clear them.
  REQUIRE(f.bus.ops.size() == 2);
  REQUIRE(f.bus.ops[0].kind == FakeBus::OpKind::Read);
  REQUIRE(f.bus.ops[0].reg == TMF8829_REG_INT_STATUS);
  REQUIRE(f.bus.ops[1].kind == FakeBus::OpKind::Write);
  REQUIRE(f.bus.ops[1].reg == TMF8829_REG_INT_STATUS);
  REQUIRE(f.bus.ops[1].data.size() == 1);
  REQUIRE(f.bus.ops[1].data[0] == raised);
}

TEST_CASE("get_and_clr_interrupts: bits outside the mask are not cleared", "[tmf8829][irq]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // Both result and motion bits raised on the device.
  f.bus.preload(TMF8829_REG_INT_STATUS, TMF8829_INT_RESULTS | TMF8829_INT_MOTION);

  std::uint8_t pending = 0xFF;
  // Caller is only interested in result frames.
  REQUIRE(tmf8829_get_and_clr_interrupts(&f.drv, TMF8829_INT_RESULTS, &pending) == TMF8829_OK);
  REQUIRE(pending == TMF8829_INT_RESULTS);

  // The driver must only write back the RESULTS bit (write-1-to-clear on
  // the real device leaves MOTION pending). We check the driver's *intent*
  // by inspecting what it wrote; FakeBus is a flat register file and does
  // not model hardware write-1-to-clear semantics.
  REQUIRE(f.bus.ops.size() == 2);
  REQUIRE(f.bus.ops[1].kind == FakeBus::OpKind::Write);
  REQUIRE(f.bus.ops[1].reg == TMF8829_REG_INT_STATUS);
  REQUIRE(f.bus.ops[1].data.size() == 1);
  REQUIRE(f.bus.ops[1].data[0] == TMF8829_INT_RESULTS);
}

TEST_CASE("get_and_clr_interrupts: read failure yields E_BUS", "[tmf8829][irq]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.bus.read_error_after_n = 0;

  std::uint8_t pending = 0u;
  REQUIRE(tmf8829_get_and_clr_interrupts(&f.drv, TMF8829_INT_ALL, &pending) == TMF8829_E_BUS);
  // No write attempted after a failed read.
  REQUIRE(f.bus.ops.size() == 1);
}

TEST_CASE("get_and_clr_interrupts: write failure yields E_BUS", "[tmf8829][irq]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.bus.preload(TMF8829_REG_INT_STATUS, TMF8829_INT_RESULTS);
  f.bus.write_error_after_n = 0;

  std::uint8_t pending = 0u;
  REQUIRE(tmf8829_get_and_clr_interrupts(&f.drv, TMF8829_INT_ALL, &pending) == TMF8829_E_BUS);
  REQUIRE(f.bus.ops.size() == 2);
  REQUIRE(f.bus.ops[0].kind == FakeBus::OpKind::Read);
  REQUIRE(f.bus.ops[1].kind == FakeBus::OpKind::Write);
}

TEST_CASE("get_and_clr_interrupts: rejects null out pointer", "[tmf8829][irq]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  REQUIRE(tmf8829_get_and_clr_interrupts(&f.drv, TMF8829_INT_ALL, nullptr) == TMF8829_E_PARAM);
  // Implementation must not touch the bus on parameter rejection.
  REQUIRE(f.bus.ops.empty());
}

TEST_CASE("get_and_clr_interrupts: rejects uninitialised driver", "[tmf8829][irq]") {
  tmf8829_driver_t drv{};
  std::uint8_t pending = 0u;
  REQUIRE(tmf8829_get_and_clr_interrupts(&drv, TMF8829_INT_ALL, &pending) == TMF8829_E_PARAM);
}

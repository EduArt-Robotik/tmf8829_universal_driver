// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
//
// Test Fixture: wires FakeBus, FakeClock, and FakePin into a
// tmf8829_driver_t with a complete tmf8829_ops_t and a min-sized buffer.
// Tests construct one Fixture per TEST_CASE (typical Catch2 idiom).

#pragma once

#include <array>
#include <cstdint>

#include "fake_bus.hpp"
#include "fake_clock.hpp"
#include "fake_pin.hpp"

extern "C" {
#include "tmf8829/tmf8829.h"
}

namespace tmf8829_test {

class Fixture {
public:
  FakeBus bus;
  FakeClock clock;
  FakePin pin;

  std::array<std::uint8_t, TMF8829_MIN_BUFFER_SIZE> buffer{};
  tmf8829_driver_t drv{};
  tmf8829_ops_t ops{};

  explicit Fixture(tmf8829_bus_t bus_kind = TMF8829_BUS_I2C, std::uint8_t i2c_addr = TMF8829_DEFAULT_I2C_ADDR) {
    // Bind ops -> static C thunks. Each thunk recovers its target from
    // drv->user_ctx (which is set to "this") or, for delay/systick, from
    // FakeClock::active.
    ops.read             = &Fixture::c_read;
    ops.write            = &Fixture::c_write;
    ops.delay_us         = &FakeClock::c_delay;
    ops.systick_us       = &FakeClock::c_systick;
    ops.write_pin_enable = &Fixture::c_write_pin_enable;
    ops.read_pin_int     = &Fixture::c_read_pin_int;

    drv.bus        = bus_kind;
    drv.i2c_addr   = i2c_addr;
    drv.user_ctx   = this;
    drv.buffer     = buffer.data();
    drv.buffer_len = static_cast<std::uint16_t>(buffer.size());

    FakeClock::active = &clock;
  }

  ~Fixture() {
    if (FakeClock::active == &clock) {
      FakeClock::active = nullptr;
    }
  }

  Fixture(const Fixture&)            = delete;
  Fixture& operator=(const Fixture&) = delete;

  /// Initialise the driver with the wired ops.  Returns the init result.
  int init() { return tmf8829_init(&drv, &ops); }

private:
  static Fixture* self(tmf8829_driver_t* drv) { return static_cast<Fixture*>(drv->user_ctx); }

  static int c_read(tmf8829_driver_t* drv, std::uint8_t reg, std::uint8_t* buf, std::uint16_t len) { return self(drv)->bus.do_read(reg, buf, len); }
  static int c_write(tmf8829_driver_t* drv, std::uint8_t reg, const std::uint8_t* buf, std::uint16_t len) { return self(drv)->bus.do_write(reg, buf, len); }
  static void c_write_pin_enable(tmf8829_driver_t* drv, int high) { self(drv)->pin.write_pin_enable(high); }
  static int c_read_pin_int(tmf8829_driver_t* drv) { return self(drv)->pin.read_pin_int(); }
};

} // namespace tmf8829_test

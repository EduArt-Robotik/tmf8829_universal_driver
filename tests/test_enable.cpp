// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
//
// Tests for the enable / disable / CPU-ready bring-up path.

#include <catch2/catch_all.hpp>

#include "support/test_fixture.hpp"

using tmf8829_test::Fixture;
using tmf8829_test::FakeBus;

TEST_CASE("enable runs the canonical bring-up sequence", "[tmf8829][enable]")
{
    Fixture f;
    REQUIRE(f.init() == TMF8829_OK);

    // Pre-set CPU ready so the very first poll succeeds.
    f.bus.preload(TMF8829_REG_ENABLE, TMF8829_ENABLE_CPU_READY_MASK);

    REQUIRE(tmf8829_enable(&f.drv) == TMF8829_OK);

    // Pin: drive low, then high (in that order).
    REQUIRE(f.pin.enable_history.size() == 2);
    REQUIRE(f.pin.enable_history[0] == 0);
    REQUIRE(f.pin.enable_history[1] == 1);

    // Delays: cap-discharge then ramp. The CPU-ready poll succeeded on the
    // very first read so no additional 1 ms delays were issued.
    REQUIRE(f.clock.delays.size() == 2);
    REQUIRE(f.clock.delays[0] == TMF8829_ENABLE_CAP_DISCHARGE_US);
    REQUIRE(f.clock.delays[1] == TMF8829_ENABLE_RAMP_US);

    // Exactly one bus operation: a 1-byte read of the ENABLE register.
    REQUIRE(f.bus.ops.size() == 1);
    REQUIRE(f.bus.ops[0].kind == FakeBus::OpKind::Read);
    REQUIRE(f.bus.ops[0].reg  == TMF8829_REG_ENABLE);
    REQUIRE(f.bus.ops[0].data.size() == 1);
    REQUIRE(f.bus.ops[0].data[0] == TMF8829_ENABLE_CPU_READY_MASK);
}

TEST_CASE("enable returns timeout if CPU never asserts ready", "[tmf8829][enable]")
{
    Fixture f;
    REQUIRE(f.init() == TMF8829_OK);

    // Leave reg_file[ENABLE] == 0; CPU ready bit is never observed.
    REQUIRE(tmf8829_enable(&f.drv) == TMF8829_E_TIMEOUT);

    // Pin sequence still ran to completion.
    REQUIRE(f.pin.enable_history.size() == 2);
    REQUIRE(f.pin.enable_history[0] == 0);
    REQUIRE(f.pin.enable_history[1] == 1);

    // Polling did 1 + TMF8829_CPU_READY_TIMEOUT_MS reads of the ENABLE
    // register, with TMF8829_CPU_READY_TIMEOUT_MS 1 ms-delays between them.
    const std::size_t expected_reads = 1 + TMF8829_CPU_READY_TIMEOUT_MS;
    REQUIRE(f.bus.ops.size() == expected_reads);
    for (const auto &op : f.bus.ops) {
        REQUIRE(op.kind == FakeBus::OpKind::Read);
        REQUIRE(op.reg  == TMF8829_REG_ENABLE);
    }

    // Cap-discharge + ramp + (timeout_ms) inter-poll delays.
    const std::size_t expected_delays = 2 + TMF8829_CPU_READY_TIMEOUT_MS;
    REQUIRE(f.clock.delays.size() == expected_delays);
    // Last `timeout_ms` delays are the 1 ms sleeps inside is_cpu_ready.
    for (std::size_t i = 2; i < expected_delays; ++i) {
        REQUIRE(f.clock.delays[i] == 1000u);
    }
}

TEST_CASE("enable propagates bus errors", "[tmf8829][enable]")
{
    Fixture f;
    REQUIRE(f.init() == TMF8829_OK);

    f.bus.read_error_after_n = 0; // fail the very first ENABLE read
    REQUIRE(tmf8829_enable(&f.drv) == TMF8829_E_BUS);
}

TEST_CASE("disable drives the enable pin low", "[tmf8829][enable]")
{
    Fixture f;
    REQUIRE(f.init() == TMF8829_OK);

    REQUIRE(tmf8829_disable(&f.drv) == TMF8829_OK);
    REQUIRE(f.pin.enable_history.size() == 1);
    REQUIRE(f.pin.enable_history[0] == 0);
    // No bus traffic, no clock service used.
    REQUIRE(f.bus.ops.empty());
    REQUIRE(f.clock.delays.empty());
}

TEST_CASE("enable / disable reject uninitialised driver", "[tmf8829][enable]")
{
    tmf8829_driver_t drv{}; // never initialised
    REQUIRE(tmf8829_enable(&drv)        == TMF8829_E_PARAM);
    REQUIRE(tmf8829_disable(&drv)       == TMF8829_E_PARAM);
    REQUIRE(tmf8829_is_cpu_ready(&drv, 5) == TMF8829_E_PARAM);
}

TEST_CASE("enable works identically with bus = SPI", "[tmf8829][enable][bus]")
{
    // The driver core is bus-agnostic. Switching drv.bus to SPI must not
    // change which registers are touched or in which order — the host
    // port's read/write callbacks are what handle bus-specific framing.
    Fixture f(TMF8829_BUS_SPI, /*i2c_addr=*/0);
    REQUIRE(f.init() == TMF8829_OK);

    f.bus.preload(TMF8829_REG_ENABLE, TMF8829_ENABLE_CPU_READY_MASK);
    REQUIRE(tmf8829_enable(&f.drv) == TMF8829_OK);

    REQUIRE(f.bus.ops.size() == 1);
    REQUIRE(f.bus.ops[0].reg == TMF8829_REG_ENABLE);
    REQUIRE(f.pin.enable_history == std::vector<int>{0, 1});
}

TEST_CASE("is_cpu_ready returns OK as soon as the bit is set",
          "[tmf8829][cpu_ready]")
{
    Fixture f;
    REQUIRE(f.init() == TMF8829_OK);
    f.bus.preload(TMF8829_REG_ENABLE, TMF8829_ENABLE_CPU_READY_MASK);

    REQUIRE(tmf8829_is_cpu_ready(&f.drv, 10) == TMF8829_OK);
    REQUIRE(f.bus.ops.size() == 1);
    REQUIRE(f.clock.delays.empty()); // no sleep on a first-read hit
}

TEST_CASE("is_cpu_ready returns TIMEOUT after exhausting the budget",
          "[tmf8829][cpu_ready]")
{
    Fixture f;
    REQUIRE(f.init() == TMF8829_OK);
    // Bit never set.
    REQUIRE(tmf8829_is_cpu_ready(&f.drv, 4) == TMF8829_E_TIMEOUT);

    // 1 + timeout_ms reads, timeout_ms 1ms-delays between them.
    REQUIRE(f.bus.ops.size() == 5);
    REQUIRE(f.clock.delays.size() == 4);
    for (std::uint32_t d : f.clock.delays) {
        REQUIRE(d == 1000u);
    }
}

TEST_CASE("is_cpu_ready returns BUS error when read fails",
          "[tmf8829][cpu_ready]")
{
    Fixture f;
    REQUIRE(f.init() == TMF8829_OK);
    f.bus.read_error_after_n = 0;
    REQUIRE(tmf8829_is_cpu_ready(&f.drv, 10) == TMF8829_E_BUS);
    REQUIRE(f.bus.ops.size() == 1);
}

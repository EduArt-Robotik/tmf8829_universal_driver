/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Smoke test: confirms the public headers parse, the static library links,
 * basic types are visible, and tmf8829_init enforces the parameter contract.
 *
 * Behavioural tests for enable / IRQ / measurement come in subsequent
 * batches; they will live in their own translation units alongside this one.
 */

#include <catch2/catch_all.hpp>

#include <array>
#include <cstdint>
#include <cstring>

extern "C" {
#include "tmf8829/tmf8829.h"
}

namespace {

/* Trivial fakes used only to build a "well-formed" ops table. They record
 * nothing; behavioural tests will introduce a richer fake later. */

int   fake_read (tmf8829_driver_t*, uint8_t, uint8_t*, uint16_t)        { return 0; }
int   fake_write(tmf8829_driver_t*, uint8_t, const uint8_t*, uint16_t)  { return 0; }
void  fake_delay_us (uint32_t)                                           {}
uint32_t fake_systick_us(void)                                           { return 0; }
void  fake_write_pin_enable(tmf8829_driver_t*, int)                      {}

const tmf8829_ops_t kCompleteOps = {
    fake_read,
    fake_write,
    fake_delay_us,
    fake_systick_us,
    fake_write_pin_enable,
    /* read_pin_int = */ nullptr,
};

/* Minimal driver instance for tests that need a "valid" config. */
struct ScratchedDriver {
    std::array<std::uint8_t, TMF8829_MIN_SCRATCH_SIZE> buf{};
    tmf8829_driver_t drv{};

    ScratchedDriver()
    {
        drv.bus         = TMF8829_BUS_I2C;
        drv.i2c_addr    = TMF8829_DEFAULT_I2C_ADDR;
        drv.scratch     = buf.data();
        drv.scratch_len = static_cast<std::uint16_t>(buf.size());
    }
};

} /* namespace */

TEST_CASE("error code conventions", "[tmf8829][smoke]")
{
    REQUIRE(TMF8829_OK == 0);
    REQUIRE(TMF8829_E_PARAM           < 0);
    REQUIRE(TMF8829_E_BUS             < 0);
    REQUIRE(TMF8829_E_TIMEOUT         < 0);
    REQUIRE(TMF8829_E_NO_RESULT       < 0);
    REQUIRE(TMF8829_E_BOOTLOADER      < 0);
    REQUIRE(TMF8829_E_STATE           < 0);
    REQUIRE(TMF8829_E_NOT_IMPLEMENTED < 0);
}

TEST_CASE("public types have non-zero size", "[tmf8829][smoke]")
{
    STATIC_REQUIRE(sizeof(tmf8829_driver_t) > 0);
    STATIC_REQUIRE(sizeof(tmf8829_ops_t)    > 0);
    STATIC_REQUIRE(sizeof(tmf8829_bus_t)    > 0);
    STATIC_REQUIRE(sizeof(tmf8829_err_t)    > 0);
}

TEST_CASE("driver version macros are populated", "[tmf8829][smoke]")
{
    REQUIRE(TMF8829_DRIVER_VERSION_MAJOR >= 0);
    REQUIRE(TMF8829_DRIVER_VERSION_MINOR >= 0);
    REQUIRE(TMF8829_DRIVER_VERSION_PATCH >= 0);
}

TEST_CASE("init rejects null driver", "[tmf8829][init]")
{
    REQUIRE(tmf8829_init(nullptr, &kCompleteOps) == TMF8829_E_PARAM);
}

TEST_CASE("init rejects null ops", "[tmf8829][init]")
{
    ScratchedDriver d;
    REQUIRE(tmf8829_init(&d.drv, nullptr) == TMF8829_E_PARAM);
}

TEST_CASE("init rejects ops missing required callbacks", "[tmf8829][init]")
{
    ScratchedDriver d;
    tmf8829_ops_t partial = {};
    REQUIRE(tmf8829_init(&d.drv, &partial) == TMF8829_E_PARAM);

    /* read alone is not enough */
    partial.read = fake_read;
    REQUIRE(tmf8829_init(&d.drv, &partial) == TMF8829_E_PARAM);
}

TEST_CASE("init rejects missing scratch", "[tmf8829][init]")
{
    tmf8829_driver_t drv{};
    drv.bus      = TMF8829_BUS_I2C;
    drv.i2c_addr = TMF8829_DEFAULT_I2C_ADDR;
    /* No scratch buffer */
    REQUIRE(tmf8829_init(&drv, &kCompleteOps) == TMF8829_E_PARAM);
}

TEST_CASE("init rejects undersized scratch", "[tmf8829][init]")
{
    std::array<std::uint8_t, 32> tiny{};
    tmf8829_driver_t drv{};
    drv.bus         = TMF8829_BUS_I2C;
    drv.i2c_addr    = TMF8829_DEFAULT_I2C_ADDR;
    drv.scratch     = tiny.data();
    drv.scratch_len = static_cast<std::uint16_t>(tiny.size());
    REQUIRE(tmf8829_init(&drv, &kCompleteOps) == TMF8829_E_PARAM);
}

TEST_CASE("init rejects invalid I2C address", "[tmf8829][init]")
{
    ScratchedDriver d;
    d.drv.i2c_addr = 0xFF; /* > 0x7F: not a valid 7-bit address */
    REQUIRE(tmf8829_init(&d.drv, &kCompleteOps) == TMF8829_E_PARAM);
}

TEST_CASE("init succeeds with a valid I2C configuration", "[tmf8829][init]")
{
    ScratchedDriver d;
    REQUIRE(tmf8829_init(&d.drv, &kCompleteOps) == TMF8829_OK);
    REQUIRE(d.drv.ops == &kCompleteOps);
}

TEST_CASE("init succeeds with a valid SPI configuration", "[tmf8829][init]")
{
    ScratchedDriver d;
    d.drv.bus      = TMF8829_BUS_SPI;
    d.drv.i2c_addr = 0; /* unused for SPI */
    REQUIRE(tmf8829_init(&d.drv, &kCompleteOps) == TMF8829_OK);
    REQUIRE(d.drv.bus == TMF8829_BUS_SPI);
}

TEST_CASE("set_log_level requires an initialised driver", "[tmf8829][init]")
{
    tmf8829_driver_t drv{};
    REQUIRE(tmf8829_set_log_level(&drv, TMF8829_LOG_INFO) == TMF8829_E_PARAM);

    ScratchedDriver d;
    REQUIRE(tmf8829_init(&d.drv, &kCompleteOps) == TMF8829_OK);
    REQUIRE(tmf8829_set_log_level(&d.drv, TMF8829_LOG_INFO) == TMF8829_OK);
    REQUIRE(d.drv.log_level == TMF8829_LOG_INFO);
}

TEST_CASE("stubbed entry points return E_NOT_IMPLEMENTED on initialised driver",
          "[tmf8829][stubs]")
{
    ScratchedDriver d;
    REQUIRE(tmf8829_init(&d.drv, &kCompleteOps) == TMF8829_OK);

    REQUIRE(tmf8829_enable(&d.drv)               == TMF8829_E_NOT_IMPLEMENTED);
    REQUIRE(tmf8829_disable(&d.drv)              == TMF8829_E_NOT_IMPLEMENTED);

    std::uint8_t mask = 0xAB;
    REQUIRE(tmf8829_handle_irq(&d.drv, &mask)    == TMF8829_E_NOT_IMPLEMENTED);
    REQUIRE(mask == 0u); /* mask must be cleared even on stubbed return */
}

/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 */

/**
 * @file tmf8829_ops.h
 * @brief Platform callback table (@ref tmf8829_ops_t) for bus, time, and GPIO.
 *
 * One table is typically @c static const and shared by all @ref tmf8829_driver_t
 * instances on the same board.
 */

#ifndef TMF8829_OPS_H
#define TMF8829_OPS_H

#include "tmf8829_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read @p len bytes from register @p reg into @p buf.
 *
 * Implementations map @p drv->bus, @p drv->i2c_addr, and @p drv->user_ctx to
 * the underlying transfer (e.g. I2C memory read at sub-address @p reg).
 *
 * @param[in]  reg  First register address on the device.
 * @param[out] buf  Destination buffer.
 * @param[in]  len  Bytes to read.
 *
 * @return @c 0 on success, negative on failure.
 */
typedef int (*tmf8829_read_fn)(tmf8829_driver_t* drv, uint8_t reg, uint8_t* buf, uint16_t len);

/**
 * @brief Write @p len bytes from @p buf starting at register @p reg.
 *
 * @return @c 0 on success, negative on failure.
 */
typedef int (*tmf8829_write_fn)(tmf8829_driver_t* drv, uint8_t reg, const uint8_t* buf, uint16_t len);

/**
 * @brief Busy-wait for at least @p us microseconds (resolution is host-defined).
 */
typedef void (*tmf8829_delay_us_fn)(uint32_t us);

/**
 * @brief Free-running microsecond tick (32-bit wrap allowed; only differences are used).
 */
typedef uint32_t (*tmf8829_systick_us_fn)(void);

/**
 * @brief Drive the sensor enable pin: low if @p high is zero, else high.
 *
 * Pin mux / mode must be configured before @ref tmf8829_init.
 */
typedef void (*tmf8829_write_pin_enable_fn)(tmf8829_driver_t* drv, int high);

/**
 * @brief Optional: read the IRQ GPIO level for this instance.
 *
 * May be @c NULL if the host polls @ref TMF8829_REG_INT_STATUS only.
 *
 * @return @c 1 if the interrupt line is asserted, @c 0 if deasserted, negative on error.
 */
typedef int (*tmf8829_read_pin_int_fn)(tmf8829_driver_t* drv);

/**
 * @brief Set of platform callbacks required by @ref tmf8829_init.
 *
 * Declare one @c const instance per port (e.g. STM32 HAL) and pass its address
 * to every sensor on that port.
 */
struct tmf8829_ops {
  tmf8829_read_fn read;                         /**< Required: register read. */
  tmf8829_write_fn write;                       /**< Required: register write. */
  tmf8829_delay_us_fn delay_us;                 /**< Required: microsecond delay. */
  tmf8829_systick_us_fn systick_us;             /**< Required: microsecond time base. */
  tmf8829_write_pin_enable_fn write_pin_enable; /**< Required: enable power pin. */
  tmf8829_read_pin_int_fn read_pin_int;         /**< Optional: IRQ pin; may be @c NULL. */
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_OPS_H */

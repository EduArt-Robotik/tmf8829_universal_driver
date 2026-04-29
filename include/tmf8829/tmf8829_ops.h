/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Platform operations table. The host provides one @ref tmf8829_ops_t
 * (typically per project, shared by all driver instances) and hands a pointer
 * to it to @ref tmf8829_init.
 */

#ifndef TMF8829_OPS_H
#define TMF8829_OPS_H

#include "tmf8829_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read @p len bytes starting at register @p reg into @p buf.
 *
 * The driver uses @c drv->bus, @c drv->i2c_addr, and @c drv->user_ctx to
 * locate the underlying bus handle. The implementation is responsible for
 * mapping that into the target framework's transfer call (HAL_I2C_Mem_Read,
 * spi_write_then_read, Wire.requestFrom, ...).
 *
 * @return 0 on success, negative on bus error.
 */
typedef int (*tmf8829_read_fn)(tmf8829_driver_t *drv,
                               uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief Write @p len bytes starting at register @p reg from @p buf.
 *
 * @return 0 on success, negative on bus error.
 */
typedef int (*tmf8829_write_fn)(tmf8829_driver_t *drv,
                                uint8_t reg, const uint8_t *buf, uint16_t len);

/**
 * @brief Block for at least @p us microseconds. Resolution is host-defined;
 *        the driver only relies on the call returning after the requested
 *        amount of time.
 */
typedef void (*tmf8829_delay_us_fn)(uint32_t us);

/**
 * @brief Return a free-running microsecond tick. Wrap-around is permitted;
 *        the driver only takes differences.
 */
typedef uint32_t (*tmf8829_systick_us_fn)(void);

/**
 * @brief Drive the sensor's enable pin high (@p high non-zero) or low.
 *
 * Pin direction / pull configuration is the host's responsibility and must
 * already be in place by the time @ref tmf8829_init runs.
 */
typedef void (*tmf8829_write_pin_enable_fn)(tmf8829_driver_t *drv, int high);

/**
 * @brief Read the IRQ pin level. Optional; may be set to NULL when the host
 *        doesn't wire the IRQ pin or polls register state instead.
 *
 * @return 1 if asserted (high), 0 if deasserted (low), negative on error.
 */
typedef int (*tmf8829_read_pin_int_fn)(tmf8829_driver_t *drv);

/**
 * @brief Set of platform callbacks the driver depends on.
 *
 * One @ref tmf8829_ops_t typically describes a target platform (e.g. STM32
 * HAL on a particular board) and is shared by every TMF8829 driver instance
 * on that platform. The struct is intentionally const-friendly; you can
 * declare it as @c static @c const in your port file.
 *
 * **Required fields**: @ref read, @ref write, @ref delay_us, @ref systick_us,
 *                      @ref write_pin_enable.
 *
 * **Optional fields**: @ref read_pin_int.
 */
struct tmf8829_ops
{
    tmf8829_read_fn               read;             /**< (required) */
    tmf8829_write_fn              write;            /**< (required) */
    tmf8829_delay_us_fn           delay_us;         /**< (required) */
    tmf8829_systick_us_fn         systick_us;       /**< (required) */
    tmf8829_write_pin_enable_fn   write_pin_enable; /**< (required) */
    tmf8829_read_pin_int_fn       read_pin_int;     /**< (optional, may be NULL) */
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_OPS_H */

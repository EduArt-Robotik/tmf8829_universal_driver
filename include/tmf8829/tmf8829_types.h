/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 * Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
 */

/**
 * @file tmf8829_types.h
 * @brief Public typedefs, errors, log levels, and compile-time limits.
 *
 * Includes only @c <stdint.h> and @c <stddef.h>. Safe to include before any
 * platform headers.
 */

#ifndef TMF8829_TYPES_H
#define TMF8829_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Forward declarations: full structs are in @ref tmf8829.h and @ref tmf8829_ops.h.
 * Typedefs are here so any header can use @ref tmf8829_driver_t and @ref tmf8829_ops_t.
 */
struct tmf8829_driver;
struct tmf8829_ops;

typedef struct tmf8829_driver tmf8829_driver_t;
typedef struct tmf8829_ops tmf8829_ops_t;

/* ------------------------------------------------------------------ */
/* Bus selection                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Physical bus used to reach this device instance.
 */
typedef enum tmf8829_bus {
  TMF8829_BUS_I2C = 0, /**< Register access via I2C. */
  TMF8829_BUS_SPI = 1, /**< Register access via SPI. */
} tmf8829_bus_t;

/* ------------------------------------------------------------------ */
/* Error / status codes                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Return codes from public driver functions.
 *
 * Functions return @c int: @c 0 (@ref TMF8829_OK) on success, negative values
 * from this enumeration on failure.
 *
 * @note Host-supplied @ref tmf8829_ops_t callbacks should use the same
 *       convention: @c 0 on success, negative on error.
 */
typedef enum tmf8829_err {
  TMF8829_OK                = 0,   /**< Success. */
  TMF8829_E_PARAM           = -1,  /**< Invalid argument or bad state. */
  TMF8829_E_BUS             = -2,  /**< Underlying bus read/write failed. */
  TMF8829_E_TIMEOUT         = -3,  /**< Poll or wait exceeded budget. */
  TMF8829_E_NO_RESULT       = -4,  /**< No frame / wrong frame type / bad EOF. */
  TMF8829_E_BOOTLOADER      = -5,  /**< Bootloader rejected command or bad image. */
  TMF8829_E_STATE           = -6,  /**< Operation invalid for current mode. */
  TMF8829_E_NOT_IMPLEMENTED = -99, /**< Reserved for stubbed entry points. */
} tmf8829_err_t;

/* ------------------------------------------------------------------ */
/* Logging                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Bit-mask log levels (ams-OSRAM compatible).
 *
 * Combine with bitwise OR. Compared against @ref tmf8829_driver_t::log_level when
 * the driver invokes @ref tmf8829_driver_t::log.
 */
enum tmf8829_log_level {
  TMF8829_LOG_NONE           = 0x00, /**< No categories. */
  TMF8829_LOG_ERROR          = 0x01, /**< Errors. */
  TMF8829_LOG_RESULTS_HEADER = 0x02, /**< Result stream headers. */
  TMF8829_LOG_RESULTS        = 0x04, /**< Result payload detail. */
  TMF8829_LOG_CLK_CORRECTION = 0x08, /**< Clock correction updates. */
  TMF8829_LOG_INFO           = 0x10, /**< General information. */
  TMF8829_LOG_VERBOSE        = 0x20, /**< Verbose tracing. */
  TMF8829_LOG_BUS            = 0x80, /**< Low-level bus traffic. */
  TMF8829_LOG_DEBUG          = 0xFF, /**< All categories. */
};

/* ------------------------------------------------------------------ */
/* Compile-time configuration                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Ring buffer depth for (host_tick, device_tick) clock-correction pairs.
 *
 * Must be a power of two. Overridable with a compile-time @c -D.
 * @hideinitializer
 */
#ifndef TMF8829_CLK_CORRECTION_PAIRS
#define TMF8829_CLK_CORRECTION_PAIRS 4u
#endif

/**
 * @brief Minimum @ref tmf8829_driver_t::buffer_len in bytes.
 *
 * Used for register staging and bootloader payloads. Larger buffers are required
 * for FIFO download with big chunks.
 * @hideinitializer
 */
#ifndef TMF8829_MIN_BUFFER_SIZE
#define TMF8829_MIN_BUFFER_SIZE 256u
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_TYPES_H */

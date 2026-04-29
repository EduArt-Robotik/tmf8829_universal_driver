/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Public type definitions and forward declarations.
 *
 * This header pulls in only <stdint.h> / <stddef.h>. Including it must never
 * cause platform headers to be visible to the consumer.
 */

#ifndef TMF8829_TYPES_H
#define TMF8829_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations. The full struct definitions live in their respective
 * public headers (tmf8829.h for tmf8829_driver, tmf8829_ops.h for tmf8829_ops),
 * but the typedef names are introduced here so all public headers can refer to
 * them without ordering constraints. */
struct tmf8829_driver;
struct tmf8829_ops;

typedef struct tmf8829_driver tmf8829_driver_t;
typedef struct tmf8829_ops    tmf8829_ops_t;

/* ------------------------------------------------------------------ */
/* Bus selection                                                      */
/* ------------------------------------------------------------------ */

/** Which bus a particular driver instance is wired to. */
typedef enum tmf8829_bus
{
    TMF8829_BUS_I2C = 0,
    TMF8829_BUS_SPI = 1,
} tmf8829_bus_t;

/* ------------------------------------------------------------------ */
/* Error / status codes                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Return codes from public driver functions.
 *
 * Functions return @c int. A return value of @c 0 (@ref TMF8829_OK) indicates
 * success. Any negative value indicates an error from this enumeration.
 *
 * @note Callbacks supplied by the host should follow the same convention:
 *       0 on success, negative on error.
 */
typedef enum tmf8829_err
{
    TMF8829_OK                  =   0,  /**< Success. */
    TMF8829_E_PARAM             =  -1,  /**< Invalid argument. */
    TMF8829_E_BUS               =  -2,  /**< Underlying bus read/write callback failed. */
    TMF8829_E_TIMEOUT           =  -3,  /**< Operation timed out (CPU not ready, no ack, ...). */
    TMF8829_E_NO_RESULT         =  -4,  /**< No measurement result available. */
    TMF8829_E_BOOTLOADER        =  -5,  /**< Bootloader command rejected or image bad. */
    TMF8829_E_STATE             =  -6,  /**< Operation invalid in current driver state. */
    TMF8829_E_NOT_IMPLEMENTED   = -99,  /**< Feature stubbed during development. */
} tmf8829_err_t;

/* ------------------------------------------------------------------ */
/* Logging                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Bit-mask log levels.
 *
 * Values mirror the bit pattern used by the ams-OSRAM reference drivers so
 * existing log-level configuration carries over. Combine with bitwise OR.
 */
enum tmf8829_log_level
{
    TMF8829_LOG_NONE            = 0x00,
    TMF8829_LOG_ERROR           = 0x01,
    TMF8829_LOG_RESULTS_HEADER  = 0x02,
    TMF8829_LOG_RESULTS         = 0x04,
    TMF8829_LOG_CLK_CORRECTION  = 0x08,
    TMF8829_LOG_INFO            = 0x10,
    TMF8829_LOG_VERBOSE         = 0x20,
    TMF8829_LOG_BUS             = 0x80,
    TMF8829_LOG_DEBUG           = 0xFF,
};

/* ------------------------------------------------------------------ */
/* Compile-time configuration                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Number of (host_tick, device_tick) pairs kept for clock correction.
 *
 * Must be a power of two. Override on the compile command line if needed.
 */
#ifndef TMF8829_CLK_CORRECTION_PAIRS
#  define TMF8829_CLK_CORRECTION_PAIRS  4u
#endif

/**
 * @brief Minimum size of the caller-provided scratch buffer, in bytes.
 *
 * The driver uses the scratch buffer for register transfers and for
 * staging firmware-download chunks. Callers may provide more; less will be
 * rejected by @c tmf8829_init.
 */
#ifndef TMF8829_MIN_SCRATCH_SIZE
#  define TMF8829_MIN_SCRATCH_SIZE      256u
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_TYPES_H */

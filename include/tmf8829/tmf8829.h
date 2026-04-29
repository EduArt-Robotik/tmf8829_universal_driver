/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Public API for the TMF8829 universal driver.
 *
 * This is the umbrella header consumers should include. It pulls in the
 * register / type / ops headers and declares the driver instance struct and
 * the public API.
 *
 * Design notes
 * ------------
 * - One @ref tmf8829_driver_t instance per sensor.
 * - Bus selection is a runtime field, not a compile flag.
 * - All host interaction goes through @ref tmf8829_ops_t callbacks.
 * - The driver allocates nothing; the caller owns scratch memory.
 * - Result and histogram frames are pulled (not pushed): call
 *   @ref tmf8829_handle_irq, then @ref tmf8829_read_results /
 *   @ref tmf8829_read_histogram.
 */

#ifndef TMF8829_H
#define TMF8829_H

#include "tmf8829_ops.h"
#include "tmf8829_regs.h"
#include "tmf8829_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Version                                                            */
/* ------------------------------------------------------------------ */

#define TMF8829_DRIVER_VERSION_MAJOR    0
#define TMF8829_DRIVER_VERSION_MINOR    1
#define TMF8829_DRIVER_VERSION_PATCH    0

/* ------------------------------------------------------------------ */
/* Per-instance application callbacks (set on tmf8829_driver_t)       */
/* ------------------------------------------------------------------ */

/** Optional per-instance log sink. Pass NULL to silence. */
typedef void (*tmf8829_log_fn)(tmf8829_driver_t *drv,
                               int level, const char *msg);

/** Called when a result frame has been pulled from the device. */
typedef void (*tmf8829_on_result_fn)(tmf8829_driver_t *drv,
                                     const uint8_t *data, uint16_t len);

/** Called when a histogram frame has been pulled from the device. */
typedef void (*tmf8829_on_histogram_fn)(tmf8829_driver_t *drv,
                                        const uint8_t *data, uint16_t len);

/** Called when the driver hits an unrecoverable runtime error. */
typedef void (*tmf8829_on_error_fn)(tmf8829_driver_t *drv, int err);

/**
 * @brief Optional firmware-image streaming callback.
 *
 * The driver invokes this during @ref tmf8829_download_firmware to fetch
 * the next chunk of the application image. Implementations may serve the
 * image from a built-in const blob, external flash, the filesystem, etc.
 *
 * @param drv               Calling driver.
 * @param offset            Byte offset into the image being requested.
 * @param buf               Destination buffer for at most @p len bytes.
 * @param len               Number of bytes the driver wants.
 * @param image_total_size  On the first call (offset == 0) the implementation
 *                          must write the total image size, in bytes, here.
 *                          On subsequent calls this argument may be NULL.
 *
 * @return Non-negative number of bytes actually copied, or a negative
 *         tmf8829_err_t value on error.
 */
typedef int (*tmf8829_fw_image_read_fn)(tmf8829_driver_t *drv,
                                        uint32_t offset,
                                        uint8_t *buf, uint32_t len,
                                        uint32_t *image_total_size);

/* ------------------------------------------------------------------ */
/* Driver instance                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief One TMF8829 driver instance.
 *
 * The struct is **public POD**: callers populate the configuration fields
 * directly before calling @ref tmf8829_init. Fields prefixed with an
 * underscore are managed by the driver and must not be touched by the host.
 *
 * @note All fields not marked otherwise default to zero-initialised
 *       (i.e. it is safe to value-initialise the struct with @c {}).
 */
struct tmf8829_driver
{
    /* ----- caller-supplied configuration ----- */

    /** Bus this instance speaks (I2C or SPI). */
    tmf8829_bus_t                 bus;

    /** I2C 7-bit slave address; unused when @ref bus is SPI. */
    uint8_t                       i2c_addr;

    /** Opaque host context handed back to every callback. */
    void                         *user_ctx;

    /** Platform callback table. Required, must remain valid for the lifetime
     *  of the driver instance. */
    const tmf8829_ops_t          *ops;

    /** Caller-provided scratch buffer used for transfers and FW download. */
    uint8_t                      *scratch;

    /** Length of @ref scratch in bytes; must be >= @ref TMF8829_MIN_SCRATCH_SIZE. */
    uint16_t                      scratch_len;

    /** Bit-mask of @ref tmf8829_log_level values. */
    uint8_t                       log_level;

    /** Optional per-instance log sink. */
    tmf8829_log_fn                log;

    /** Result frame callback. */
    tmf8829_on_result_fn          on_result;

    /** Histogram frame callback. */
    tmf8829_on_histogram_fn       on_histogram;

    /** Error callback. */
    tmf8829_on_error_fn           on_error;

    /** Firmware-image stream provider for @ref tmf8829_download_firmware. */
    tmf8829_fw_image_read_fn      fw_image_read;

    /* ----- driver-private state (do not touch) ----- */

    uint16_t                      _clk_corr_ratio_uq;
    uint8_t                       _clk_corr_enable;
    uint8_t                       _clk_corr_idx;
    uint32_t                      _host_ticks   [TMF8829_CLK_CORRECTION_PAIRS];
    uint32_t                      _device_ticks [TMF8829_CLK_CORRECTION_PAIRS];
    uint32_t                      _initialised;
};

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise a driver instance.
 *
 * Validates the supplied @p ops table and per-instance configuration on
 * @p drv (scratch pointer / length, bus selection, I2C address). On success,
 * @p drv is bound to @p ops and ready for further calls.
 *
 * @param drv  Caller-allocated driver instance with configuration fields
 *             populated.
 * @param ops  Platform callback table. Must remain valid until the driver
 *             instance is no longer used.
 *
 * @return @ref TMF8829_OK on success, negative @ref tmf8829_err_t otherwise.
 *
 * @note The host is responsible for having configured the enable / IRQ pins
 *       (direction, pulls, alternate functions) and the bus peripheral
 *       *before* calling this function. The driver does not configure pins
 *       or buses; it only operates them via the supplied callbacks.
 */
int tmf8829_init(tmf8829_driver_t *drv, const tmf8829_ops_t *ops);

/**
 * @brief Update the active log mask for an initialised driver instance.
 *
 * @return @ref TMF8829_OK on success.
 */
int tmf8829_set_log_level(tmf8829_driver_t *drv, uint8_t level);

/**
 * @brief Drive the enable pin high and wait for the device CPU to come up.
 *
 * @return @ref TMF8829_OK on success, @ref TMF8829_E_TIMEOUT if the device
 *         did not assert CPU-ready within the expected window.
 *
 * @note Stubbed in the current scaffold; returns @ref TMF8829_E_NOT_IMPLEMENTED.
 */
int tmf8829_enable(tmf8829_driver_t *drv);

/**
 * @brief Drive the enable pin low.
 *
 * @return @ref TMF8829_OK on success.
 *
 * @note Stubbed in the current scaffold; returns @ref TMF8829_E_NOT_IMPLEMENTED.
 */
int tmf8829_disable(tmf8829_driver_t *drv);

/**
 * @brief Read and clear the device interrupt-status register.
 *
 * @param[out] mask_out  Bitwise-OR of @c TMF8829_INT_* flags indicating which
 *                       events were pending. Pass NULL if not needed.
 *
 * @return @ref TMF8829_OK on success, negative on error.
 *
 * @note Stubbed in the current scaffold; returns @ref TMF8829_E_NOT_IMPLEMENTED.
 */
int tmf8829_handle_irq(tmf8829_driver_t *drv, uint8_t *mask_out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_H */

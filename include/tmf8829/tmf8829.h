/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Public API for the TMF8829 universal driver.
 *
 * Function names follow @c tmf8829_snake_case, mirroring the ams-OSRAM
 * reference driver (CamelCase) in a predictable way
 * (@c tmf8829Enable → @c tmf8829_enable).
 *
 * All host I/O goes through @ref tmf8829_ops_t. Result and histogram frames
 * use a **pull** model: clear interrupts, then @ref tmf8829_read_results or
 * @ref tmf8829_read_histogram.
 */

#ifndef TMF8829_H
#define TMF8829_H

#include "tmf8829_ops.h"
#include "tmf8829_regs.h"
#include "tmf8829_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TMF8829_DRIVER_VERSION_MAJOR    0
#define TMF8829_DRIVER_VERSION_MINOR    2
#define TMF8829_DRIVER_VERSION_PATCH    0

#ifndef TMF8829_ENABLE_CAP_DISCHARGE_US
#  define TMF8829_ENABLE_CAP_DISCHARGE_US   3000u
#endif
#ifndef TMF8829_ENABLE_RAMP_US
#  define TMF8829_ENABLE_RAMP_US            3000u
#endif
#ifndef TMF8829_CPU_READY_TIMEOUT_MS
#  define TMF8829_CPU_READY_TIMEOUT_MS      3u
#endif

/**
 * @brief Millisecond budget passed to @ref tmf8829_is_cpu_ready from
 *        @ref tmf8829_wakeup (vendor @c ENABLE_TIME_MS).
 */
#ifndef TMF8829_WAKEUP_CPU_READY_TIMEOUT_MS
#  define TMF8829_WAKEUP_CPU_READY_TIMEOUT_MS  3u
#endif

typedef void (*tmf8829_log_fn)(tmf8829_driver_t *drv,
                               int level, const char *msg);

/** Called once with the pre-header + frame header bytes (21 bytes). */
typedef void (*tmf8829_on_stream_header_fn)(tmf8829_driver_t *drv,
                                            const uint8_t *data, uint16_t len);

typedef void (*tmf8829_on_result_fn)(tmf8829_driver_t *drv,
                                     const uint8_t *data, uint16_t len);

typedef void (*tmf8829_on_histogram_fn)(tmf8829_driver_t *drv,
                                        const uint8_t *data, uint16_t len);

typedef void (*tmf8829_on_error_fn)(tmf8829_driver_t *drv, int err);

typedef int (*tmf8829_fw_image_read_fn)(tmf8829_driver_t *drv,
                                        uint32_t offset,
                                        uint8_t *buf, uint32_t len,
                                        uint32_t *image_total_size);

struct tmf8829_driver
{
    tmf8829_bus_t                 bus;
    uint8_t                       i2c_addr;
    void                         *user_ctx;
    const tmf8829_ops_t          *ops;
    uint8_t                      *scratch;
    uint16_t                      scratch_len;
    uint8_t                       log_level;
    tmf8829_log_fn                log;
    tmf8829_on_stream_header_fn  on_stream_header;
    tmf8829_on_result_fn          on_result;
    tmf8829_on_histogram_fn       on_histogram;
    tmf8829_on_error_fn           on_error;
    tmf8829_fw_image_read_fn      fw_image_read;

    /**
     * Host ticks in one millisecond for clock correction (default 0 = 1000).
     * Set before @ref tmf8829_init if your @ref tmf8829_ops_t::systick_us
     * tick is not 1&nbsp;µs granularity.
     */
    uint32_t                      host_ticks_per_1000_us;

    /** Cached configuration page (190 bytes). */
    uint8_t                       config[TMF8829_CFG_PAGE_SIZE];

    uint32_t                      device_serial_number;
    uint8_t                       app_version[4];
    uint8_t                       chip_version[2];

    uint16_t                      _clk_corr_ratio_uq;
    uint8_t                       _clk_corr_enable;
    uint8_t                       _clk_corr_idx;
    uint8_t                       _cyclic_running;
    uint32_t                      _host_ticks   [TMF8829_CLK_CORRECTION_PAIRS];
    uint32_t                      _device_ticks [TMF8829_CLK_CORRECTION_PAIRS];
    uint32_t                      _initialised;
};

int tmf8829_init(tmf8829_driver_t *drv, const tmf8829_ops_t *ops);
int tmf8829_set_log_level(tmf8829_driver_t *drv, uint8_t level);

int tmf8829_enable(tmf8829_driver_t *drv);
int tmf8829_disable(tmf8829_driver_t *drv);
int tmf8829_is_cpu_ready(tmf8829_driver_t *drv, uint8_t timeout_ms);

int tmf8829_get_and_clr_interrupts(tmf8829_driver_t *drv,
                                   uint8_t mask, uint8_t *out_pending);
int tmf8829_clr_and_enable_interrupts(tmf8829_driver_t *drv, uint8_t mask);
int tmf8829_disable_interrupts(tmf8829_driver_t *drv, uint8_t mask);

int tmf8829_soft_reset(tmf8829_driver_t *drv);
int tmf8829_standby(tmf8829_driver_t *drv);
int tmf8829_power_up(tmf8829_driver_t *drv);
int tmf8829_wakeup(tmf8829_driver_t *drv);
/** @return 1 if CPU ready, 0 if not, or negative @ref tmf8829_err_t. */
int tmf8829_is_device_wakeup(tmf8829_driver_t *drv);

int tmf8829_read_device_info(tmf8829_driver_t *drv);

int tmf8829_command(tmf8829_driver_t *drv, uint8_t cmd);
int tmf8829_cmd_load_config_page(tmf8829_driver_t *drv);
int tmf8829_cmd_write_page(tmf8829_driver_t *drv);
int tmf8829_get_configuration(tmf8829_driver_t *drv);
int tmf8829_set_configuration(tmf8829_driver_t *drv);

int tmf8829_start_measurement(tmf8829_driver_t *drv);
int tmf8829_stop_measurement(tmf8829_driver_t *drv);

int tmf8829_bootloader_spi_off(tmf8829_driver_t *drv);
int tmf8829_bootloader_i2c_off(tmf8829_driver_t *drv);

/**
 * @brief Download application firmware via @ref tmf8829_driver_t::fw_image_read.
 *
 * @param image_start_addr  Typically @ref TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT.
 * @param use_fifo          Non-zero to use bootloader FIFO path (needs larger
 *                          scratch, ≥ image chunk size).
 */
int tmf8829_download_firmware(tmf8829_driver_t *drv, uint32_t image_start_addr,
                              int use_fifo);

int tmf8829_read_results(tmf8829_driver_t *drv);
int tmf8829_read_histogram(tmf8829_driver_t *drv);

uint16_t tmf8829_get_uint16(const uint8_t *data);
uint32_t tmf8829_get_uint32(const uint8_t *data);
void     tmf8829_set_uint16(uint16_t value, uint8_t *data);

uint8_t  tmf8829_get_pixel_size(uint8_t layout);
uint16_t tmf8829_correct_distance(const tmf8829_driver_t *drv, uint16_t distance);
void     tmf8829_correct_distance_data_segment(tmf8829_driver_t *drv,
                                                 uint16_t size, uint8_t layout);

int      tmf8829_clk_correction_set(tmf8829_driver_t *drv, uint8_t enable);
uint16_t tmf8829_clk_correction_ratio_uq15(const tmf8829_driver_t *drv);

#ifdef __cplusplus
}
#endif

#endif /* TMF8829_H */

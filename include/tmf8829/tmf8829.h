/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 * Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
 */

/**
 * @file tmf8829.h
 * @version 0.1.0
 * @brief Public API for the portable TMF8829 driver.
 *
 * Function names use @c tmf8829_snake_case, mirroring the ams-OSRAM reference
 * (CamelCase) in a predictable way (e.g. @c tmf8829Enable → @c tmf8829_enable).
 *
 * All host I/O goes through @ref tmf8829_ops_t. Measurement data uses a **pull**
 * model: acknowledge interrupts with @ref tmf8829_get_and_clr_interrupts, then
 * @ref tmf8829_read_results or @ref tmf8829_read_histogram.
 */

#ifndef TMF8829_H
#define TMF8829_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tmf8829/tmf8829_version.h"

#include "tmf8829_ops.h"
#include "tmf8829_regs.h"
#include "tmf8829_types.h"

/**
 * @brief Microseconds to hold the enable pin low before power-on (cap discharge).
 * @hideinitializer
 */
#ifndef TMF8829_ENABLE_CAP_DISCHARGE_US
#define TMF8829_ENABLE_CAP_DISCHARGE_US 3000u
#endif
/**
 * @brief Microseconds to wait after driving enable high before polling CPU ready.
 * @hideinitializer
 */
#ifndef TMF8829_ENABLE_RAMP_US
#define TMF8829_ENABLE_RAMP_US 3000u
#endif
/**
 * @brief Default millisecond budget for @ref tmf8829_is_cpu_ready inside @ref tmf8829_enable.
 * @hideinitializer
 */
#ifndef TMF8829_CPU_READY_TIMEOUT_MS
#define TMF8829_CPU_READY_TIMEOUT_MS 3u
#endif

/**
 * @brief Millisecond budget passed to @ref tmf8829_is_cpu_ready from
 *        @ref tmf8829_wakeup (vendor @c ENABLE_TIME_MS).
 * @hideinitializer
 */
#ifndef TMF8829_WAKEUP_CPU_READY_TIMEOUT_MS
#define TMF8829_WAKEUP_CPU_READY_TIMEOUT_MS 3u
#endif

/**
 * @brief Optional host logging callback (not invoked unless the driver calls it).
 *
 * @param[in] drv   Driver instance.
 * @param[in] level Bit mask compatible with @ref tmf8829_log_level.
 * @param[in] msg   NUL-terminated message.
 */
typedef void (*tmf8829_log_fn)(tmf8829_driver_t* drv, int level, const char* msg);

/**
 * @brief Invoked once per FIFO read with the fused pre-header and frame header.
 *
 * @param[in] drv   Driver instance.
 * @param[in] data  Buffer of length @p len (typically @c TMF8829_PRE_HEADER_SIZE
 *                  + @c TMF8829_FRAME_HEADER_SIZE bytes).
 * @param[in] len   Number of bytes in @p data.
 */
typedef void (*tmf8829_on_stream_header_fn)(tmf8829_driver_t* drv, const uint8_t* data, uint16_t len);

/**
 * @brief Invoked for each result payload chunk read from the FIFO.
 *
 * @param[in] drv   Driver instance.
 * @param[in] data  Chunk stored in @ref tmf8829_driver_t::buffer.
 * @param[in] len   Chunk length in bytes.
 */
typedef void (*tmf8829_on_result_fn)(tmf8829_driver_t* drv, const uint8_t* data, uint16_t len);

/**
 * @brief Invoked for each histogram payload chunk read from the FIFO.
 *
 * @param[in] drv   Driver instance.
 * @param[in] data  Chunk stored in @ref tmf8829_driver_t::buffer.
 * @param[in] len   Chunk length in bytes.
 */
typedef void (*tmf8829_on_histogram_fn)(tmf8829_driver_t* drv, const uint8_t* data, uint16_t len);

/**
 * @brief Optional error reporter (driver may call when a recoverable issue occurs).
 *
 * @param[in] drv  Driver instance.
 * @param[in] err  Negative @ref tmf8829_err_t or other host-defined code.
 */
typedef void (*tmf8829_on_error_fn)(tmf8829_driver_t* drv, int err);

/**
 * @brief Read-only access to a firmware image for @ref tmf8829_download_firmware.
 *
 * **Size probe:** if @p buf is @c NULL and @p len is @c 0, write the total image
 * size in bytes into @p *image_total_size (if non-@c NULL) and return @c 0.
 * Negative return indicates failure.
 *
 * **Data read:** copy up to @p len bytes from @p offset into @p buf. Return the
 * number of bytes copied on success, or negative on failure. Short reads (return
 * value less than @p len before end of image) are treated as errors by the
 * downloader.
 *
 * @param[in,out] drv                Driver instance (may be ignored).
 * @param[in]     offset             Byte offset into the image.
 * @param[out]    buf                Destination or @c NULL for probe only.
 * @param[in]     len                Max bytes to copy, or @c 0 for probe only.
 * @param[out]    image_total_size   Set on probe; may be @c NULL on data reads.
 *
 * @return Non-negative byte count on success, negative on error.
 */
typedef int (*tmf8829_fw_image_read_fn)(tmf8829_driver_t* drv, uint32_t offset, uint8_t* buf, uint32_t len, uint32_t* image_total_size);

/**
 * @brief One driver instance per physical TMF8829.
 *
 * Host supplies @ref buffer, @ref ops, and optional callbacks. Fields whose
 * names start with @c _ are reserved for internal use but are visible so the
 * struct can be zero-initialised as static storage.
 */
struct tmf8829_driver {
  /** Bus protocol for this instance (@ref TMF8829_BUS_I2C or @ref TMF8829_BUS_SPI). */
  tmf8829_bus_t bus;
  /** I2C 7-bit address when @ref bus is @ref TMF8829_BUS_I2C; ignored for SPI. */
  uint8_t i2c_addr;
  /** Opaque handle passed to @ref tmf8829_ops_t callbacks (e.g. HAL handle). */
  void* user_ctx;
  /** Platform callbacks; set by @ref tmf8829_init from its argument. */
  const tmf8829_ops_t* ops;
  /** Caller-owned buffer for register IO and download staging. */
  uint8_t* buffer;
  /** Size of @ref buffer in bytes; must be at least @ref TMF8829_MIN_BUFFER_SIZE. */
  uint16_t buffer_len;
  /**
   * Optional per-transfer bus write capability in bytes.
   *
   * Limits the payload length passed to @ref tmf8829_ops_t::write in a single call.
   * Set to @c 0 to disable this extra cap (driver then only uses protocol limits).
   */
  uint16_t max_write_len;
  /** Bit mask: @ref tmf8829_log_level; used if @ref log is non-@c NULL. */
  uint8_t log_level;
  /** Optional log sink; may be @c NULL. */
  tmf8829_log_fn log;
  /** Optional: called with stream header bytes; may be @c NULL. */
  tmf8829_on_stream_header_fn on_stream_header;
  /** Optional: called per result chunk; may be @c NULL. */
  tmf8829_on_result_fn on_result;
  /** Optional: called per histogram chunk; may be @c NULL. */
  tmf8829_on_histogram_fn on_histogram;
  /** Optional error callback; may be @c NULL. */
  tmf8829_on_error_fn on_error;
  /** Optional image reader for @ref tmf8829_download_firmware; may be @c NULL. */
  tmf8829_fw_image_read_fn fw_image_read;

  /**
   * Host timer ticks that correspond to one millisecond (for clock correction).
   * If @c 0 after @ref tmf8829_init, the driver assumes @c 1000 ticks per ms
   * (i.e. @ref tmf8829_ops_t::systick_us counts microseconds). Set before
   * @ref tmf8829_init if your tick is coarser or finer.
   */
  uint32_t host_ticks_per_1000_us;

  /**
   * Cached configuration page (@ref TMF8829_CFG_PAGE_SIZE bytes).
   * Filled by @ref tmf8829_get_configuration; consumed by @ref tmf8829_set_configuration.
   */
  uint8_t config[TMF8829_CFG_PAGE_SIZE];

  /** Device serial number (little-endian), updated by @ref tmf8829_read_device_info. */
  uint32_t device_serial_number;
  /** Application version triple + build, from registers; see @ref tmf8829_read_device_info. */
  uint8_t app_version[4];
  /** Chip id / revision bytes from @ref TMF8829_REG_CID_RID. */
  uint8_t chip_version[2];

  /* --- Internal state (do not access directly; layout may change) --- */
  uint16_t _clk_corr_ratio_uq;                          /**< Internal: UQ15 clock ratio vs nominal. */
  uint8_t _clk_corr_enable;                             /**< Internal: non-zero if pairs are collected. */
  uint8_t _clk_corr_idx;                                /**< Internal: ring index for correction pairs. */
  uint8_t _cyclic_running;                              /**< Internal: measurement active flag. */
  uint32_t _host_ticks[TMF8829_CLK_CORRECTION_PAIRS];   /**< Internal */
  uint32_t _device_ticks[TMF8829_CLK_CORRECTION_PAIRS]; /**< Internal */
  uint32_t _initialised;                                /**< Internal: magic after @ref tmf8829_init. */
};

/**
 * @brief Validate parameters, bind @p ops, and reset driver state.
 *
 * @param[in,out] drv  Driver instance; @ref tmf8829_driver_t::buffer and lengths must be valid.
 * @param[in]     ops  Platform operations (must remain valid for the lifetime of @p drv).
 *
 * @return @ref TMF8829_OK on success, or negative @ref tmf8829_err_t.
 */
int tmf8829_init(tmf8829_driver_t* drv, const tmf8829_ops_t* ops);

/**
 * @brief Update @ref tmf8829_driver_t::log_level for optional @ref tmf8829_driver_t::log use.
 *
 * @return @ref TMF8829_OK, or @ref TMF8829_E_PARAM if uninitialised.
 */
int tmf8829_set_log_level(tmf8829_driver_t* drv, uint8_t level);

/**
 * @brief Drive the enable pin high and wait for CPU ready.
 *
 * If @ref tmf8829_ops_t::write_pin_enable is @c NULL (enable pin permanently
 * tied high), the pin toggle and discharge delays are skipped and the function
 * only polls for CPU readiness.
 *
 * To do a power cycle, call @ref tmf8829_disable followed by @ref tmf8829_enable
 * followed by choosing a communication interface.
 *
 * @return @ref TMF8829_OK, @ref TMF8829_E_TIMEOUT, @ref TMF8829_E_BUS, or @ref TMF8829_E_PARAM.
 */
int tmf8829_enable(tmf8829_driver_t* drv);

/**
 * @brief Drive the enable pin low (sensor off).
 *
 * If @ref tmf8829_ops_t::write_pin_enable is @c NULL (enable pin permanently
 * tied high), returns @ref TMF8829_E_NOT_IMPLEMENTED since the sensor cannot
 * be powered down via software.
 */
int tmf8829_disable(tmf8829_driver_t* drv);

/**
 * @brief Poll @ref TMF8829_REG_ENABLE until @ref TMF8829_ENABLE_CPU_READY_MASK is set or time elapses.
 *
 * @param[in,out] drv        Initialised driver instance.
 * @param[in] timeout_ms  Maximum time to poll (milliseconds). Uses @c timeout_ms+1 loop iterations with 1&nbsp;ms delay.
 *
 * @return @ref TMF8829_OK when ready, @ref TMF8829_E_TIMEOUT, @ref TMF8829_E_BUS, or @ref TMF8829_E_PARAM.
 */
int tmf8829_is_cpu_ready(tmf8829_driver_t* drv, uint8_t timeout_ms);

/**
 * @brief Read @ref TMF8829_REG_INT_STATUS and write-1-to-clear bits in @p mask.
 *
 * @param[in,out] drv        Initialised driver instance.
 * @param[in]  mask         Bits to consider (typically @ref TMF8829_INT_ALL).
 * @param[out] out_pending  Cleared interrupt bits that were set before the clear (subset of @p mask).
 *
 * @return @ref TMF8829_OK, @ref TMF8829_E_BUS, or @ref TMF8829_E_PARAM.
 */
int tmf8829_get_and_clr_interrupts(tmf8829_driver_t* drv, uint8_t mask, uint8_t* out_pending);

/**
 * @brief Read-modify-write @ref TMF8829_REG_INT_ENAB to enable @p mask bits.
 */
int tmf8829_clr_and_enable_interrupts(tmf8829_driver_t* drv, uint8_t mask);

/**
 * @brief Read-modify-write @ref TMF8829_REG_INT_ENAB to disable @p mask bits.
 */
int tmf8829_disable_interrupts(tmf8829_driver_t* drv, uint8_t mask);

/**
 * @brief Write @ref TMF8829_RESET_SOFT_MASK to @ref TMF8829_REG_RESET and reset clock-correction state.
 */
int tmf8829_soft_reset(tmf8829_driver_t* drv);

/**
 * @brief Write @ref TMF8829_RESET_HARD_MASK to @ref TMF8829_REG_RESET and wait until CPU is ready.
 */
int tmf8829_hard_reset(tmf8829_driver_t* drv);

/**
 * @brief Request standby if the CPU is running (sets @ref TMF8829_ENABLE_POFF_MASK).
 */
int tmf8829_standby(tmf8829_driver_t* drv);

/**
 * @brief Assert power-on (@ref TMF8829_ENABLE_PON_MASK) and wait @ref TMF8829_ENABLE_RAMP_US.
 */
int tmf8829_power_up(tmf8829_driver_t* drv);

/**
 * @brief Leave low-power: if CPU not ready, set RAM boot path + PON and poll readiness.
 */
int tmf8829_wakeup(tmf8829_driver_t* drv);

/**
 * @brief Non-blocking wakeup check: reads enable register and CPU-ready bit.
 *
 * @param[in,out] drv       Initialised driver instance.
 * @param[out] out_awake  Set to @c 1 if CPU ready, @c 0 if not.
 *
 * @return @ref TMF8829_OK, @ref TMF8829_E_BUS, or @ref TMF8829_E_PARAM.
 */
int tmf8829_is_device_wakeup(tmf8829_driver_t* drv, uint8_t* out_awake);

/**
 * @brief Read serial number, app version, and chip id registers into @p drv.
 *
 * Returns @ref TMF8829_E_STATE if @ref TMF8829_REG_APP_ID reports bootloader
 * mode (@ref TMF8829_APP_ID__BOOTLOADER, @c 0x80) instead of application mode
 * (@ref TMF8829_APP_ID__APPLICATION, @c 0x01). It only succeeds if the device
 * is in application mode (i.e. has the firmware loaded).
 *
 * This behavior is kept for parity with the original ams-OSRAM reference
 * drivers (Linux and Arduino), which also treat bootloader state as an error
 * in the corresponding device-info routine.
 *
 * @return @ref TMF8829_OK or a negative error code.
 */
int tmf8829_read_device_info(tmf8829_driver_t* drv);

/**
 * @brief Issue a raw application command byte and poll @ref TMF8829_REG_CMD_STAT for acceptance.
 *
 * @param[in,out] drv        Initialised driver instance.
 * @param[in] cmd         Opcode (e.g. @ref TMF8829_CMD_MEASURE).
 * @param[in] timeout_ms  Maximum time to poll for command acceptance.
 *
 * @pre Driver initialised and device in application mode.
 */
int tmf8829_command(tmf8829_driver_t* drv, uint8_t cmd, uint16_t timeout_ms);

/** @brief @ref TMF8829_CMD_LOAD_CONFIG_PAGE */
int tmf8829_cmd_load_config_page(tmf8829_driver_t* drv);
/** @brief @ref TMF8829_CMD_WRITE_PAGE using @ref tmf8829_driver_t::config */
int tmf8829_cmd_write_page(tmf8829_driver_t* drv);

/**
 * @brief After @ref tmf8829_cmd_load_config_page, read the configuration page into @ref tmf8829_driver_t::config.
 */
int tmf8829_get_configuration(tmf8829_driver_t* drv);

/**
 * @brief Write @ref tmf8829_driver_t::config to the device (load page + write page sequence).
 *
 * @pre The config page must have been loaded first via @ref tmf8829_get_configuration or
 *      manually populated from a known-good state. The device must be in application mode
 *      with no measurement active.
 */
int tmf8829_set_configuration(tmf8829_driver_t* drv);

/** @brief @ref TMF8829_CMD_MEASURE
 *  @pre Configuration written, device in application mode, no measurement active.
 */
int tmf8829_start_measurement(tmf8829_driver_t* drv);
/** @brief @ref TMF8829_CMD_STOP
 *  @pre Measurement active; wakes device if in standby.
 */
int tmf8829_stop_measurement(tmf8829_driver_t* drv);

/** @brief Bootloader: @ref TMF8829_BL_CMD_STAT_SPI_OFF */
int tmf8829_bootloader_spi_off(tmf8829_driver_t* drv);
/** @brief Bootloader: @ref TMF8829_BL_CMD_STAT_I2C_OFF */
int tmf8829_bootloader_i2c_off(tmf8829_driver_t* drv);

/**
 * @brief Stream firmware through @ref tmf8829_driver_t::fw_image_read and start the RAM application.
 *
 * @pre Bootloader running, @ref tmf8829_driver_t::fw_image_read set, buffer large enough
 * (non-FIFO: @ref TMF8829_BL_WR_HEADER + @ref TMF8829_BL_MAX_PAYLOAD; FIFO: full image chunks).
 *
 * @param[in,out] drv              Initialised driver instance.
 * @param[in] image_start_addr  Load address; use @ref TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT for vendor image.
 * @param[in] use_fifo          Non-zero for FIFO download path; @c 0 for direct WR_RAM.
 *
 * @return @ref TMF8829_OK on success, negative @ref tmf8829_err_t or callback error code.
 */
int tmf8829_download_firmware(tmf8829_driver_t* drv, uint32_t image_start_addr, int use_fifo);

/**
 * @brief Read a result frame from the FIFO (header via @ref TMF8829_REG_FIFO_STATUS, payload via @ref TMF8829_REG_FIFO).
 *
 * Optionally applies clock distance correction to payload segments when enabled.
 * Invokes @ref tmf8829_driver_t::on_stream_header / @ref tmf8829_driver_t::on_result if set.
 *
 * @return @ref TMF8829_OK if EOF marker matches @ref TMF8829_FRAME_EOF; @ref TMF8829_E_NO_RESULT if wrong frame type
 *         or bad footer; @ref TMF8829_E_PARAM / @ref TMF8829_E_BUS as appropriate.
 */
int tmf8829_read_results(tmf8829_driver_t* drv);

/**
 * @brief Read a histogram frame from the FIFO (same header path as results).
 *
 * @return @ref TMF8829_OK on valid histogram EOF; @ref TMF8829_E_NO_RESULT if not a histogram frame.
 */
int tmf8829_read_histogram(tmf8829_driver_t* drv);

/** @brief Little-endian uint16 from two bytes. */
uint16_t tmf8829_get_uint16(const uint8_t* data);
/** @brief Little-endian uint32 from four bytes. */
uint32_t tmf8829_get_uint32(const uint8_t* data);
/** @brief Write @p value to two bytes, little-endian. */
void tmf8829_set_uint16(uint16_t value, uint8_t* data);

/**
 * @brief Bytes per result pixel for a given @ref TMF8829_REG_CFG_RESULT_FORMAT layout byte.
 *
 * Uses @c TMF8829_RESULT_FORMAT_* masks in @p layout.
 */
uint8_t tmf8829_get_pixel_size(uint8_t layout);

/**
 * @brief Scale a raw distance using @ref tmf8829_driver_t::_clk_corr_ratio_uq (UQ15).
 *
 * @return Saturated 16-bit corrected distance.
 */
uint16_t tmf8829_correct_distance(const tmf8829_driver_t* drv, uint16_t distance);

/**
 * @brief Apply @ref tmf8829_correct_distance to each distance field in @ref tmf8829_driver_t::buffer.
 *
 * @param[in,out] drv     Initialised driver instance.
 * @param[in] size    Number of bytes in buffer to treat as pixels of size @ref tmf8829_get_pixel_size(@p layout).
 * @param[in] layout  Result-format byte (peak count and flags).
 */
void tmf8829_correct_distance_data_segment(tmf8829_driver_t* drv, uint16_t size, uint8_t layout);

/**
 * @brief Enable or disable collection of clock-correction samples in result reads.
 *
 * When enabling, internal ratio state is reset.
 */
int tmf8829_clk_correction_set(tmf8829_driver_t* drv, uint8_t enable);

/**
 * @brief Current UQ15 ratio used by @ref tmf8829_correct_distance; nominal unity is 32768.
 *
 * @return Unity ratio if @p drv is @c NULL.
 */
uint16_t tmf8829_clk_correction_ratio_uq15(const tmf8829_driver_t* drv);

/**
 * @brief Change the device's I2C slave address and update the driver instance.
 *
 * Writes @p new_addr to @ref TMF8829_REG_I2C_DEVADDR, then updates
 * @ref tmf8829_driver_t::i2c_addr so subsequent transfers use the new address.
 *
 * @pre Device in application mode; bus type must be @ref TMF8829_BUS_I2C.
 *
 * @param[in,out] drv       Initialised driver instance.
 * @param[in] new_addr  New 7-bit I2C address (0x00–0x7F).
 *
 * @return @ref TMF8829_OK, @ref TMF8829_E_PARAM, or @ref TMF8829_E_BUS.
 */
int tmf8829_set_i2c_address(tmf8829_driver_t* drv, uint8_t new_addr);

#ifdef __cplusplus
}
#endif

#endif /* TMF8829_H */

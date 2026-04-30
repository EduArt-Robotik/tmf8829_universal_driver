/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 */

#include "tmf8829/tmf8829.h"
#include "tmf8829/tmf8829_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TMF8829_SATURATE16(x) ((uint32_t)(x) > 0xFFFFu ? 0xFFFFu : (uint16_t)(x))

#define CLK_IDX_MOD(i) \
    ((uint8_t)((i) & (uint8_t)((TMF8829_CLK_CORRECTION_PAIRS)-1u)))

#define MAX_U32_SCALE 0x80000000u

/* -------------------------------------------------------------------------- */
/* Low-level bus                                                              */
/* -------------------------------------------------------------------------- */

static uint32_t drv_host_ticks_per_ms(const tmf8829_driver_t *drv)
{
    if (drv->host_ticks_per_1000_us == 0u) {
        return 1000u;
    }
    return drv->host_ticks_per_1000_us;
}

static int bus_read(tmf8829_driver_t *drv, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (drv->ops->read(drv, reg, buf, len) != 0) {
        return TMF8829_E_BUS;
    }
    return TMF8829_OK;
}

static int bus_write(tmf8829_driver_t *drv, uint8_t reg, const uint8_t *buf, uint16_t len)
{
    if (drv->ops->write(drv, reg, buf, len) != 0) {
        return TMF8829_E_BUS;
    }
    return TMF8829_OK;
}

/**
 * Poll until @p reg's first byte equals @p expected or timeout.
 * Matches vendor tmf8829CheckRegister semantics.
 */
static int reg_poll_first(tmf8829_driver_t *drv, uint8_t reg,
                          uint8_t expected, uint8_t len, uint16_t timeout_ms)
{
    if (len == 0u || len > drv->buffer_len) {
        return TMF8829_E_PARAM;
    }
    unsigned int remaining = (unsigned int)timeout_ms + 1u;
    do {
        int rc = bus_read(drv, reg, drv->buffer, len);
        if (rc != TMF8829_OK) {
            return rc;
        }
        if (drv->buffer[0] == expected) {
            return TMF8829_OK;
        }
        if (remaining > 1u) {
            drv->ops->delay_us(1000u);
        }
    } while (--remaining > 0u);
    return TMF8829_E_TIMEOUT;
}

/* -------------------------------------------------------------------------- */
/* Clock correction                                                           */
/* -------------------------------------------------------------------------- */

static void clk_reset(tmf8829_driver_t *drv)
{
    uint8_t i;
    drv->_clk_corr_idx = 0u;
    drv->_clk_corr_ratio_uq = (uint16_t)(1u << 15);
    for (i = 0u; i < (uint8_t)TMF8829_CLK_CORRECTION_PAIRS; i++) {
        drv->_host_ticks[i]   = 0u;
        drv->_device_ticks[i] = 0u;
    }
}

static uint16_t clk_calc_ratio_uq(uint32_t h_diff, uint32_t t_diff,
                                  uint16_t prev, uint32_t host_per_ms)
{
    uint32_t ratio_uq32 = prev;
    const uint32_t ht = host_per_ms;
    if (h_diff >= ht && t_diff >= (uint32_t)TMF8829_TICKS_PER_1000_US) {
        while (h_diff < MAX_U32_SCALE && t_diff < MAX_U32_SCALE) {
            h_diff <<= 1u;
            t_diff <<= 1u;
        }
        t_diff /= (uint32_t)TMF8829_TICKS_PER_1000_US;
        h_diff /= ht;
        while (h_diff < MAX_U32_SCALE && t_diff < MAX_U32_SCALE) {
            h_diff <<= 1u;
            t_diff <<= 1u;
        }
        t_diff = (t_diff + (1u << 14)) >> 15u;
        if (t_diff != 0u) {
            ratio_uq32 = (h_diff + (t_diff >> 1u)) / t_diff;
            if (ratio_uq32 > (uint32_t)(1u << 15) + (1u << 14) ||
                ratio_uq32 < (uint32_t)(1u << 15) - (1u << 14)) {
                ratio_uq32 = prev;
            }
        }
    }
    return (uint16_t)ratio_uq32;
}

static void clk_add_pair(tmf8829_driver_t *drv, uint32_t host_tick,
                         uint32_t device_tick)
{
    uint8_t idx;
    uint8_t idx_old;
    uint32_t h_diff;
    uint32_t t_diff;
    const uint32_t hpm = drv_host_ticks_per_ms(drv);

    drv->_clk_corr_idx = CLK_IDX_MOD(drv->_clk_corr_idx + 1u);
    idx = drv->_clk_corr_idx;
    drv->_host_ticks[idx]   = host_tick;
    drv->_device_ticks[idx] = device_tick;

    idx_old = CLK_IDX_MOD((uint8_t)(idx + TMF8829_CLK_CORRECTION_PAIRS - 1u));

    if (drv->_host_ticks[idx] < drv->_host_ticks[idx_old]) {
        h_diff = 0u;
    } else {
        h_diff = drv->_host_ticks[idx] - drv->_host_ticks[idx_old];
    }
    t_diff = drv->_device_ticks[idx] - drv->_device_ticks[idx_old];
    drv->_clk_corr_ratio_uq = clk_calc_ratio_uq(h_diff, t_diff,
                                                drv->_clk_corr_ratio_uq, hpm);
}

/* -------------------------------------------------------------------------- */
/* Init / power                                                               */
/* -------------------------------------------------------------------------- */

static int validate_ops(const tmf8829_ops_t *ops)
{
    if (ops == NULL) {
        return TMF8829_E_PARAM;
    }
    if (ops->read == NULL || ops->write == NULL || ops->delay_us == NULL ||
        ops->systick_us == NULL || ops->write_pin_enable == NULL) {
        return TMF8829_E_PARAM;
    }
    return TMF8829_OK;
}

static int validate_instance(const tmf8829_driver_t *drv)
{
    if (drv == NULL) {
        return TMF8829_E_PARAM;
    }
    if (drv->buffer == NULL || drv->buffer_len < TMF8829_MIN_BUFFER_SIZE) {
        return TMF8829_E_PARAM;
    }
    if (drv->bus != TMF8829_BUS_I2C && drv->bus != TMF8829_BUS_SPI) {
        return TMF8829_E_PARAM;
    }
    if (drv->bus == TMF8829_BUS_I2C && drv->i2c_addr > 0x7Fu) {
        return TMF8829_E_PARAM;
    }
    return TMF8829_OK;
}

int tmf8829_init(tmf8829_driver_t *drv, const tmf8829_ops_t *ops)
{
    int rc = validate_instance(drv);
    if (rc != TMF8829_OK) {
        return rc;
    }
    rc = validate_ops(ops);
    if (rc != TMF8829_OK) {
        return rc;
    }

    drv->ops = ops;
    clk_reset(drv);
    drv->_clk_corr_enable = 1u;
    memset(drv->config, 0, sizeof(drv->config));
    drv->device_serial_number = 0u;
    memset(drv->app_version, 0, sizeof(drv->app_version));
    memset(drv->chip_version, 0, sizeof(drv->chip_version));
    drv->_cyclic_running = 0u;

    drv->_initialised = TMF8829_MAGIC_INITIALISED;
    return TMF8829_OK;
}

int tmf8829_set_log_level(tmf8829_driver_t *drv, uint8_t level)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->log_level = level;
    return TMF8829_OK;
}

int tmf8829_enable(tmf8829_driver_t *drv)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }

    drv->ops->write_pin_enable(drv, 0);
    drv->ops->delay_us(TMF8829_ENABLE_CAP_DISCHARGE_US);
    drv->ops->write_pin_enable(drv, 1);
    drv->ops->delay_us(TMF8829_ENABLE_RAMP_US);
    return tmf8829_is_cpu_ready(drv, TMF8829_CPU_READY_TIMEOUT_MS);
}

int tmf8829_disable(tmf8829_driver_t *drv)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->ops->write_pin_enable(drv, 0);
    return TMF8829_OK;
}

int tmf8829_is_cpu_ready(tmf8829_driver_t *drv, uint8_t timeout_ms)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }

    unsigned int remaining = (unsigned int)timeout_ms + 1u;
    do {
        uint8_t v = 0u;
        int rc = bus_read(drv, TMF8829_REG_ENABLE, &v, 1);
        if (rc != TMF8829_OK) {
            return rc;
        }
        if ((v & TMF8829_ENABLE_CPU_READY_MASK) != 0u) {
            return TMF8829_OK;
        }
        if (remaining > 1u) {
            drv->ops->delay_us(1000u);
        }
    } while (--remaining > 0u);

    return TMF8829_E_TIMEOUT;
}

int tmf8829_get_and_clr_interrupts(tmf8829_driver_t *drv,
                                   uint8_t mask, uint8_t *out_pending)
{
    uint8_t status;
    uint8_t pending;

    if (!tmf8829_internal_is_initialised(drv) || out_pending == NULL) {
        return TMF8829_E_PARAM;
    }

    *out_pending = 0u;

    if (bus_read(drv, TMF8829_REG_INT_STATUS, &status, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }

    pending = (uint8_t)(status & mask);
    if (pending != 0u) {
        if (bus_write(drv, TMF8829_REG_INT_STATUS, &pending, 1) != TMF8829_OK) {
            return TMF8829_E_BUS;
        }
    }

    *out_pending = pending;
    return TMF8829_OK;
}

int tmf8829_soft_reset(tmf8829_driver_t *drv)
{
    uint8_t v = TMF8829_RESET_SOFT_MASK;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (bus_write(drv, TMF8829_REG_RESET, &v, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    clk_reset(drv);
    return TMF8829_OK;
}

int tmf8829_standby(tmf8829_driver_t *drv)
{
    uint8_t en;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (bus_read(drv, TMF8829_REG_ENABLE, &en, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    if ((en & TMF8829_ENABLE_CPU_READY_MASK) != 0u) {
        en = (uint8_t)(en | TMF8829_ENABLE_POFF_MASK);
        if (bus_write(drv, TMF8829_REG_ENABLE, &en, 1) != TMF8829_OK) {
            return TMF8829_E_BUS;
        }
    }
    return TMF8829_OK;
}

int tmf8829_power_up(tmf8829_driver_t *drv)
{
    uint8_t v = TMF8829_ENABLE_PON_MASK;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (bus_write(drv, TMF8829_REG_ENABLE, &v, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    drv->ops->delay_us(TMF8829_ENABLE_RAMP_US);
    return TMF8829_OK;
}

int tmf8829_wakeup(tmf8829_driver_t *drv)
{
    uint8_t en;
    int rc;

    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (bus_read(drv, TMF8829_REG_ENABLE, &en, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    if ((en & TMF8829_ENABLE_CPU_READY_MASK) == 0u) {
        en = (uint8_t)(en |
                        (uint8_t)(TMF8829_POWERUP_RAM << TMF8829_ENABLE_POWERUP_SHIFT));
        en = (uint8_t)(en | TMF8829_ENABLE_PON_MASK);
        if (bus_write(drv, TMF8829_REG_ENABLE, &en, 1) != TMF8829_OK) {
            return TMF8829_E_BUS;
        }
        rc = tmf8829_is_cpu_ready(drv, (uint8_t)TMF8829_WAKEUP_CPU_READY_TIMEOUT_MS);
        if (rc == TMF8829_OK) {
            return TMF8829_OK;
        }
        return rc;
    }
    return TMF8829_OK;
}

int tmf8829_is_device_wakeup(tmf8829_driver_t *drv)
{
    uint8_t en;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (bus_read(drv, TMF8829_REG_ENABLE, &en, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return (en & TMF8829_ENABLE_CPU_READY_MASK) ? 1 : 0;
}

/* -------------------------------------------------------------------------- */
/* Application / bootloader helpers                                           */
/* -------------------------------------------------------------------------- */

int tmf8829_read_device_info(tmf8829_driver_t *drv)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }

    memset(drv->chip_version, 0, sizeof(drv->chip_version));
    memset(drv->app_version, 0, sizeof(drv->app_version));
    drv->device_serial_number = 0u;

    if (bus_read(drv, TMF8829_REG_ID, drv->chip_version, 2) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    if (bus_read(drv, TMF8829_REG_APP_ID, drv->app_version, 4) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    if (drv->app_version[0] != TMF8829_APP_ID__APPLICATION) {
        return TMF8829_E_STATE;
    }
    if (bus_read(drv, TMF8829_REG_SERIAL_NUMBER_0, drv->buffer, 4) !=
        TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    drv->device_serial_number = tmf8829_get_uint32(drv->buffer);
    return TMF8829_OK;
}

int tmf8829_command(tmf8829_driver_t *drv, uint8_t cmd)
{
    int rc;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->buffer[0] = cmd;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    rc = reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_STAT_OK, 1u,
                        TMF8829_APP_CMD_LOAD_CONFIG_TIMEOUT_MS);
    return rc;
}

int tmf8829_cmd_load_config_page(tmf8829_driver_t *drv)
{
    int rc;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->buffer[0] = TMF8829_CMD_LOAD_CONFIG_PAGE;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    rc = reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_STAT_OK, 1u,
                        TMF8829_APP_CMD_LOAD_CONFIG_TIMEOUT_MS);
    if (rc != TMF8829_OK) {
        return rc;
    }
    return reg_poll_first(drv, TMF8829_REG_CID_RID, TMF8829_CMD_LOAD_CONFIG_PAGE,
                          1u, TMF8829_APP_CMD_LOAD_CONFIG_TIMEOUT_MS);
}

int tmf8829_cmd_write_page(tmf8829_driver_t *drv)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->buffer[0] = TMF8829_CMD_WRITE_PAGE;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_STAT_OK, 1u,
                          TMF8829_APP_CMD_WRITE_CONFIG_TIMEOUT_MS);
}

int tmf8829_get_configuration(tmf8829_driver_t *drv)
{
    int rc = tmf8829_cmd_load_config_page(drv);
    if (rc != TMF8829_OK) {
        return rc;
    }
    if (sizeof(drv->config) > drv->buffer_len) {
        return TMF8829_E_PARAM;
    }
    return bus_read(drv, TMF8829_REG_CFG_PERIOD_MS_LSB, drv->config,
                    (uint16_t)sizeof(drv->config));
}

int tmf8829_set_configuration(tmf8829_driver_t *drv)
{
    int rc;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    rc = bus_write(drv, TMF8829_REG_CFG_PERIOD_MS_LSB, drv->config,
                   (uint16_t)sizeof(drv->config));
    if (rc != TMF8829_OK) {
        return rc;
    }
    return tmf8829_cmd_write_page(drv);
}

int tmf8829_start_measurement(tmf8829_driver_t *drv)
{
    uint16_t period;
    int rc;

    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }

    clk_reset(drv);

    drv->buffer[0] = TMF8829_CMD_MEASURE;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    rc = reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_STAT_ACCEPTED, 1u,
                        TMF8829_APP_CMD_MEASURE_TIMEOUT_MS);
    if (rc != TMF8829_OK) {
        return rc;
    }

    period = (uint16_t)drv->config[0] +
             (uint16_t)((uint16_t)drv->config[TMF8829_REG_CFG_PERIOD_MS_MSB -
                                                TMF8829_REG_CFG_PERIOD_MS_LSB]
                        << 8);
    if (period != 0u) {
        drv->_cyclic_running = 1u;
    }
    return TMF8829_OK;
}

int tmf8829_stop_measurement(tmf8829_driver_t *drv)
{
    int rc;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }

    rc = tmf8829_wakeup(drv);
    if (rc != TMF8829_OK) {
        return rc;
    }

    drv->buffer[0] = TMF8829_CMD_STOP;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    drv->_cyclic_running = 0u;
    return reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_STAT_OK, 1u,
                          TMF8829_APP_CMD_STOP_TIMEOUT_MS);
}

int tmf8829_clr_and_enable_interrupts(tmf8829_driver_t *drv, uint8_t mask)
{
    uint8_t v;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    v = 0xFFu;
    if (bus_write(drv, TMF8829_REG_INT_STATUS, &v, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    if (bus_read(drv, TMF8829_REG_INT_ENAB, &v, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    v = (uint8_t)(v | mask);
    return bus_write(drv, TMF8829_REG_INT_ENAB, &v, 1);
}

int tmf8829_disable_interrupts(tmf8829_driver_t *drv, uint8_t mask)
{
    uint8_t v;
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (bus_read(drv, TMF8829_REG_INT_ENAB, &v, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    v = (uint8_t)(v & (uint8_t)~mask);
    if (bus_write(drv, TMF8829_REG_INT_ENAB, &v, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return bus_write(drv, TMF8829_REG_INT_STATUS, &mask, 1);
}

/* -------------------------------------------------------------------------- */
/* Bootloader                                                                 */
/* ----------------------------:---------------------------------------------- */

int tmf8829_bootloader_spi_off(tmf8829_driver_t *drv)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->buffer[0] = TMF8829_BL_CMD_STAT_SPI_OFF;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_BL_STAT_READY, 1u,
                          TMF8829_BL_CMD_TIMEOUT_MS);
}

int tmf8829_bootloader_i2c_off(tmf8829_driver_t *drv)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->buffer[0] = TMF8829_BL_CMD_STAT_I2C_OFF;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_BL_STAT_READY, 1u,
                          TMF8829_BL_CMD_TIMEOUT_MS);
}

static int bl_set_ram_addr(tmf8829_driver_t *drv, uint32_t addr)
{
    drv->buffer[0] = TMF8829_BL_CMD_STAT_ADDR_RAM;
    drv->buffer[1] = 4u;
    drv->buffer[2] = (uint8_t)(addr & 0xFFu);
    drv->buffer[3] = (uint8_t)((addr >> 8) & 0xFFu);
    drv->buffer[4] = (uint8_t)((addr >> 16) & 0xFFu);
    drv->buffer[5] = (uint8_t)((addr >> 24) & 0xFFu);
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 6u) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_BL_STAT_READY, 3u,
                          TMF8829_BL_SET_ADDR_TIMEOUT_MS);
}

static int bl_write_ram_both(tmf8829_driver_t *drv, uint8_t payload_len,
                             const uint8_t *payload)
{
    uint16_t total = (uint16_t)(TMF8829_BL_WR_HEADER + payload_len);
    if (total > drv->buffer_len) {
        return TMF8829_E_PARAM;
    }
    drv->buffer[0] = TMF8829_BL_CMD_STAT_W_RAM_BOTH;
    drv->buffer[1] = payload_len;
    memcpy(&drv->buffer[TMF8829_BL_WR_HEADER], payload, payload_len);
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, total) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_BL_STAT_READY, 1u,
                          TMF8829_BL_W_RAM_TIMEOUT_MS);
}

static int bl_write_fifo_both(tmf8829_driver_t *drv, uint32_t addr, uint32_t len)
{
    uint16_t wsize = (uint16_t)((len + 3u) / 4u);
    drv->buffer[0] = TMF8829_BL_CMD_STAT_FIFO_BOTH;
    drv->buffer[1] = 6u;
    drv->buffer[2] = (uint8_t)(addr & 0xFFu);
    drv->buffer[3] = (uint8_t)((addr >> 8) & 0xFFu);
    drv->buffer[4] = (uint8_t)((addr >> 16) & 0xFFu);
    drv->buffer[5] = (uint8_t)((addr >> 24) & 0xFFu);
    drv->buffer[6] = (uint8_t)(wsize & 0xFFu);
    drv->buffer[7] = (uint8_t)((wsize >> 8) & 0xFFu);
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 8u) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return reg_poll_first(drv, TMF8829_REG_CMD_STAT, TMF8829_BL_STAT_READY, 1u,
                          TMF8829_BL_W_FIFO_TIMEOUT_MS);
}

static int bl_start_ram_app(tmf8829_driver_t *drv)
{
    drv->buffer[0] = TMF8829_BL_CMD_STAT_START_RAM_APP;
    if (bus_write(drv, TMF8829_REG_CMD_STAT, drv->buffer, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    return reg_poll_first(drv, TMF8829_REG_APP_ID, TMF8829_APP_ID__APPLICATION,
                          1u, TMF8829_BL_START_APP_TIMEOUT_MS);
}

static int download_set_enable_ram(tmf8829_driver_t *drv)
{
    uint8_t en;
    if (bus_read(drv, TMF8829_REG_ENABLE, &en, 1) != TMF8829_OK) {
        return TMF8829_E_BUS;
    }
    en = (uint8_t)(en & (uint8_t)~TMF8829_ENABLE_POWERUP_MASK);
    en = (uint8_t)(en |
                   (uint8_t)(TMF8829_POWERUP_RAM << TMF8829_ENABLE_POWERUP_SHIFT));
    return bus_write(drv, TMF8829_REG_ENABLE, &en, 1);
}

int tmf8829_download_firmware(tmf8829_driver_t *drv, uint32_t image_start_addr,
                              int use_fifo)
{
    uint32_t idx = 0u;
    int rc;
    uint32_t image_size = 0u;
    uint16_t chunk_len;

    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (drv->fw_image_read == NULL) {
        return TMF8829_E_PARAM;
    }

    if (use_fifo) {
        int n = drv->fw_image_read(drv, 0u, NULL, 0u, &image_size);
        if (n < 0) {
            return n;
        }
        if (image_size == 0u) {
            return TMF8829_E_PARAM;
        }
        rc = bl_write_fifo_both(drv, image_start_addr, image_size);
        if (rc != TMF8829_OK) {
            return rc;
        }
        idx = 0u;
        while (idx < image_size) {
            uint32_t room = (uint32_t)drv->buffer_len;
            uint32_t need = image_size - idx;
            chunk_len = (uint16_t)((need < room) ? need : room);
            n = drv->fw_image_read(drv, idx, drv->buffer, chunk_len, NULL);
            if (n < 0) {
                return n;
            }
            if ((uint32_t)n != (uint32_t)chunk_len) {
                return TMF8829_E_BOOTLOADER;
            }
            if (bus_write(drv, TMF8829_REG_FIFO, drv->buffer, chunk_len) !=
                TMF8829_OK) {
                return TMF8829_E_BUS;
            }
            idx += chunk_len;
        }
    } else {
        rc = bl_set_ram_addr(drv, image_start_addr);
        if (rc != TMF8829_OK) {
            return rc;
        }
        idx = 0u;
        {
            int n = drv->fw_image_read(drv, 0u, NULL, 0u, &image_size);
            if (n < 0) {
                return n;
            }
        }
        if (image_size == 0u) {
            return TMF8829_E_PARAM;
        }
        while (idx < image_size) {
            uint32_t remain = image_size - idx;
            uint32_t maxpl  = TMF8829_BL_MAX_PAYLOAD;
            chunk_len = (uint16_t)((remain < maxpl) ? remain : maxpl);
            if (TMF8829_BL_WR_HEADER + chunk_len > drv->buffer_len) {
                return TMF8829_E_PARAM;
            }
            {
                int n = drv->fw_image_read(drv, idx,
                                           &drv->buffer[TMF8829_BL_WR_HEADER],
                                           chunk_len, NULL);
                if (n < 0) {
                    return n;
                }
                if ((uint32_t)n != (uint32_t)chunk_len) {
                    return TMF8829_E_BOOTLOADER;
                }
            }
            rc = bl_write_ram_both(drv, (uint8_t)chunk_len,
                                   &drv->buffer[TMF8829_BL_WR_HEADER]);
            if (rc != TMF8829_OK) {
                return rc;
            }
            idx += chunk_len;
        }
    }

    rc = bl_start_ram_app(drv);
    if (rc != TMF8829_OK) {
        return rc;
    }
    return download_set_enable_ram(drv);
}

/* -------------------------------------------------------------------------- */
/* Results / histogram                                                        */
/* -------------------------------------------------------------------------- */

int tmf8829_read_results(tmf8829_driver_t *drv)
{
    uint32_t h_tick;
    uint32_t t_tick;
    uint8_t pixel_size;
    uint8_t layout;
    uint16_t payload;
    uint16_t size_to_read;
    uint16_t rx_size = 0u;
    int rx_ok;
    uint16_t eof_marker;

    const uint16_t hdr_read =
        (uint16_t)(TMF8829_PRE_HEADER_SIZE + TMF8829_FRAME_HEADER_SIZE);

    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (hdr_read > drv->buffer_len) {
        return TMF8829_E_PARAM;
    }

    if (bus_read(drv, TMF8829_REG_FIFO_STATUS, drv->buffer, hdr_read) !=
        TMF8829_OK) {
        return TMF8829_E_BUS;
    }

    if (drv->on_stream_header != NULL) {
        drv->on_stream_header(drv, drv->buffer, hdr_read);
    }

    h_tick = drv->ops->systick_us();
    t_tick = tmf8829_get_uint32(drv->buffer + 1u);
    layout = drv->buffer[TMF8829_PRE_HEADER_SIZE + 1u];
    pixel_size = tmf8829_get_pixel_size(layout);

    if (drv->_clk_corr_enable) {
        clk_add_pair(drv, h_tick, t_tick);
    }

    if ((drv->buffer[TMF8829_PRE_HEADER_SIZE] & TMF8829_FID_MASK) !=
        TMF8829_FID_RESULTS) {
        return TMF8829_E_NO_RESULT;
    }

    payload = tmf8829_get_uint16(drv->buffer + 7u);
    size_to_read = (uint16_t)(payload -
                              (TMF8829_FRAME_HEADER_SIZE -
                               TMF8829_FRAME_HEADER_OFFSET));

    rx_ok = TMF8829_OK;
    do {
        if (pixel_size > 0u && size_to_read > drv->buffer_len) {
            rx_size = (uint16_t)(((uint16_t)(drv->buffer_len / pixel_size)) *
                                 pixel_size);
        } else if (size_to_read > drv->buffer_len) {
            rx_size = drv->buffer_len;
        } else {
            rx_size = size_to_read;
        }
        if (rx_size == 0u) {
            if (size_to_read > 0u) {
                return TMF8829_E_PARAM;
            }
            break;
        }
        size_to_read = (uint16_t)(size_to_read - rx_size);
        if (bus_read(drv, TMF8829_REG_FIFO, drv->buffer, rx_size) !=
            TMF8829_OK) {
            return TMF8829_E_BUS;
        }
        rx_ok = TMF8829_OK;

        if (drv->_clk_corr_enable) {
            uint16_t size_to_correct = rx_size;
            if (size_to_read < TMF8829_FRAME_FOOTER_SIZE) {
                uint16_t trim = (uint16_t)(TMF8829_FRAME_FOOTER_SIZE -
                                            size_to_read);
                if (rx_size > trim) {
                    size_to_correct = (uint16_t)(rx_size - trim);
                } else {
                    size_to_correct = 0u;
                }
            }
            tmf8829_correct_distance_data_segment(drv, size_to_correct, layout);
        }

        if (drv->on_result != NULL) {
            drv->on_result(drv, drv->buffer, rx_size);
        }

    } while (size_to_read > 0u && rx_ok == TMF8829_OK);

    if (rx_size < TMF8829_FRAME_EOF_SIZE) {
        return TMF8829_E_NO_RESULT;
    }
    eof_marker = tmf8829_get_uint16(drv->buffer + rx_size -
                                    TMF8829_FRAME_EOF_SIZE);
    if (eof_marker == TMF8829_FRAME_EOF) {
        return TMF8829_OK;
    }
    return TMF8829_E_NO_RESULT;
}

int tmf8829_read_histogram(tmf8829_driver_t *drv)
{
    uint16_t payload;
    uint16_t size_to_read;
    uint16_t rx_size = 0u;
    int rx_ok;
    uint16_t eof_marker;

    const uint16_t hdr_read =
        (uint16_t)(TMF8829_PRE_HEADER_SIZE + TMF8829_FRAME_HEADER_SIZE);

    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (hdr_read > drv->buffer_len) {
        return TMF8829_E_PARAM;
    }

    if (bus_read(drv, TMF8829_REG_FIFO_STATUS, drv->buffer, hdr_read) !=
        TMF8829_OK) {
        return TMF8829_E_BUS;
    }

    if (drv->on_stream_header != NULL) {
        drv->on_stream_header(drv, drv->buffer, hdr_read);
    }

    if ((drv->buffer[TMF8829_PRE_HEADER_SIZE] & TMF8829_FID_MASK) !=
        TMF8829_FID_HISTOGRAMS) {
        return TMF8829_E_NO_RESULT;
    }

    payload = tmf8829_get_uint16(drv->buffer + 7u);
    size_to_read = (uint16_t)(payload -
                              (TMF8829_FRAME_HEADER_SIZE -
                               TMF8829_FRAME_HEADER_OFFSET));

    rx_ok = TMF8829_OK;
    do {
        if (size_to_read > drv->buffer_len) {
            rx_size = drv->buffer_len;
        } else {
            rx_size = size_to_read;
        }
        if (rx_size == 0u) {
            break;
        }
        size_to_read = (uint16_t)(size_to_read - rx_size);
        if (bus_read(drv, TMF8829_REG_FIFO, drv->buffer, rx_size) !=
            TMF8829_OK) {
            return TMF8829_E_BUS;
        }
        rx_ok = TMF8829_OK;

        if (drv->on_histogram != NULL) {
            drv->on_histogram(drv, drv->buffer, rx_size);
        }

    } while (size_to_read > 0u && rx_ok == TMF8829_OK);

    if (rx_size < TMF8829_FRAME_EOF_SIZE) {
        return TMF8829_E_NO_RESULT;
    }
    eof_marker = tmf8829_get_uint16(drv->buffer + rx_size -
                                    TMF8829_FRAME_EOF_SIZE);
    if (eof_marker == TMF8829_FRAME_EOF) {
        return TMF8829_OK;
    }
    return TMF8829_E_NO_RESULT;
}

/* -------------------------------------------------------------------------- */
/* Utilities                                                                  */
/* -------------------------------------------------------------------------- */

uint16_t tmf8829_get_uint16(const uint8_t *data)
{
    uint16_t t = data[1];
    return (uint16_t)((uint16_t)(t << 8) + data[0]);
}

uint32_t tmf8829_get_uint32(const uint8_t *data)
{
    uint32_t t = data[3];
    t = (t << 8u) + data[2];
    t = (t << 8u) + data[1];
    t = (t << 8u) + data[0];
    return t;
}

void tmf8829_set_uint16(uint16_t value, uint8_t *data)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

uint8_t tmf8829_get_pixel_size(uint8_t layout)
{
    uint8_t size;
    uint8_t num_peak = (uint8_t)(layout & TMF8829_RESULT_FORMAT_NR_PEAKS_MASK);
    uint8_t use_signal =
        (layout & TMF8829_RESULT_FORMAT_SIG_STRENGTH_MASK) ? 1u : 0u;
    uint8_t use_noise  = (layout & TMF8829_RESULT_FORMAT_NOISE_MASK) ? 1u : 0u;
    uint8_t use_xtalk  = (layout & TMF8829_RESULT_FORMAT_XTALK_MASK) ? 1u : 0u;

    size = (uint8_t)((num_peak * (uint8_t)(3u + 2u * use_signal)) +
                     (uint8_t)(2u * use_noise) + (uint8_t)(2u * use_xtalk));
    return size;
}

uint16_t tmf8829_correct_distance(const tmf8829_driver_t *drv, uint16_t distance)
{
    uint32_t c = distance;
    uint16_t ratio;

    if (drv == NULL) {
        return distance;
    }
    ratio = drv->_clk_corr_ratio_uq;
    c = (uint32_t)((ratio * c + (1u << 14)) >> 15);
    return TMF8829_SATURATE16(c);
}

void tmf8829_correct_distance_data_segment(tmf8829_driver_t *drv,
                                           uint16_t size, uint8_t layout)
{
    uint8_t pixel_size = tmf8829_get_pixel_size(layout);
    uint8_t num_peaks;
    uint16_t idxpixel;

    if (drv == NULL || pixel_size == 0u || size == 0u) {
        return;
    }
    num_peaks = (uint8_t)(layout & TMF8829_RESULT_FORMAT_NR_PEAKS_MASK);

    for (idxpixel = 0u; idxpixel < size;
         idxpixel = (uint16_t)(idxpixel + pixel_size)) {
        uint8_t idx = 0u;
        uint8_t idx_peak;

        if ((layout & TMF8829_RESULT_FORMAT_NOISE_MASK) != 0u) {
            idx = (uint8_t)(idx + 2u);
        }
        if ((layout & TMF8829_RESULT_FORMAT_XTALK_MASK) != 0u) {
            idx = (uint8_t)(idx + 2u);
        }

        for (idx_peak = 0u; idx_peak < num_peaks; idx_peak++) {
            uint16_t raw = tmf8829_get_uint16(&drv->buffer[idxpixel + idx]);
            uint16_t cor = tmf8829_correct_distance(drv, raw);
            tmf8829_set_uint16(cor, &drv->buffer[idxpixel + idx]);
            idx = (uint8_t)(idx + 3u);
            if ((layout & TMF8829_RESULT_FORMAT_SIG_STRENGTH_MASK) != 0u) {
                idx = (uint8_t)(idx + 2u);
            }
        }
    }
}

int tmf8829_clk_correction_set(tmf8829_driver_t *drv, uint8_t enable)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    drv->_clk_corr_enable = (uint8_t)(enable ? 1u : 0u);
    clk_reset(drv);
    return TMF8829_OK;
}

uint16_t tmf8829_clk_correction_ratio_uq15(const tmf8829_driver_t *drv)
{
    if (drv == NULL) {
        return (uint16_t)(1u << 15);
    }
    return drv->_clk_corr_ratio_uq;
}

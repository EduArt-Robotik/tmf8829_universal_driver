/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Driver core. Currently a scaffold: only init / set_log_level are
 * implemented. Other public functions return TMF8829_E_NOT_IMPLEMENTED until
 * filled in by subsequent batches.
 */

#include "tmf8829/tmf8829.h"
#include "tmf8829_internal.h"

#include <stddef.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static int tmf8829_validate_ops(const tmf8829_ops_t *ops)
{
    if (ops == NULL) {
        return TMF8829_E_PARAM;
    }
    /* Required callbacks. read_pin_int is optional. */
    if (ops->read             == NULL ||
        ops->write            == NULL ||
        ops->delay_us         == NULL ||
        ops->systick_us       == NULL ||
        ops->write_pin_enable == NULL) {
        return TMF8829_E_PARAM;
    }
    return TMF8829_OK;
}

static int tmf8829_validate_instance(const tmf8829_driver_t *drv)
{
    if (drv == NULL) {
        return TMF8829_E_PARAM;
    }
    if (drv->scratch == NULL || drv->scratch_len < TMF8829_MIN_SCRATCH_SIZE) {
        return TMF8829_E_PARAM;
    }
    if (drv->bus != TMF8829_BUS_I2C && drv->bus != TMF8829_BUS_SPI) {
        return TMF8829_E_PARAM;
    }
    /* For I2C, only 7-bit addresses are valid. */
    if (drv->bus == TMF8829_BUS_I2C && drv->i2c_addr > 0x7Fu) {
        return TMF8829_E_PARAM;
    }
    return TMF8829_OK;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int tmf8829_init(tmf8829_driver_t *drv, const tmf8829_ops_t *ops)
{
    int rc = tmf8829_validate_instance(drv);
    if (rc != TMF8829_OK) {
        return rc;
    }
    rc = tmf8829_validate_ops(ops);
    if (rc != TMF8829_OK) {
        return rc;
    }

    drv->ops = ops;

    /* Reset driver-private state. */
    drv->_clk_corr_ratio_uq = 0u;
    drv->_clk_corr_enable   = 1u; /* enabled by default */
    drv->_clk_corr_idx      = 0u;
    memset(drv->_host_ticks,   0, sizeof(drv->_host_ticks));
    memset(drv->_device_ticks, 0, sizeof(drv->_device_ticks));

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
    return TMF8829_E_NOT_IMPLEMENTED;
}

int tmf8829_disable(tmf8829_driver_t *drv)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    return TMF8829_E_NOT_IMPLEMENTED;
}

int tmf8829_handle_irq(tmf8829_driver_t *drv, uint8_t *mask_out)
{
    if (!tmf8829_internal_is_initialised(drv)) {
        return TMF8829_E_PARAM;
    }
    if (mask_out != NULL) {
        *mask_out = 0u;
    }
    return TMF8829_E_NOT_IMPLEMENTED;
}

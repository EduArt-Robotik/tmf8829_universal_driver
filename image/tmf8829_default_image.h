/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Optional default firmware-image adapter.
 *
 * Provides a @ref tmf8829_fw_image_read_fn implementation backed by the
 * built-in vendor TMF8829 application image. Only built when the CMake
 * option TMF8829_INCLUDE_DEFAULT_IMAGE is ON.
 *
 * Usage:
 *   tmf8829_driver_t drv = { ... };
 *   drv.fw_image_read = tmf8829_default_image_read;
 *   tmf8829_init(&drv, &ops);
 *   tmf8829_download_firmware(&drv);
 */

#ifndef TMF8829_DEFAULT_IMAGE_H
#define TMF8829_DEFAULT_IMAGE_H

#include "tmf8829/tmf8829.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief @ref tmf8829_fw_image_read_fn implementation backed by the bundled
 *        ams-OSRAM TMF8829 application image.
 */
int tmf8829_default_image_read(tmf8829_driver_t *drv,
                               uint32_t offset,
                               uint8_t *buf, uint32_t len,
                               uint32_t *image_total_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_DEFAULT_IMAGE_H */

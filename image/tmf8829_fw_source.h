/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 */

/**
 * @file tmf8829_fw_source.h
 * @brief Optional @ref tmf8829_fw_image_read_fn backed by the bundled firmware blob.
 *
 * Built only when CMake option @c TMF8829_INCLUDE_DEFAULT_IMAGE is @c ON
 * (@c tmf8829::default_image target).
 *
 * **Typical wiring**
 * @code
 * tmf8829_driver_t drv = { ... };
 * drv.fw_image_read = tmf8829_fw_source_read;
 * tmf8829_init(&drv, &ops);
 * // ... enter bootloader, then:
 * tmf8829_download_firmware(&drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, 0);
 * @endcode
 */

#ifndef TMF8829_FW_SOURCE_H
#define TMF8829_FW_SOURCE_H

#include "tmf8829/tmf8829.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read firmware bytes from the static @ref tmf8829_fw_image array.
 *
 * Implements the contract of @ref tmf8829_fw_image_read_fn:
 * - **Probe:** @p buf @c NULL, @p len @c 0 — if @p image_total_size is not @c NULL,
 *   stores the image length in bytes and returns @c 0.
 * - **Read:** copies up to @p len bytes from @p offset into @p buf; returns byte count,
 *   or @ref TMF8829_E_PARAM if @p buf is @c NULL while @p len is non-zero.
 * - Offsets at or beyond the image size yield return value @c 0.
 *
 * @note The @p drv argument is ignored; it may be @c NULL for probe/read calls.
 */
int tmf8829_fw_source_read(tmf8829_driver_t *drv,
                           uint32_t offset,
                           uint8_t *buf, uint32_t len,
                           uint32_t *image_total_size);

/* Backward-compatible alias for previous naming. */
#define tmf8829_default_image_read tmf8829_fw_source_read

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_FW_SOURCE_H */

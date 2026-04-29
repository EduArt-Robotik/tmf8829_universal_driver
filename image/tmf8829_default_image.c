/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Default firmware-image adapter (placeholder).
 *
 * The vendor binary blob will be vendored into this directory in a later
 * batch. For now this translation unit defines a small empty image and a
 * compliant tmf8829_default_image_read() so the opt-in build target compiles
 * and links.
 */

#include "tmf8829_default_image.h"

#include <string.h>

/* Placeholder image: 1 byte of 0x00. Replaced with the vendor blob in the
 * batch that wires up firmware download. */
static const uint8_t TMF8829_DEFAULT_IMAGE_DATA[1] = { 0x00u };

#define TMF8829_DEFAULT_IMAGE_SIZE \
    ((uint32_t)(sizeof(TMF8829_DEFAULT_IMAGE_DATA)))

int tmf8829_default_image_read(tmf8829_driver_t *drv,
                               uint32_t offset,
                               uint8_t *buf, uint32_t len,
                               uint32_t *image_total_size)
{
    (void)drv;

    if (image_total_size != NULL) {
        *image_total_size = TMF8829_DEFAULT_IMAGE_SIZE;
    }

    if (offset >= TMF8829_DEFAULT_IMAGE_SIZE) {
        return 0;
    }

    if (buf == NULL || len == 0u) {
        return TMF8829_E_PARAM;
    }

    uint32_t remaining = TMF8829_DEFAULT_IMAGE_SIZE - offset;
    uint32_t to_copy   = (len < remaining) ? len : remaining;
    memcpy(buf, &TMF8829_DEFAULT_IMAGE_DATA[offset], to_copy);
    return (int)to_copy;
}

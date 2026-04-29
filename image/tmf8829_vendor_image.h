/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 */

/**
 * @file tmf8829_vendor_image.h
 * @brief Declares the binary TMF8829 application image (ams-OSRAM reference build).
 *
 * The array is defined in @c tmf8829_vendor_image.c. Length must match
 * @ref TMF8829_VENDOR_IMAGE_NUM_BYTES (enforced with @c _Static_assert in the @c .c file).
 */

#ifndef TMF8829_VENDOR_IMAGE_H
#define TMF8829_VENDOR_IMAGE_H

#include <stdint.h>

/** Byte length of @ref tmf8829_vendor_image (0x36D0 for the current bundled file). */
#define TMF8829_VENDOR_IMAGE_NUM_BYTES 0x36D0u

/** ROM copy of the downloadable measurement-application image. */
extern const uint8_t tmf8829_vendor_image[TMF8829_VENDOR_IMAGE_NUM_BYTES];

#endif /* TMF8829_VENDOR_IMAGE_H */

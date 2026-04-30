/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 */

/**
 * @file tmf8829_fw_image.h
 * @brief Declares the binary TMF8829 application image (ams-OSRAM reference build).
 *
 * The array is defined in @c tmf8829_fw_image.c. Length must match
 * @ref TMF8829_FW_IMAGE_NUM_BYTES (enforced with @c _Static_assert in the @c .c file).
 */

#ifndef TMF8829_FW_IMAGE_H
#define TMF8829_FW_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Byte length of @ref tmf8829_fw_image (0x36D0 for the current bundled file). */
#define TMF8829_FW_IMAGE_NUM_BYTES 0x36D0u

/** ROM copy of the downloadable measurement-application image. */
extern const uint8_t tmf8829_fw_image[TMF8829_FW_IMAGE_NUM_BYTES];

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_FW_IMAGE_H */

/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Internal helpers shared between translation units of the driver.
 * Not part of the public API; this header is not installed.
 */

#ifndef TMF8829_INTERNAL_H
#define TMF8829_INTERNAL_H

#include "tmf8829/tmf8829.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Magic value written to @c drv->_initialised by @ref tmf8829_init. */
#define TMF8829_MAGIC_INITIALISED 0x8829D71Cu

/** True if @p drv has been successfully initialised. */
static inline int tmf8829_internal_is_initialised(const tmf8829_driver_t* drv) {
  return drv != NULL && drv->_initialised == TMF8829_MAGIC_INITIALISED;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_INTERNAL_H */

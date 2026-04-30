/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Default firmware-image adapter: exposes tmf8829_fw_image_read_fn over the
 * bundled binary in tmf8829_fw_image.c.
 */

#include "tmf8829_fw_source.h"

#include <string.h>

#include "tmf8829_fw_image.h"

int tmf8829_fw_source_read(tmf8829_driver_t* drv, uint32_t offset, uint8_t* buf, uint32_t len, uint32_t* image_total_size) {
  const uint32_t image_bytes = (uint32_t)sizeof(tmf8829_fw_image);

  (void)drv;

  if (image_total_size != NULL) {
    *image_total_size = image_bytes;
  }

  /* Size-only probe (e.g. FIFO download path). */
  if (buf == NULL && len == 0u) {
    return 0;
  }

  if (buf == NULL) {
    return TMF8829_E_PARAM;
  }

  if (offset >= image_bytes) {
    return 0;
  }

  {
    uint32_t remaining = image_bytes - offset;
    uint32_t to_copy   = (len < remaining) ? len : remaining;
    memcpy(buf, &tmf8829_fw_image[offset], to_copy);
    return (int)to_copy;
  }
}

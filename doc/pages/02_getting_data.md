# Getting Data Out of the Driver {#getting_data}

This page explains how measurement results get from the TMF8829 sensor to your application, why the driver uses callbacks for data delivery, and how to assemble a complete frame when the payload is larger than the driver buffer.

## Overview

Once a measurement is complete, the TMF8829 writes the result frame into an internal FIFO.
Your application retrieves that frame by calling @ref tmf8829_read_results (or @ref tmf8829_read_histogram for histogram data).
During this call the driver reads the FIFO in chunks, clock-corrects distance data, and delivers each chunk to your application through one of the registered callback functions set on the @ref tmf8829_driver_t struct:

| Field | Callback type | When invoked |
|---|---|---|
| `on_stream_header` | @ref tmf8829_on_stream_header_fn | Once per `tmf8829_read_results` / `tmf8829_read_histogram` call, before any payload. Carries the fused pre-header (5 bytes) + frame header (16 bytes) = **21 bytes total**. |
| `on_result` | @ref tmf8829_on_result_fn | One or more times per `tmf8829_read_results` call, once for each FIFO chunk. The **final chunk ends with the 12-byte frame footer** (including the 2-byte EOF marker). |
| `on_histogram` | @ref tmf8829_on_histogram_fn | One or more times per `tmf8829_read_histogram` call, once for each FIFO chunk. The final chunk also ends with the 12-byte frame footer. |

> All callbacks are optional. Set any field to `NULL` to ignore that data path.

## Why Callbacks instead of a Return Buffer

The TMF8829 stores result frames in a sensor-side FIFO (`TMF8829_FIFO_SIZE` = 8192 bytes).
A single measurement frame — especially at high resolutions such as 48×32 — can be several kilobytes in size.
Allocating a contiguous host buffer large enough for the worst-case frame would waste RAM on constrained microcontrollers.

Instead the driver re-uses the small, caller-supplied `drv->buffer` as a staging area and calls the registered callback once for each chunk read from the FIFO.
This keeps the required RAM at `TMF8829_MIN_BUFFER_SIZE` (256 bytes minimum) regardless of the measurement resolution.
A larger buffer reduces the number of bus transactions and callback invocations, but is never mandatory.

## The `on_result` Callback Is Called Multiple Times

**This is the most important thing to understand about the data path.**

The total payload of one result frame equals the number of measurement zones multiplied by the bytes per zone (which depends on the active result format).
For large configurations (e.g. 48×32 at full format) this can far exceed `drv->buffer_len`.
The driver therefore loops, reading `drv->buffer_len` bytes (or fewer for the final chunk) and invoking `on_result` each iteration:

```text
tmf8829_read_results()
  │
  ├─ read 21 bytes from FIFO_STATUS
  │    └─ on_stream_header(drv, buf, 21)                            [once]
  │         ├─ bytes  0– 4 : pre-header  (TMF8829_PRE_HEADER_SIZE   = 5)
  │         └─ bytes  5–20 : frame header (TMF8829_FRAME_HEADER_SIZE = 16)
  │
  ├─ loop while payload remaining:
  │    ├─ read min(buffer_len, remaining) bytes from FIFO
  │    ├─ apply clock correction to distance data (if enabled)
  │    └─ on_result(drv, buf, chunk_len)                            [one or more times]
  │         ├─ intermediate chunks : pure measurement data
  │         └─ final chunk         : remaining measurement data
  │                                  + 12-byte footer (TMF8829_FRAME_FOOTER_SIZE)
  │                                    └─ last 2 bytes: EOF marker 0xE0F7
  │
  └─ verify EOF marker in final chunk
```

Your `on_result` implementation must therefore **accumulate** all chunks to reconstruct the full frame.
The callback receives a pointer to `drv->buffer` (overwritten each iteration) and the chunk length.
Copy the data out before the function returns.

> A buffer of at least the full payload size reduces `on_result` to a single call per measurement.

## Minimal Setup to Trigger a Measurement

The steps below show the minimum code required to have `on_result` fire.
Error handling is omitted for clarity.

### 1. Implement the Result Callback

```c
static uint8_t my_frame_buf[5120]; /* large buffer */
static uint16_t my_frame_len = 0;

static void on_result(tmf8829_driver_t* drv, const uint8_t* data, uint16_t len) {
    (void)drv;
    /* Accumulate chunks into a contiguous frame buffer. */
    if (my_frame_len + len <= sizeof(my_frame_buf)) {
        memcpy(my_frame_buf + my_frame_len, data, len);
        my_frame_len += len;
    }
}
```

### 2. Register the Header Callback (Optional)

If you also want the 21-byte stream header, implement `on_stream_header` and register it on the driver instance.
The callback fires once per `tmf8829_read_results` call, before any `on_result` invocations, so you can use it to initialise your accumulation buffer:

```c
static void on_header(tmf8829_driver_t* drv, const uint8_t* data, uint16_t len) {
    (void)drv;
    /* Copy header to the start of the frame buffer and advance the write position. */
    memcpy(my_frame_buf, data, len);
    my_frame_len = len;
}
```

### 3. Configure the Driver Instance

```c
static uint8_t drv_buf[512]; /* driver staging buffer; larger = fewer bus transactions */

static tmf8829_driver_t sensor = {
    .bus              = TMF8829_BUS_I2C,
    .i2c_addr         = TMF8829_DEFAULT_I2C_ADDR,
    .user_ctx         = &my_i2c_handle,
    .buffer           = drv_buf,
    .buffer_len       = sizeof(drv_buf),
    .on_result        = on_result,   /* register the callback */
    .on_stream_header = on_header,    /* optional; remove if header is not needed */
};
```

### 4. Initialise, Enable, and Download Firmware

```c
tmf8829_init(&sensor, &my_ops);
tmf8829_enable(&sensor);
tmf8829_download_firmware(&sensor, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, /*use_fifo=*/ false);
```

### 5. Load a Configuration Preset and Start Measuring

```c
/* Load a preset that matches the desired resolution. */
tmf8829_command(&sensor, TMF8829_CMD_LOAD_CFG_8X8, /*timeout_ms=*/ 50);

/* Enable the result interrupt and start cyclic measurement. */
tmf8829_clr_and_enable_interrupts(&sensor, TMF8829_INT_RESULTS);
tmf8829_start_measurement(&sensor);
```

### 6. Service the Interrupt in Your Main Loop (or ISR)

Poll the interrupt status register and call @ref tmf8829_read_results when the result-ready bit is set:

```c
uint8_t pending = 0;
tmf8829_get_and_clr_interrupts(&sensor, TMF8829_INT_RESULTS, &pending);
if (pending & TMF8829_INT_RESULTS) {
    my_frame_len = 0; /* reset accumulator before each new measurement */
    tmf8829_read_results(&sensor);
    /* on_result has now been called one or more times; my_frame_buf is complete. */
    process_frame(my_frame_buf, my_frame_len);
}
```

> Reset the accumulator (`my_frame_len = 0`) **before** calling @ref tmf8829_read_results, not inside `on_result`, so the first chunk is not accidentally appended to a previous incomplete frame.

## Frame Layout Reference

The following table shows the complete on-wire frame structure and which callback delivers each section:

| Section | Size (bytes) | Callback | Notes |
|---|---|---|---|
| Pre-header | 5 (`TMF8829_PRE_HEADER_SIZE`) | `on_stream_header` | Device tick, FIFO status |
| Frame header | 16 (`TMF8829_FRAME_HEADER_SIZE`) | `on_stream_header` | Frame id, payload length, focal-plane layout |
| Measurement data | `pixel_size × num_zones` | `on_result` (one or more chunks) | Distance + optional signal/noise per zone |
| Frame footer | 12 (`TMF8829_FRAME_FOOTER_SIZE`) | `on_result` (appended to **final** chunk) | Calibration metadata; last 2 bytes are the EOF marker `0xE0F7` |

The `on_stream_header` buffer is therefore always exactly **21 bytes**.
The total `on_result` data across all chunks is `pixel_size × num_zones + 12` bytes.

<div class="section_buttons">

| Read Previous | Read Next |
|:--|--:|
| [Integration Guide](01_integration_guide.md) | |

</div>

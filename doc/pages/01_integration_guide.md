# Integration Guide {#integration_guide}

This guide walks you through implementing the platform callbacks and bringing up a TMF8829 sensor on any microcontroller or host system.

## Overview

The driver communicates with the TMF8829 exclusively through a callback table (`tmf8829_ops_t`). Your job is to implement these callbacks for your platform's bus peripheral, timer, and GPIO. Once that is done, the full driver API (enable, configure, measure, download firmware) works without further platform code.

```text
┌────────────────────────────────────┐
│  Application                       │
├────────────────────────────────────┤
│  tmf8829_universal_driver          │
│  (tmf8829_init, tmf8829_enable,    │
│   tmf8829_read_results, ...)       │
├────────────────────────────────────┤
│  tmf8829_ops_t  (your callbacks)   │
├────────────────────────────────────┤
│  Platform HAL / registers          │
└────────────────────────────────────┘
```

## Step 1: Include the Driver

Add the library to your CMake project:

```cmake
add_subdirectory(external/tmf8829_universal_driver)
target_link_libraries(your_firmware PRIVATE tmf8829::tmf8829)
```

In your source file:

```c
#include <tmf8829/tmf8829.h>
```

## Step 2: Implement the Callback Table

You must provide four required callbacks and one or two optional callbacks in a `tmf8829_ops_t` struct.

### Required Callbacks

| Callback | Signature | Purpose |
|----------|-----------|---------|
| `read` | `int (tmf8829_driver_t* drv, uint8_t reg, uint8_t* buf, uint16_t len)` | Read `len` bytes starting at register `reg` into `buf`. |
| `write` | `int (tmf8829_driver_t* drv, uint8_t reg, const uint8_t* buf, uint16_t len)` | Write `len` bytes from `buf` starting at register `reg`. |
| `delay_us` | `void (uint32_t us)` | Busy-wait for at least `us` microseconds. |
| `systick_us` | `uint32_t (void)` | Return a free-running microsecond counter (32-bit wrap is fine). |

### Optional Callbacks

| Callback | Signature | Purpose |
|----------|-----------|---------|
| `write_pin_enable` | `void (tmf8829_driver_t* drv, int high)` | Drive the sensor enable/power pin high or low. Set to `NULL` if the enable pin is not controllable (permanently high, e.g. hard wired on the PCB). |
| `read_pin_int` | `int (tmf8829_driver_t* drv)` | Read the interrupt GPIO level. Return 1 if asserted, 0 if not. Set to `NULL` if you poll `TMF8829_REG_INT_STATUS` instead. |

All callbacks return `0` on success and a negative value on failure (matching the driver's own convention).

### Bus Read/Write Details

The `drv` pointer gives you access to:
- `drv->bus` — whether the sensor uses I2C or SPI
- `drv->i2c_addr` — the 7-bit I2C address (only relevant for I2C)
- `drv->user_ctx` — an opaque `void*` you set to your peripheral handle

**I2C:** Perform an I2C memory-read/write with `reg` as the 8-bit sub-address.

**SPI:** Prefix with a 2-byte header (`TMF8829_SPI_RD_CMD`/`TMF8829_SPI_WR_CMD` + register), manage NSS manually. For reads, discard the one stuff byte returned by the device before the actual payload.

## Step 3: Create the Driver Instance

```c
static uint8_t sensor_buffer[TMF8829_MIN_BUFFER_SIZE];

static tmf8829_driver_t sensor = {
    .bus        = TMF8829_BUS_I2C,      /* or TMF8829_BUS_SPI */
    .i2c_addr   = TMF8829_DEFAULT_I2C_ADDR,
    .user_ctx   = &my_i2c_handle,       /* platform peripheral handle */
    .buffer     = sensor_buffer,
    .buffer_len = sizeof(sensor_buffer),
};
```

The buffer is used internally for register staging and firmware download. It must be at least `TMF8829_MIN_BUFFER_SIZE` bytes. Larger buffers enable faster FIFO downloads.

## Step 4: Initialise and Enable

```c
static const tmf8829_ops_t my_ops = {
    .read             = my_read,
    .write            = my_write,
    .delay_us         = my_delay_us,
    .systick_us       = my_systick_us,
    .write_pin_enable = my_write_pin_enable,
    .read_pin_int     = NULL,  /* optional */
};

int rc = tmf8829_init(&sensor, &my_ops);
if (rc != TMF8829_OK) { /* handle error */ }

rc = tmf8829_enable(&sensor);
if (rc != TMF8829_OK) { /* handle error */ }
```

`tmf8829_enable()` performs the power-on sequence: drives enable low (capacitor discharge), then high, and polls until the CPU reports ready.

## Step 5: Download Firmware

The tmf8829 main firmware is stored in the sensors RAM memory.
The RAM is volatile and looses its content when the sensor is powered down.
Therefore we need to upload the sensor firmware to the sensor after every power cycle:

```c
#include <tmf8829/tmf8829_fw_source.h>  /* only if linking tmf8829::default_image */

sensor.fw_image_read = tmf8829_fw_source_read;

rc = tmf8829_download_firmware(&sensor, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, false);
```

Or supply your own `tmf8829_fw_image_read_fn` to read from external flash, a filesystem, etc.

## Step 6: Configure and Measure

```c
/* Load a configuration preset */
rc = tmf8829_send_command(&sensor, TMF8829_CMD_LOAD_CFG_8X8);

/* Start cyclic measurement */
rc = tmf8829_send_command(&sensor, TMF8829_CMD_MEASURE);

/* In your main loop or ISR: */
uint8_t irqs = 0;
rc = tmf8829_get_and_clr_interrupts(&sensor, TMF8829_INT_RESULT_IRQ, &irqs);
if (irqs & TMF8829_INT_RESULT_IRQ) {
    rc = tmf8829_read_results(&sensor);
}
```

## Multi-Instance Support

Each physical sensor gets its own `tmf8829_driver_t`. They can share the same `tmf8829_ops_t` if they are on the same bus type. Differentiate them via `user_ctx` and `i2c_addr`:

```c
static tmf8829_driver_t sensor_a = {
    .bus      = TMF8829_BUS_I2C,
    .i2c_addr = 0x41,
    .user_ctx = &hi2c1,
    .buffer   = buf_a,
    .buffer_len = sizeof(buf_a),
};

static tmf8829_driver_t sensor_b = {
    .bus      = TMF8829_BUS_SPI,
    .i2c_addr = 0,          /* unused for SPI */
    .user_ctx = &hspi1,
    .buffer   = buf_b,
    .buffer_len = sizeof(buf_b),
};
```

Both instances can share the same ops table — the `read`/`write` implementations dispatch on `drv->bus`.

## Reference: STM32 HAL Implementation

Below is a condensed reference showing how the callbacks are implemented for an STM32G4 using the HAL.

### Per-Instance Context

Since each `tmf8829_driver_t` carries a `user_ctx` pointer, we use a small struct to hold both the bus handle and the per-sensor pin assignment:

```c
#include "tmf8829/tmf8829.h"
#include "tmf8829/tmf8829_regs.h"
#include "stm32g4xx_hal.h"

/** Per-sensor platform context passed via drv->user_ctx. */
typedef struct {
    I2C_HandleTypeDef* hi2c;       /* NULL when using SPI */
    SPI_HandleTypeDef* hspi;       /* NULL when using I2C */
    GPIO_TypeDef*      nss_port;   /* SPI chip-select port (SPI only) */
    uint16_t           nss_pin;    /* SPI chip-select pin  (SPI only) */
    GPIO_TypeDef*      en_port;    /* Enable / power-gate port */
    uint16_t           en_pin;     /* Enable / power-gate pin  */
} tmf8829_platform_ctx_t;
```

### Platform Callbacks

```c
/* ---------- Timing ---------- */

static void delay_us(uint32_t us) {
    const uint32_t start = systick_us();
    while ((uint32_t)(systick_us() - start) < us) {}
}

static uint32_t systick_us(void) {
    /* Return a platform-specific microsecond counter. */
    return my_get_us_tick();
}

/* ---------- Enable Pin ---------- */

static void write_pin_enable(tmf8829_driver_t* drv, int high) {
    tmf8829_platform_ctx_t* ctx = (tmf8829_platform_ctx_t*)drv->user_ctx;
    HAL_GPIO_WritePin(ctx->en_port, ctx->en_pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---------- I2C ---------- */

static int i2c_read(tmf8829_driver_t* drv, uint8_t reg, uint8_t* buf, uint16_t len) {
    tmf8829_platform_ctx_t* ctx = (tmf8829_platform_ctx_t*)drv->user_ctx;
    uint16_t dev_addr = (uint16_t)((drv->i2c_addr & 0x7F) << 1);
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(ctx->hi2c, dev_addr, reg,
                                             I2C_MEMADD_SIZE_8BIT, buf, len, 50);
    return (st == HAL_OK) ? 0 : -1;
}

static int i2c_write(tmf8829_driver_t* drv, uint8_t reg, const uint8_t* buf, uint16_t len) {
    tmf8829_platform_ctx_t* ctx = (tmf8829_platform_ctx_t*)drv->user_ctx;
    uint16_t dev_addr = (uint16_t)((drv->i2c_addr & 0x7F) << 1);
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(ctx->hi2c, dev_addr, reg,
                                              I2C_MEMADD_SIZE_8BIT,
                                              (uint8_t*)buf, len, 50);
    return (st == HAL_OK) ? 0 : -1;
}

/* ---------- SPI ---------- */

static int spi_read(tmf8829_driver_t* drv, uint8_t reg, uint8_t* buf, uint16_t len) {
    tmf8829_platform_ctx_t* ctx = (tmf8829_platform_ctx_t*)drv->user_ctx;
    uint8_t hdr[2] = { TMF8829_SPI_RD_CMD, reg };

    HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(ctx->hspi, hdr, 2, 50);

    /* Discard one stuff byte before actual data. */
    uint8_t stuff = 0;
    HAL_SPI_Receive(ctx->hspi, &stuff, 1, 50);

    HAL_StatusTypeDef st = HAL_SPI_Receive(ctx->hspi, buf, len, 50);
    HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_SET);
    return (st == HAL_OK) ? 0 : -1;
}

static int spi_write(tmf8829_driver_t* drv, uint8_t reg, const uint8_t* buf, uint16_t len) {
    tmf8829_platform_ctx_t* ctx = (tmf8829_platform_ctx_t*)drv->user_ctx;
    uint8_t hdr[2] = { TMF8829_SPI_WR_CMD, reg };

    HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(ctx->hspi, hdr, 2, 100);
    if (st == HAL_OK && len > 0 && buf != NULL) {
        st = HAL_SPI_Transmit(ctx->hspi, (uint8_t*)buf, len, 100);
    }
    HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_SET);
    return (st == HAL_OK) ? 0 : -1;
}

/* ---------- Unified dispatcher ---------- */

static int port_read(tmf8829_driver_t* drv, uint8_t reg, uint8_t* buf, uint16_t len) {
    return (drv->bus == TMF8829_BUS_SPI) ? spi_read(drv, reg, buf, len)
                                         : i2c_read(drv, reg, buf, len);
}

static int port_write(tmf8829_driver_t* drv, uint8_t reg, const uint8_t* buf, uint16_t len) {
    return (drv->bus == TMF8829_BUS_SPI) ? spi_write(drv, reg, buf, len)
                                         : i2c_write(drv, reg, buf, len);
}

/* ---------- Ops table (shared by all instances) ---------- */

const tmf8829_ops_t my_platform_ops = {
    .read             = port_read,
    .write            = port_write,
    .delay_us         = delay_us,
    .systick_us       = systick_us,
    .write_pin_enable = write_pin_enable,
    .read_pin_int     = NULL,
};
```

### Instantiation Example (Two Sensors)

```c
static uint8_t buf_a[TMF8829_MIN_BUFFER_SIZE];
static uint8_t buf_b[TMF8829_MIN_BUFFER_SIZE];

static tmf8829_platform_ctx_t ctx_a = {
    .hi2c    = &hi2c1,
    .en_port = GPIOA, .en_pin = GPIO_PIN_4,
};

static tmf8829_platform_ctx_t ctx_b = {
    .hspi    = &hspi1,
    .nss_port = GPIOB, .nss_pin = GPIO_PIN_6,
    .en_port  = GPIOC, .en_pin  = GPIO_PIN_0,
};

static tmf8829_driver_t sensor_a = {
    .bus        = TMF8829_BUS_I2C,
    .i2c_addr   = 0x41,
    .user_ctx   = &ctx_a,
    .buffer     = buf_a,
    .buffer_len = sizeof(buf_a),
};

static tmf8829_driver_t sensor_b = {
    .bus        = TMF8829_BUS_SPI,
    .user_ctx   = &ctx_b,
    .buffer     = buf_b,
    .buffer_len = sizeof(buf_b),
};
```

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| `tmf8829_init` returns `TMF8829_E_PARAM` | Missing required callback, buffer too small, or invalid I2C address (> 0x7F). |
| `tmf8829_enable` returns `TMF8829_E_TIMEOUT` | Enable pin not wired, wrong GPIO port/pin, or sensor not powered. |
| Garbage data from reads | SPI: stuff byte not discarded. I2C: address not left-shifted. |
| Firmware download fails | Buffer too small for chunk size, or `fw_image_read` returns short reads. |
| Clock correction drift | `systick_us` wraps too fast or has low resolution. Ensure monotonic microsecond accuracy. |

<div class="section_buttons">

| Read Previous | |
|:--|--:|
| [Readme](../../README.md) | |

</div>

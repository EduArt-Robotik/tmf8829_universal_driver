# tmf8829_universal_driver

> A small, portable, multi-instance C11 driver for the **ams-OSRAM TMF8829**
> direct time-of-flight ranging sensor.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Status: scaffold](https://img.shields.io/badge/status-scaffold-orange)

## Status

**Pre-alpha scaffold.** The library skeleton, public API surface, and build
system are in place. Function bodies are stubs that return
`TMF8829_E_NOT_IMPLEMENTED`. See [Roadmap](#roadmap).

## Why another TMF8829 driver?

ams-OSRAM publishes two reference driver packages — one for **Arduino** and one
for the **Linux kernel**. They are functional and well-commented, but their
shared "shim" architecture has properties that make integration into a larger
firmware project awkward:

- **The bus (I2C vs SPI) is selected at compile time** via mutually exclusive
  `USE_I2C` / `USE_SPI` `#define`s. You cannot run one TMF8829 on I2C and
  another on SPI in the same firmware.
- **Single-device assumption baked in**: enable / interrupt pin numbers are
  global `#define`s; the platform shim is not parameterized by an instance
  handle.
- **Layering inversion**: the platform "shim" includes the application driver
  header and calls into it (e.g. `tmf8829_app_process_irq` lives in the shim).
- **Tight platform coupling in headers**: including the core driver header
  transitively pulls in `<linux/kernel.h>` / `<linux/gpio.h>` / `<Arduino.h>`.
- **Application policy in the driver**: CSV printing, frame post-processing,
  and console input are part of the "shim" interface.
- **Buffer sizing escapes the platform layer**: `DATA_BUFFER_SIZE` is set in
  the shim header and used to size a member of the core driver struct, so the
  driver layout depends on the platform port.

This project keeps the proven **register-level protocol logic** of the
ams-OSRAM drivers but wraps it in a tighter abstraction:

- One `tmf8829_driver_t` instance per sensor.
- Bus type is a runtime field on the driver struct.
- All platform interaction happens through a single `tmf8829_ops_t` callback
  table (`read`, `write`, `delay_us`, `systick_us`, `write_pin_enable`,
  `read_pin_int`).
- Driver headers depend on `<stdint.h>` and `<stddef.h>` only.
- Pin configuration, interrupt registration, bus initialization, and clock
  setup are entirely the host's concern. The driver only knows "drive enable
  pin high" / "what's the IRQ pin reading?".
- Logging is one optional callback. CSV / `printf` macros are gone.
- Frames are delivered by **pull**: the host calls `tmf8829_handle_irq()` and
  then `tmf8829_read_results()` / `tmf8829_read_histogram()`.
- The driver does no dynamic allocation. Scratch memory is caller-provided.
- The default firmware image is **opt-in** — it lives under `image/` as a
  separate CMake target you only link if you want the bundled blob.

## Project layout

```text
tmf8829_universal_driver/
├── CMakeLists.txt          # adds tmf8829_universal STATIC; options gate tests + image
├── LICENSE                 # MIT (with attribution to ams-OSRAM)
├── include/tmf8829/
│   ├── tmf8829.h           # umbrella; init, enable, measurement API
│   ├── tmf8829_types.h     # tmf8829_bus_t, tmf8829_err_t, log levels, forward decls
│   ├── tmf8829_ops.h       # tmf8829_ops_t platform callback table
│   └── tmf8829_regs.h      # register addresses, command opcodes, frame layout
├── src/
│   ├── tmf8829.c           # protocol / state machine (currently stubs)
│   └── tmf8829_internal.h  # private helpers
├── image/                  # opt-in vendor firmware blob (not built by default)
│   ├── tmf8829_default_image.h
│   └── tmf8829_default_image.c
└── tests/
    ├── test_smoke.cpp      # link / type / error-code sanity (Catch2 v3)
    └── support/            # in-memory fakes for upcoming behavior tests
```

## Requirements

- **CMake ≥ 3.13**
- **C11** compiler for the library (any modern GCC / Clang / armcc / IAR)
- **C++17** compiler for the tests (only when `TMF8829_BUILD_TESTS=ON`)

The library itself has **no third-party dependencies**. Catch2 is fetched only
when tests are built.

## Build

### As a CMake subdirectory (the typical case)

```cmake
add_subdirectory(external/tmf8829_universal_driver)
target_link_libraries(your_firmware PRIVATE tmf8829::tmf8829)
```

### Standalone (for development / running tests)

```sh
cmake -S external/tmf8829_universal_driver -B build -DTMF8829_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

When the project is built as the top-level CMake project, `TMF8829_BUILD_TESTS`
is auto-enabled.

## CMake options

| Option | Default | Purpose |
|---|---|---|
| `TMF8829_BUILD_TESTS` | `OFF` (auto-`ON` standalone) | Build Catch2 unit tests. Pulls Catch2 v3 via `FetchContent`. **Disable on embedded targets.** |
| `TMF8829_INCLUDE_DEFAULT_IMAGE` | `OFF` | Build the bundled vendor firmware image as a separate `tmf8829::default_image` static library. |

## Usage sketch

> **Note**: the API below describes the intended shape. Function bodies are
> currently stubs (`TMF8829_E_NOT_IMPLEMENTED`); see [Roadmap](#roadmap).

```c
#include <tmf8829/tmf8829.h>

/* Provide one ops table per port (typically once per project). */
static const tmf8829_ops_t my_stm32_ops = {
    .read              = my_stm32_read,
    .write             = my_stm32_write,
    .delay_us          = my_stm32_delay_us,
    .systick_us        = my_stm32_systick_us,
    .write_pin_enable  = my_stm32_write_pin_enable,
    .read_pin_int      = my_stm32_read_pin_int,
};

/* One driver instance per sensor. */
static uint8_t scratch_a[512];
static tmf8829_driver_t sensor_a = {
    .bus         = TMF8829_BUS_I2C,
    .i2c_addr    = 0x41,
    .user_ctx    = &my_i2c_handle_for_sensor_a,
    .scratch     = scratch_a,
    .scratch_len = sizeof(scratch_a),
};

int rc = tmf8829_init(&sensor_a, &my_stm32_ops);
```

A second sensor on **SPI** in the same firmware is just a second driver
instance with `bus = TMF8829_BUS_SPI` — no recompilation, no `#ifdef` flags.

## Roadmap

- [x] Scaffold: layout, public types, CMake, Catch2 smoke test.
- [ ] `tmf8829_init` parameter validation + ops contract enforcement.
- [ ] Enable / disable / CPU-ready polling.
- [ ] Bus dispatch (I2C / SPI runtime).
- [ ] Bootloader rewrite + firmware download via `fw_image_read` callback.
- [ ] Application register layer: command, config-page load/write.
- [ ] Result / histogram pull.
- [ ] Clock correction (driver field, no `#define`s).
- [ ] Vendor blob in `image/` + default `fw_image_read` adapter.
- [ ] Doxygen pass + GitHub Actions CI.

## License & attribution

This project is released under the **MIT License** — see [LICENSE](LICENSE).

It is a clean-room reimplementation; protocol details (register addresses,
command opcodes, bootloader sequence, frame layout) were learned from the
ams-OSRAM TMF8829 Arduino and Linux reference drivers, both of which are
distributed under the MIT license. The `LICENSE` file contains the
acknowledgment.

The optional default firmware image under `image/` is the unmodified binary
produced by ams-OSRAM AG and redistributed unchanged under the same terms.

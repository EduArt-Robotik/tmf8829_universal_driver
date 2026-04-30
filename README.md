# tmf8829_universal_driver

> A small, portable, multi-instance C11 driver for the **ams-OSRAM TMF8829** direct time-of-flight ranging sensor.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE) ![Version](https://img.shields.io/badge/version-0.1.0-blue)

## Why another TMF8829 driver?

ams-OSRAM publishes two reference driver packages, one for [**Arduino**](https://github.com/ams-osram/tmf8829_driver_arduino) and one for the [**Linux kernel**](https://github.com/ams-osram/tmf8829_driver_linux).
They are functional and well-commented, but their shared "shim" architecture has properties that make integration into a larger firmware project awkward:

- **The bus (I2C vs SPI) is selected at compile time** via mutually exclusive `USE_I2C` / `USE_SPI` `#define`s.
  You cannot run one TMF8829 on I2C and another on SPI in the same firmware.
- **Single-device assumption baked in**: enable / interrupt pin numbers are global `#define`s.
  The platform shim is not parameterized by an instance handle.
- **Layering inversion**: the platform "shim" includes the application driver header and calls into it.
- **Tight platform coupling in headers**: including the core driver header transitively pulls in `<linux/kernel.h>` / `<linux/gpio.h>` / `<Arduino.h>`.
- **Application policy in the driver**: CSV printing, frame post-processing, and console input are part of the "shim" interface.
- **Buffer sizing escapes the platform layer**: `DATA_BUFFER_SIZE` is set in the shim header and used to size a member of the core driver struct, so the driver layout depends on the platform port.

This project keeps the proven **register-level protocol logic** of the ams-OSRAM drivers but wraps it in a tighter abstraction:

- One `tmf8829_driver_t` instance per sensor.
- Bus type (I2C or SPI) is a runtime field on the driver struct.
- All platform interaction happens through a single `tmf8829_ops_t` callback table (`read`, `write`, `delay_us`, `systick_us`, `write_pin_enable`, `read_pin_int`).
- Driver headers depend on `<stdint.h>` and `<stddef.h>` only.
- Pin configuration, interrupt registration, bus initialization, and clock setup are entirely the host's concern.
  The driver only knows "drive enable pin high" / "what's the IRQ pin reading?".
- Logging is one optional callback. CSV / `printf` macros are gone.
- Frames are delivered by **pull**: clear interrupts with `tmf8829_get_and_clr_interrupts`, then `tmf8829_read_results()` / `tmf8829_read_histogram()`.
- The driver does no dynamic allocation. The buffer memory is caller-provided.
- The default firmware image is **opt-in**.
  It lives under `image/` as a separate CMake target you only link if you want the bundled blob.
  This allows storing the image in external storage instead of embedding it in firmware.

## Project layout

```text
tmf8829_universal_driver/
├── .github/workflows/ci.yml
├── cmake/tmf8829_universal_driverConfig.cmake.in
├── CMakeLists.txt          # core target + options + install/export package files
├── LICENSE                 # MIT
├── include/tmf8829/
│   ├── tmf8829.h           # init, enable, measurement API
│   ├── tmf8829_internal.h  # init magic + inline guards (not for app use)
│   ├── tmf8829_types.h     # tmf8829_bus_t, tmf8829_err_t, log levels, forward decls
│   ├── tmf8829_ops.h       # tmf8829_ops_t platform callback table
│   └── tmf8829_regs.h      # register addresses, command opcodes, frame layout
├── src/
│   └── tmf8829.c           # protocol implementation
├── image/                  # opt-in vendor firmware (not built by default)
│   ├── CMakeLists.txt
│   ├── tmf8829_fw_image.c / .h       # vendored ams-OSRAM app binary
│   ├── tmf8829_fw_source.h
│   └── tmf8829_fw_source.c           # fw_image_read adapter -> fw_image blob
└── tests/
    ├── CMakeLists.txt
    ├── test_*.cpp          # Catch2 v3 test units
    └── support/            # fakes and shared fixture helpers
```

## Requirements

- **CMake ≥ 3.13**
- **C11** compiler for the library (any modern GCC / Clang / armcc / IAR)
- **C++17** compiler for the tests (only when `TMF8829_BUILD_TESTS=ON`)

The library itself has **no third-party dependencies**.
Catch2 is fetched only when tests are built.

## Build

### As a CMake subdirectory (the typical case)

```cmake
add_subdirectory(external/tmf8829_universal_driver)
target_link_libraries(your_firmware PRIVATE tmf8829::tmf8829)
```

### Bundled application image

```cmake
set(TMF8829_INCLUDE_DEFAULT_IMAGE ON CACHE BOOL "" FORCE)
add_subdirectory(external/tmf8829_universal_driver)
target_link_libraries(your_firmware PRIVATE tmf8829::tmf8829 tmf8829::default_image)
```

Then assign `drv.fw_image_read = tmf8829_fw_source_read` before `tmf8829_init`, and call `tmf8829_download_firmware(&drv, TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT, use_fifo)` from bootloader context.

### Standalone (for development / running tests)

```sh
cmake -S external/tmf8829_universal_driver -B build -DTMF8829_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

When the project is built as the top-level CMake project, `TMF8829_BUILD_TESTS` is auto-enabled.

### Install and consume via CMake's `find_package`

This project installs headers, static libraries, and CMake package files for `find_package(tmf8829_universal_driver CONFIG REQUIRED)` consumers.

```sh
cmake -S external/tmf8829_universal_driver -B build -DTMF8829_INCLUDE_DEFAULT_IMAGE=ON
cmake --build build
cmake --install build --prefix /your/install/prefix
```

Consumer example:

```cmake
find_package(tmf8829_universal_driver CONFIG REQUIRED)
target_link_libraries(your_firmware PRIVATE tmf8829::tmf8829)
# Optional bundled image target
# (only available if installed with TMF8829_INCLUDE_DEFAULT_IMAGE=ON):
# target_link_libraries(your_firmware PRIVATE tmf8829::default_image)
```

## CMake options

| Option | Default | Purpose |
|---|---|---|
| `TMF8829_BUILD_TESTS` | `OFF` (auto-`ON` standalone) | Build Catch2 unit tests. Pulls Catch2 v3 via `FetchContent`. **Disable on embedded targets.** |
| `TMF8829_INCLUDE_DEFAULT_IMAGE` | `OFF` | Build the bundled vendor firmware image as `tmf8829::default_image`. |

## Usage sketch

```c
#include <tmf8829/tmf8829.h>

static const tmf8829_ops_t my_stm32_ops = {
    .read              = my_stm32_read,
    .write             = my_stm32_write,
    .delay_us          = my_stm32_delay_us,
    .systick_us        = my_stm32_systick_us,
    .write_pin_enable  = my_stm32_write_pin_enable,
    .read_pin_int      = my_stm32_read_pin_int,
};

static uint8_t buffer_a[512];
static tmf8829_driver_t sensor_a = {
    .bus         = TMF8829_BUS_I2C,
    .i2c_addr    = 0x41,
    .user_ctx    = &my_i2c_handle_for_sensor_a,
    .buffer      = buffer_a,
    .buffer_len  = sizeof(buffer_a),
};

int rc = tmf8829_init(&sensor_a, &my_stm32_ops);
```

A second sensor on **SPI** in the same firmware is just a second driver instance with `bus = TMF8829_BUS_SPI`, no recompilation, no `#ifdef` flags.

## Current scope

- Public C API in `include/tmf8829/` for init, power/enable, command/config, bootloader firmware download, and result/histogram reads.
- Optional default firmware image packaged as `tmf8829::default_image` (`image/tmf8829_fw_image.*` + `image/tmf8829_fw_source.*`).
- Catch2 unit tests under `tests/` with fakes for bus, time, and pin behavior.
- CI workflow in `.github/workflows/ci.yml` building and testing matrix targets.

## License & attribution

This project is released under the **MIT License** (see [LICENSE](LICENSE)).

It is a clean reimplementation, the protocol details (register addresses, command opcodes, bootloader sequence, frame layout) were learned from the ams-OSRAM TMF8829 Arduino and Linux reference drivers, both of which are distributed under the MIT license.
The `LICENSE` file contains the acknowledgment.

The optional default firmware image under `image/` is derived from the ams-OSRAM Arduino `tmf8829_image.c` (MIT) and redistributed under the same terms.

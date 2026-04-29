cmake_minimum_required(VERSION 3.13)

project(tmf8829_universal_driver
  LANGUAGES C
  VERSION 0.1.0
  DESCRIPTION "Portable, multi-instance C11 driver for the ams-OSRAM TMF8829"
)

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------

# Default OFF, because Catch2 cannot be built on most embedded toolchains.
# Auto-enabled below when this CMakeLists is the top-level project.
option(TMF8829_BUILD_TESTS
  "Build TMF8829 universal driver Catch2 tests"
  OFF
)

# Opt-in compilation of the bundled vendor application image. OFF by default
# because most projects either supply their own image or pull it from
# external storage.
option(TMF8829_INCLUDE_DEFAULT_IMAGE
  "Build the bundled ams-OSRAM TMF8829 firmware image as tmf8829::default_image"
  OFF
)

# Auto-enable tests when this project is top-level (i.e. configured directly,
# not via add_subdirectory). Mirrors the convention used by the sibling
# edu_sensorring_transport library.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(TMF8829_BUILD_TESTS ON CACHE BOOL "" FORCE)
endif()

# ----------------------------------------------------------------------------
# Core library
# ----------------------------------------------------------------------------

set(tmf8829_sources
  src/tmf8829.c
)

add_library(tmf8829_universal STATIC ${tmf8829_sources})

# Friendly alias used by consumers via add_subdirectory().
add_library(tmf8829::tmf8829 ALIAS tmf8829_universal)

target_include_directories(tmf8829_universal PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Private include for src/tmf8829_internal.h
target_include_directories(tmf8829_universal PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(tmf8829_universal PUBLIC c_std_11)

# Encourage warnings without forcing -Werror on consumers.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(tmf8829_universal PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion
  )
endif()

# ----------------------------------------------------------------------------
# Optional: bundled firmware image
# ----------------------------------------------------------------------------

if(TMF8829_INCLUDE_DEFAULT_IMAGE)
  add_subdirectory(image)
endif()

# ----------------------------------------------------------------------------
# Optional: Catch2 tests
# ----------------------------------------------------------------------------

if(TMF8829_BUILD_TESTS)
  # Catch2 tests are written in C++; enable the language only when needed so
  # cross-compiled embedded firmware builds (which always set the option to
  # OFF) don't have to provide a C++ toolchain.
  enable_language(CXX)
  enable_testing()
  add_subdirectory(tests)
endif()

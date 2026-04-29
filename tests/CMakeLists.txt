# Tests target.
#
# Only entered from the parent CMakeLists when TMF8829_BUILD_TESTS=ON.
# Catch2 cannot be built on most embedded toolchains; do not enable this
# option for cross-compiled firmware builds.

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
  FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(Catch2)

# Catch2's CMake helpers (catch_discover_tests) live in the Catch2
# extras/ directory installed onto the module path automatically.
include(CTest)
include(Catch)

set(tmf8829_test_sources
  test_smoke.cpp
  test_enable.cpp
  test_interrupts.cpp
  test_utilities.cpp
  support/fake_clock.cpp
)

if(TARGET tmf8829::default_image)
  list(APPEND tmf8829_test_sources test_default_image.cpp)
endif()

add_executable(tmf8829_universal_tests ${tmf8829_test_sources})

target_link_libraries(tmf8829_universal_tests PRIVATE
  tmf8829::tmf8829
  Catch2::Catch2WithMain
)

if(TARGET tmf8829::default_image)
  target_link_libraries(tmf8829_universal_tests PRIVATE tmf8829::default_image)
endif()

# Tests #include "support/foo.hpp" relative to the tests/ root.
target_include_directories(tmf8829_universal_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tmf8829_universal_tests PRIVATE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(tmf8829_universal_tests PRIVATE
    -Wall -Wextra -Wpedantic
  )
endif()

catch_discover_tests(tmf8829_universal_tests
  PROPERTIES LABELS "tmf8829"
)

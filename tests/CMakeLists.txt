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
)

add_executable(tmf8829_universal_tests ${tmf8829_test_sources})

target_link_libraries(tmf8829_universal_tests PRIVATE
  tmf8829::tmf8829
  Catch2::Catch2WithMain
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

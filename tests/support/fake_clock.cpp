// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
//
// Definition of the FakeClock::active thread_local pointer. Lives in its
// own translation unit so it has exactly one definition across all test
// binaries.

#include "fake_clock.hpp"

namespace tmf8829_test {

thread_local FakeClock* FakeClock::active = nullptr;

} // namespace tmf8829_test

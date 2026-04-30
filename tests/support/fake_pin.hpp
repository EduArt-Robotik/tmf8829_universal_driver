// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
//
// FakePin: records every level the driver writes to the enable pin and
// optionally returns a scripted value for read_pin_int.

#pragma once

#include <cstdint>
#include <vector>

extern "C" {
#include "tmf8829/tmf8829.h"
}

namespace tmf8829_test {

class FakePin {
public:
  /// Sequence of levels written via tmf8829_ops_t::write_pin_enable.
  std::vector<int> enable_history;

  /// Returned by read_pin_int. Tests may write directly.
  int int_level = 0;

  void write_pin_enable(int high) { enable_history.push_back(high ? 1 : 0); }
  int read_pin_int() { return int_level; }
};

} // namespace tmf8829_test

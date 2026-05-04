// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
// Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
//
// In-memory FakeBus: a 256-byte register file that backs every read and
// records every read/write the driver performs. Bus-error injection is
// supported through `read_error_after_n` / `write_error_after_n`.
//
// The fake is bus-agnostic: it has no notion of I2C vs SPI framing. The
// universal driver does not prepend SPI prefix bytes — that is the host
// port file's job — so a single FakeBus stands in for both bus paths in
// our tests.

#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

extern "C" {
#include "tmf8829/tmf8829.h"
}

namespace tmf8829_test {

class FakeBus {
public:
  enum class OpKind {
    Read,
    Write
  };

  struct Op {
    OpKind kind;
    std::uint8_t reg;
    std::vector<std::uint8_t> data;
  };

  /// Register file backing reads. Tests preload values here.
  std::array<std::uint8_t, 256> reg_file{};

  /// Ordered log of every transfer the driver performed.
  std::vector<Op> ops;

  /// If >= 0, the next read returns -1 and decrements the counter; the
  /// counter going below zero stops further injection. Use 0 to fail the
  /// very next read.
  int read_error_after_n  = -1;
  int write_error_after_n = -1;

  /// Auto-response map: after any write to reg R, set reg_file[R] = value.
  /// Models HW registers that acknowledge commands (e.g. CMD_STAT → READY).
  std::unordered_map<std::uint8_t, std::uint8_t> write_auto_response;

  /// Non-incrementing registers: reads/writes to these addresses do not
  /// advance through the register file. Models HW FIFOs.
  std::unordered_map<std::uint8_t, bool> non_incrementing;

  /// FIFO read queue: when a non-incrementing register is read, bytes are
  /// consumed from this queue in order. Falls back to reg_file if empty.
  std::unordered_map<std::uint8_t, std::vector<std::uint8_t> > fifo_read_queue;

  /// Push data into a FIFO read queue for a non-incrementing register.
  void fifo_push(std::uint8_t reg, std::initializer_list<std::uint8_t> bytes) {
    auto& q = fifo_read_queue[reg];
    q.insert(q.end(), bytes.begin(), bytes.end());
  }
  void fifo_push(std::uint8_t reg, const std::uint8_t* data, std::size_t len) {
    auto& q = fifo_read_queue[reg];
    q.insert(q.end(), data, data + len);
  }

  void preload(std::uint8_t reg, std::uint8_t value) { reg_file[reg] = value; }
  void preload(std::uint8_t reg, std::initializer_list<std::uint8_t> bytes) {
    std::uint16_t r = reg;
    for (std::uint8_t b : bytes) {
      reg_file[r++ & 0xFFu] = b;
    }
  }

  int do_read(std::uint8_t reg, std::uint8_t* buf, std::uint16_t len) {
    // Snapshot the data we are about to deliver so the op log is faithful.
    std::vector<std::uint8_t> snapshot(len);
    std::uint8_t addr = reg;
    for (std::uint16_t i = 0; i < len; ++i) {
      if (non_incrementing.count(addr) > 0) {
        // Consume from FIFO queue if available, else return reg_file value.
        auto it = fifo_read_queue.find(addr);
        if (it != fifo_read_queue.end() && !it->second.empty()) {
          snapshot[i] = it->second.front();
          it->second.erase(it->second.begin());
        } else {
          snapshot[i] = reg_file[addr];
        }
      } else {
        snapshot[i] = reg_file[addr];
        addr        = static_cast<std::uint8_t>((addr + 1u) & 0xFFu);
      }
    }

    ops.push_back({ OpKind::Read, reg, snapshot });

    if (read_error_after_n == 0) {
      read_error_after_n = -1;
      return -1;
    }
    if (read_error_after_n > 0) {
      --read_error_after_n;
    }

    for (std::uint16_t i = 0; i < len; ++i) {
      buf[i] = snapshot[i];
    }
    return 0;
  }

  int do_write(std::uint8_t reg, const std::uint8_t* buf, std::uint16_t len) {
    ops.push_back({ OpKind::Write, reg, std::vector<std::uint8_t>(buf, buf + len) });

    if (write_error_after_n == 0) {
      write_error_after_n = -1;
      return -1;
    }
    if (write_error_after_n > 0) {
      --write_error_after_n;
    }

    bool fifo_start    = non_incrementing.count(reg) > 0;
    std::uint8_t waddr = reg;
    for (std::uint16_t i = 0; i < len; ++i) {
      reg_file[waddr] = buf[i];
      // Only advance if start addr is not FIFO and current addr is not FIFO.
      if (!fifo_start && non_incrementing.count(waddr) == 0) {
        waddr = static_cast<std::uint8_t>((waddr + 1u) & 0xFFu);
      }
    }

    // Apply auto-response: simulate HW acknowledging command.
    auto it = write_auto_response.find(reg);
    if (it != write_auto_response.end()) {
      reg_file[reg] = it->second;
    }

    return 0;
  }
};

} // namespace tmf8829_test

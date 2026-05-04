// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
// Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
//
// Tests for tmf8829_start_measurement, tmf8829_stop_measurement,
// tmf8829_read_results, and tmf8829_read_histogram.

#include <array>
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

#include "support/test_fixture.hpp"

using tmf8829_test::FakeBus;
using tmf8829_test::Fixture;

namespace {

// Build a minimal valid result frame using the FIFO queue model.
// The driver reads 21 bytes from FIFO_STATUS (0xFA):
//   - 5 bytes pre-header from registers 0xFA-0xFE (incrementing)
//   - 16 bytes frame header from FIFO port at 0xFF (non-incrementing queue)
// Then it reads payload data from the FIFO port (0xFF) in subsequent reads.
void preload_result_frame(Fixture& f, std::uint16_t payload_len, std::uint8_t fid = TMF8829_FID_RESULTS) {
  // Mark FIFO as non-incrementing.
  f.bus.non_incrementing[TMF8829_REG_FIFO] = true;

  // Pre-header in normal registers: FIFO_STATUS + 4 bytes sys_tick.
  f.bus.preload(TMF8829_REG_FIFO_STATUS, static_cast<std::uint8_t>(0x01u));                                // FIFO not empty
  f.bus.preload(static_cast<std::uint8_t>(TMF8829_REG_FIFO_STATUS + 1u), static_cast<std::uint8_t>(0x10)); // tick LSB
  f.bus.preload(static_cast<std::uint8_t>(TMF8829_REG_FIFO_STATUS + 2u), static_cast<std::uint8_t>(0x00));
  f.bus.preload(static_cast<std::uint8_t>(TMF8829_REG_FIFO_STATUS + 3u), static_cast<std::uint8_t>(0x00));
  f.bus.preload(static_cast<std::uint8_t>(TMF8829_REG_FIFO_STATUS + 4u), static_cast<std::uint8_t>(0x00));

  // Frame header (16 bytes) pushed into FIFO queue at 0xFF.
  // The driver reads these as part of the initial 21-byte read (bytes 5-20).
  // Layout: [FID|FPM][layout][payload_len at offset 2-3]...
  // buffer[7] = pre_header[7] is actually FIFO byte index 2 (7 - 5 = 2).
  std::array<std::uint8_t, TMF8829_FRAME_HEADER_SIZE> hdr{};
  hdr[0] = static_cast<std::uint8_t>(fid | 0x01u); // FID | FPM
  hdr[1] = 0x01u;                                  // layout: 1 peak
  // payload_len goes at buffer[7], which is FIFO byte 2 (index 7 - 5 = 2).
  hdr[2] = static_cast<std::uint8_t>(payload_len & 0xFFu);
  hdr[3] = static_cast<std::uint8_t>((payload_len >> 8) & 0xFFu);
  f.bus.fifo_push(TMF8829_REG_FIFO, hdr.data(), hdr.size());
}

// Callback tracker
struct ResultCapture {
  std::vector<std::vector<std::uint8_t> > chunks;
  static ResultCapture* active;
  static void callback(tmf8829_driver_t*, const std::uint8_t* data, std::uint16_t len) { active->chunks.emplace_back(data, data + len); }
};
ResultCapture* ResultCapture::active = nullptr;

} // namespace

TEST_CASE("start_measurement rejects uninitialised driver", "[tmf8829][measurement]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_start_measurement(&drv) == TMF8829_E_PARAM);
}

TEST_CASE("start_measurement: happy path non-cyclic", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // Set period to 0 (non-cyclic / single-shot).
  f.drv.config[0] = 0u;
  f.drv.config[1] = 0u;

  // CMD_STAT must transition to ACCEPTED (0x01).
  f.bus.preload(TMF8829_REG_CMD_STAT, TMF8829_STAT_ACCEPTED);
  f.bus.write_auto_response[TMF8829_REG_CMD_STAT] = TMF8829_STAT_ACCEPTED;

  REQUIRE(tmf8829_start_measurement(&f.drv) == TMF8829_OK);
  REQUIRE(f.drv._cyclic_running == 0u);
}

TEST_CASE("start_measurement: happy path cyclic", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // Set period to 100 ms (cyclic).
  f.drv.config[0] = 100u;
  f.drv.config[1] = 0u;

  f.bus.preload(TMF8829_REG_CMD_STAT, TMF8829_STAT_ACCEPTED);
  f.bus.write_auto_response[TMF8829_REG_CMD_STAT] = TMF8829_STAT_ACCEPTED;

  REQUIRE(tmf8829_start_measurement(&f.drv) == TMF8829_OK);
  REQUIRE(f.drv._cyclic_running == 1u);
}

TEST_CASE("stop_measurement rejects uninitialised driver", "[tmf8829][measurement]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_stop_measurement(&drv) == TMF8829_E_PARAM);
}

TEST_CASE("stop_measurement: happy path", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // Device already awake.
  f.bus.preload(TMF8829_REG_ENABLE, TMF8829_ENABLE_CPU_READY_MASK);
  // CMD_STAT returns OK after stop command.
  f.bus.preload(TMF8829_REG_CMD_STAT, TMF8829_STAT_OK);
  f.bus.write_auto_response[TMF8829_REG_CMD_STAT] = TMF8829_STAT_OK;

  REQUIRE(tmf8829_stop_measurement(&f.drv) == TMF8829_OK);
  REQUIRE(f.drv._cyclic_running == 0u);
}

TEST_CASE("read_results rejects uninitialised driver", "[tmf8829][measurement]") {
  tmf8829_driver_t drv{};
  REQUIRE(tmf8829_read_results(&drv) == TMF8829_E_PARAM);
}

TEST_CASE("read_results: wrong frame ID returns E_NO_RESULT", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // Preload a histogram frame ID instead of results.
  preload_result_frame(f, 20, TMF8829_FID_HISTOGRAMS);

  REQUIRE(tmf8829_read_results(&f.drv) == TMF8829_E_NO_RESULT);
}

TEST_CASE("read_results: bus failure during header read", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);
  f.bus.read_error_after_n = 0;

  REQUIRE(tmf8829_read_results(&f.drv) == TMF8829_E_BUS);
}

TEST_CASE("read_results: bus failure during FIFO payload read", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // Payload just big enough: header overhead + some data + EOF.
  // TMF8829_FRAME_HEADER_SIZE - TMF8829_FRAME_HEADER_OFFSET = 12
  // So payload_len = 12 + actual_data.
  // Let's use payload_len = 14 so size_to_read = 2 (just the EOF).
  preload_result_frame(f, static_cast<std::uint16_t>(TMF8829_FRAME_HEADER_SIZE - TMF8829_FRAME_HEADER_OFFSET + 4u));

  // Let the header read succeed, but fail on the FIFO read.
  f.bus.read_error_after_n = 1;

  REQUIRE(tmf8829_read_results(&f.drv) == TMF8829_E_BUS);
}

TEST_CASE("read_results: valid result frame with correct EOF invokes callback", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  // We need enough payload for the EOF marker to appear.
  // payload_len - (16 - 4) = size_to_read => payload_len - 12 = size_to_read
  // EOF is last 2 bytes. Let's make size_to_read = 2 (just the EOF).
  const std::uint16_t payload_len = static_cast<std::uint16_t>(TMF8829_FRAME_HEADER_SIZE - TMF8829_FRAME_HEADER_OFFSET + 2u);
  preload_result_frame(f, payload_len);

  // Push EOF payload into FIFO queue: 0xE0F7 in LE = {0xF7, 0xE0}.
  f.bus.fifo_push(TMF8829_REG_FIFO, { 0xF7, 0xE0 });

  // Track callbacks.
  ResultCapture capture;
  ResultCapture::active = &capture;
  f.drv.on_result       = ResultCapture::callback;

  REQUIRE(tmf8829_read_results(&f.drv) == TMF8829_OK);
  REQUIRE(capture.chunks.size() == 1u);
  REQUIRE(capture.chunks[0].size() == 2u);
}

TEST_CASE("read_results: bad EOF returns E_NO_RESULT", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  const std::uint16_t payload_len = static_cast<std::uint16_t>(TMF8829_FRAME_HEADER_SIZE - TMF8829_FRAME_HEADER_OFFSET + 2u);
  preload_result_frame(f, payload_len);

  // Wrong EOF marker.
  f.bus.fifo_push(TMF8829_REG_FIFO, { 0x00, 0x00 });

  REQUIRE(tmf8829_read_results(&f.drv) == TMF8829_E_NO_RESULT);
}

TEST_CASE("read_histogram: wrong frame ID returns E_NO_RESULT", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  preload_result_frame(f, 20, TMF8829_FID_RESULTS);

  REQUIRE(tmf8829_read_histogram(&f.drv) == TMF8829_E_NO_RESULT);
}

TEST_CASE("read_histogram: valid frame with correct EOF", "[tmf8829][measurement]") {
  Fixture f;
  REQUIRE(f.init() == TMF8829_OK);

  const std::uint16_t payload_len = static_cast<std::uint16_t>(TMF8829_FRAME_HEADER_SIZE - TMF8829_FRAME_HEADER_OFFSET + 2u);
  preload_result_frame(f, payload_len, TMF8829_FID_HISTOGRAMS);
  f.bus.fifo_push(TMF8829_REG_FIFO, { 0xF7, 0xE0 });

  REQUIRE(tmf8829_read_histogram(&f.drv) == TMF8829_OK);
}

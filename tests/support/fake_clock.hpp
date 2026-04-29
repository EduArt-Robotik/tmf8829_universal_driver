// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 tmf8829_universal_driver contributors
//
// FakeClock: records every delay_us call and advances a virtual systick.
//
// Because tmf8829_ops_t::delay_us / systick_us do not take a tmf8829_driver_t
// (they are platform-global services), the C-callable thunks recover their
// FakeClock target from a thread_local pointer. Tests must construct the
// fixture before issuing driver calls and tear it down afterwards.

#pragma once

#include <cstdint>
#include <vector>

namespace tmf8829_test {

class FakeClock
{
public:
    /// Pointer to the FakeClock the C thunks dispatch to. Set/cleared by Fixture.
    static thread_local FakeClock *active;

    std::vector<std::uint32_t> delays;
    std::uint32_t              now_us = 0u;

    void delay_us(std::uint32_t us)
    {
        delays.push_back(us);
        now_us += us;
    }

    std::uint32_t systick_us() const { return now_us; }

    // C-callable thunks to bind into tmf8829_ops_t.
    static void c_delay(std::uint32_t us)
    {
        if (active != nullptr) {
            active->delay_us(us);
        }
    }
    static std::uint32_t c_systick(void)
    {
        return active != nullptr ? active->systick_us() : 0u;
    }
};

} // namespace tmf8829_test

// CLI completion barrier for all asynchronous native DSP workers.
#pragma once
#include "viper-control.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace viperbass::cli_worker_ack {
struct Pair {
    const char* name;
    const std::atomic<std::uint32_t>* requested;
    const std::atomic<std::uint32_t>* applied;
};
inline std::array<Pair, 11> workers(const Controls& c) {
    return {{{"Graphic EQ", &c.eq_request_serial, &c.eq_applied_serial},
             {"Tube", &c.tube_request_serial, &c.tube_applied_serial},
             {"AnalogX", &c.analogx_request_serial, &c.analogx_applied_serial},
             {"Crossfeed", &c.crossfeed_request_serial, &c.crossfeed_applied_serial},
             {"FET", &c.fet_request_serial, &c.fet_applied_serial},
             {"Multiband", &c.mb_request_serial, &c.mb_applied_serial},
             {"DDC", &c.ddc_request_serial, &c.ddc_applied_serial},
             {"Speaker Correction", &c.speaker_request_serial, &c.speaker_applied_serial},
             {"Stereo Imager", &c.imager_request_serial, &c.imager_applied_serial},
             {"Dynamic EQ", &c.deq_request_serial, &c.deq_applied_serial},
             {"Convolver", &c.convolver_request_serial, &c.convolver_applied_serial}}};
}
using Snapshot = std::array<std::uint32_t, 11>;
inline Snapshot snapshot(const Controls& c) {
    Snapshot values{};
    const auto items = workers(c);
    for (std::size_t i = 0; i < items.size(); ++i)
        values[i] = items[i].requested->load(std::memory_order_acquire);
    return values;
}
// One shared deadline for every worker modified by this CLI invocation.
// Read-only requests never block on unrelated pending asynchronous work.
inline bool waitForCompletion(const Controls& c, const Snapshot& before, bool all,
                              std::chrono::steady_clock::duration timeout,
                              std::string& pending) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    const auto items = workers(c);
    while (true) {
        pending.clear();
        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto request = items[i].requested->load(std::memory_order_acquire);
            if (!all && request == before[i]) continue;
            if (items[i].applied->load(std::memory_order_acquire) != request) {
                if (!pending.empty()) pending += ", ";
                pending += items[i].name;
            }
        }
        if (pending.empty()) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
} // namespace viperbass::cli_worker_ack

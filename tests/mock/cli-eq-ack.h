// Isolated CLI tests emulate all native DSP worker acknowledgments.
// This thread never runs in the production CLI and never applies DSP effects.
#pragma once
#include "viper-control.h"
#include <atomic>
#include <chrono>
#include <thread>

class MockCliEqAcknowledger {
    viperbass::Controls& controls_;
    std::atomic<bool> stopping_{false};
    std::thread worker_;
public:
    explicit MockCliEqAcknowledger(viperbass::Controls& controls)
        : controls_(controls), worker_([this] {
            while (!stopping_.load(std::memory_order_acquire)) {
                const auto acknowledge = [](std::atomic<std::uint32_t>& applied,
                                            const std::atomic<std::uint32_t>& requested) {
                    applied.store(requested.load(std::memory_order_acquire),
                                  std::memory_order_release);
                };
                acknowledge(controls_.eq_applied_serial, controls_.eq_request_serial);
                acknowledge(controls_.tube_applied_serial, controls_.tube_request_serial);
                acknowledge(controls_.analogx_applied_serial, controls_.analogx_request_serial);
                acknowledge(controls_.crossfeed_applied_serial, controls_.crossfeed_request_serial);
                acknowledge(controls_.fet_applied_serial, controls_.fet_request_serial);
                acknowledge(controls_.mb_applied_serial, controls_.mb_request_serial);
                acknowledge(controls_.ddc_applied_serial, controls_.ddc_request_serial);
                acknowledge(controls_.speaker_applied_serial, controls_.speaker_request_serial);
                acknowledge(controls_.imager_applied_serial, controls_.imager_request_serial);
                acknowledge(controls_.deq_applied_serial, controls_.deq_request_serial);
                acknowledge(controls_.convolver_applied_serial, controls_.convolver_request_serial);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }) {}
    MockCliEqAcknowledger(const MockCliEqAcknowledger&) = delete;
    MockCliEqAcknowledger& operator=(const MockCliEqAcknowledger&) = delete;
    void stop() {
        stopping_.store(true, std::memory_order_release);
        if (worker_.joinable()) worker_.join();
    }
    ~MockCliEqAcknowledger() { stop(); }
};

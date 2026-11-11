#pragma once
#include "LStfDetector.h"
#include "IqQueue.h"
#include <memory>
#include <thread>
#include <string>

// Detection only. OFDM FFT/CFO compensation and decoding belong to a later stage.
class WiFiDemodulator {
public:
    struct Snapshot {
        bool enabled = false;
        uint64_t candidates = 0, resets = 0, processed_samples = 0;
        IqQueue::Stats queue;
        std::vector<LStfEvent> recent;
        std::string error;
    };
    explicit WiFiDemodulator(size_t queue_capacity = 32);
    ~WiFiDemodulator();
    void enable(bool enabled, double actual_rate_hz);
    void submit(std::span<const std::complex<int16_t>> samples, uint64_t first, uint64_t segment, bool gap);
    Snapshot snapshot() const;
private:
    void run();
    IqQueue queue_;
    mutable std::mutex mutex_;
    Snapshot state_;
    uint64_t generation_ = 0;
    std::jthread worker_;
};

#pragma once
#include "LStfDetector.h"
#include "IqQueue.h"
#include <memory>
#include <thread>
#include <string>

// L-STF detection and legacy L-LTF synchronization; no OFDM payload decoding.
struct LltfSyncEvent {
    uint64_t segment = 0;
    uint64_t candidate_index = 0;
    uint64_t ltf_first_index = 0; // first 64-sample useful LTF, strongest matched path
    uint64_t packet_start_index = 0; // inferred nominal L-STF origin = ltf_first_index - 192
    double correlation = 0;
    double coarse_cfo_hz = 0;
    double fine_cfo_hz = 0; // residual after coarse CFO correction
    double total_cfo_hz = 0;
};
class WiFiDemodulator {
public:
    struct Snapshot {
        bool enabled = false;
        uint64_t candidates = 0, resets = 0, processed_samples = 0;
        uint64_t synchronized_packets = 0, rejected_candidates = 0;
        IqQueue::Stats queue;
        std::vector<LStfEvent> recent;
        std::vector<LltfSyncEvent> recent_sync;
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

#include "WiFiDemodulator.h"
#include <cmath>
#include <stdexcept>

WiFiDemodulator::WiFiDemodulator(size_t capacity) : queue_(capacity), worker_([this]{run();}) {}
WiFiDemodulator::~WiFiDemodulator() { queue_.close(); worker_.join(); }
void WiFiDemodulator::enable(bool enabled, double rate) {
    std::lock_guard lock(mutex_);
    state_.enabled = enabled && std::isfinite(rate) && std::abs(rate-20e6)<=20;
    ++generation_; state_.recent.clear(); state_.error.clear();
}
void WiFiDemodulator::submit(std::span<const std::complex<int16_t>> samples, uint64_t first, uint64_t segment, bool gap) {
    std::lock_guard lock(mutex_);
    if (state_.enabled) queue_.push(samples,first,segment,gap,generation_);
}
WiFiDemodulator::Snapshot WiFiDemodulator::snapshot() const {
    std::lock_guard lock(mutex_);
    auto result=state_; result.queue=queue_.stats(); return result;
}
void WiFiDemodulator::run() {
    LStfDetector detector;
    uint64_t generation=~uint64_t{0}, segment=~uint64_t{0}, next=0;
    IqBlock block;
    while (queue_.pop(block)) {
        uint64_t current;
        {
            std::lock_guard lock(mutex_);
            if (!state_.enabled || block.token!=generation_) continue;
            current=generation_;
            if (current!=generation || segment!=block.segment || next!=block.first || block.discontinuity) {
                detector.reset(block.first); ++state_.resets;
            }
        }
        try {
            auto events=detector.process(block.samples);
            std::lock_guard lock(mutex_);
            if (current==generation_ && state_.enabled) {
                state_.processed_samples += block.samples.size();
                for (auto event:events) {
                    event.segment=block.segment; ++state_.candidates;
                    state_.recent.push_back(event);
                    if (state_.recent.size()>8) state_.recent.erase(state_.recent.begin());
                }
            }
        } catch (const std::exception& e) {
            std::lock_guard lock(mutex_); state_.error=e.what(); state_.enabled=false;
        }
        generation=current; segment=block.segment; next=block.first+block.samples.size();
    }
}

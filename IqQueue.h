#pragma once
#include <cstddef>
#include <complex>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <stdexcept>
#include <vector>

struct IqBlock {
    std::vector<std::complex<int16_t>> samples;
    uint64_t first = 0, segment = 0, token = 0;
    bool discontinuity = false;
};
// Single producer, single consumer. Drop newest, never wait for free space.
// The next accepted block carries the gap marker, even if no more data arrives yet.
class IqQueue {
public:
    struct Stats { uint64_t dropped_samples = 0, overflows = 0; size_t queued = 0; };
    explicit IqQueue(size_t capacity = 32) : capacity_(capacity) {
        if (!capacity) throw std::invalid_argument("zero queue capacity");
    }
    bool push(std::span<const std::complex<int16_t>> samples, uint64_t first, uint64_t segment, bool gap = false, uint64_t token = 0) {
        std::lock_guard lock(mutex_);
        if (closed_) return false;
        if (queue_.size() >= capacity_) {
            dropped_ += samples.size(); ++overflows_; gap_ = true; return false;
        }
        queue_.push_back({{samples.begin(),samples.end()},first,segment,token,gap || gap_});
        gap_ = false; cv_.notify_one(); return true;
    }
    bool pop(IqBlock& block) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock,[&]{return closed_ || !queue_.empty();});
        if (queue_.empty()) return false;
        block = std::move(queue_.front()); queue_.pop_front(); return true;
    }
    void close() { std::lock_guard lock(mutex_); closed_ = true; cv_.notify_all(); }
    Stats stats() const { std::lock_guard lock(mutex_); return {dropped_,overflows_,queue_.size()}; }
private:
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<IqBlock> queue_;
    bool closed_ = false, gap_ = false;
    uint64_t dropped_ = 0, overflows_ = 0;
};

#pragma once
#include <cstddef>
#include <complex>
#include <cstdint>
#include <span>
#include <vector>
#include <algorithm>
#include <stdexcept>
// Accumulate successful short receives without inventing gaps or padding FFT input.
class IqAccumulator {
public:
    explicit IqAccumulator(size_t size) : size_(size) {
        if (!size) throw std::invalid_argument("zero FFT block size");
        pending_.reserve(size);
    }
    void reset() { pending_.clear(); }
    template<class Consumer> void process(std::span<const std::complex<int16_t>> samples, Consumer consume) {
        while (!samples.empty()) {
            const size_t count=std::min(size_-pending_.size(),samples.size());
            pending_.insert(pending_.end(),samples.begin(),samples.begin()+count);
            samples=samples.subspan(count);
            if (pending_.size()==size_) { consume(std::span<const std::complex<int16_t>>(pending_));pending_.clear(); }
        }
    }
private:
    size_t size_;
    std::vector<std::complex<int16_t>> pending_;
};

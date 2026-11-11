#include "LStfDetector.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

LStfDetector::LStfDetector(LStfConfig config) : config_(config) {
    if (!std::isfinite(config.sample_rate_hz) || std::abs(config.sample_rate_hz - 20e6) > 20 ||
        !(config.correlation_threshold > 0 && config.correlation_threshold <= 1) ||
        !std::isfinite(config.power_threshold) || config.power_threshold < 0 ||
        !config.plateau_samples || !config.release_samples)
        throw std::invalid_argument("L-STF requires 20 MS/s, valid thresholds and nonzero plateau/release");
}
void LStfDetector::reset(uint64_t next_sample) {
    *this = LStfDetector(config_);
    index_ = next_sample;
}
std::vector<LStfEvent> LStfDetector::process(std::span<const std::complex<int16_t>> samples) {
    std::vector<LStfEvent> events;
    for (auto raw : samples) {
        const std::complex<double> x(raw.real()/32768.0, raw.imag()/32768.0);
        const size_t h = received_ % lag;
        const auto delayed = history_[h];
        history_[h] = x;
        if (received_ >= lag) {
            const size_t p = (received_ - lag) % window;
            correlation_ -= products_[p]; energy0_ -= energies0_[p]; energy1_ -= energies1_[p];
            // Positive CFO => positive correlation phase.
            products_[p] = std::conj(delayed)*x;
            energies0_[p] = std::norm(delayed); energies1_[p] = std::norm(x);
            correlation_ += products_[p]; energy0_ += energies0_[p]; energy1_ += energies1_[p];
            if (received_ >= lag + window - 1) {
                const double power = std::max(0.0, (energy0_ + energy1_) / (2*window));
                const double denom = energy0_*energy1_;
                const double score = denom > 1e-24 ? std::clamp(std::norm(correlation_)/denom,0.0,1.0) : 0;
                const bool qualifies = score >= config_.correlation_threshold && power >= config_.power_threshold;
                if (qualifies) {
                    release_ = 0;
                    if (!latched_) {
                        if (plateau_++ == 0) candidate_.sample_index = index_ - (lag + window - 1);
                        if (plateau_ >= config_.plateau_samples) {
                            candidate_.correlation = score;
                            candidate_.power = power;
                            candidate_.cfo_hz = std::arg(correlation_)*config_.sample_rate_hz/(2*std::numbers::pi*lag);
                            events.push_back(candidate_); latched_ = true;
                        }
                    }
                } else {
                    plateau_ = 0;
                    if (++release_ >= config_.release_samples) latched_ = false;
                }
            }
        }
        ++received_; ++index_;
    }
    return events;
}

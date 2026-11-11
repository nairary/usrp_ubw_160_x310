#pragma once
#include <cstddef>
#include <complex>
#include <cstdint>
#include <span>
#include <vector>

struct LStfConfig {
    double sample_rate_hz = 20e6;
    double correlation_threshold = 0.75; // |R|^2 / (E0 E1)
    double power_threshold = 1e-6;       // mean normalized complex sample power
    size_t plateau_samples = 32;
    size_t release_samples = 32;
};
enum class DetectionStatus { PlateauCandidate };
struct LStfEvent {
    uint64_t sample_index = 0; // first qualifying correlation-window start; NOT packet start
    double correlation = 0;
    double cfo_hz = 0;
    double power = 0;
    DetectionStatus status = DetectionStatus::PlateauCandidate;
    uint64_t segment = 0;
};
class LStfDetector {
public:
    explicit LStfDetector(LStfConfig config = {});
    void reset(uint64_t next_sample = 0);
    std::vector<LStfEvent> process(std::span<const std::complex<int16_t>> samples);
private:
    static constexpr size_t lag = 16, window = 64;
    LStfConfig config_;
    std::complex<double> history_[lag]{};
    std::complex<double> products_[window]{};
    double energies0_[window]{}, energies1_[window]{};
    std::complex<double> correlation_{};
    double energy0_ = 0, energy1_ = 0;
    uint64_t index_ = 0, received_ = 0;
    size_t plateau_ = 0, release_ = 0;
    bool latched_ = false;
    LStfEvent candidate_;
};

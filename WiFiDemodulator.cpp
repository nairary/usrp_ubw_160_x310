#include "WiFiDemodulator.h"
#include <cmath>
#include <stdexcept>
#include <array>
#include <algorithm>
#include <deque>
#include <numbers>

namespace {
constexpr double sampleRate = 20e6;
constexpr double twoPi = 2*std::numbers::pi;

// Known IEEE legacy L-LTF subcarriers -26..26, DC zero. Computed once per worker.
std::array<std::complex<double>,64> makeLtfReference() {
    constexpr int tones[53] = {
        1,1,-1,-1,1,1,-1,1,-1,1,1,1,1,1,1,-1,-1,1,1,-1,1,-1,1,1,1,1,
        0,1,-1,-1,1,1,-1,1,-1,1,-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,1,-1,1,1,1,1
    };
    std::array<std::complex<double>,64> ref{};
    for (size_t n=0;n<64;++n)
        for (int k=-26;k<=26;++k)
            ref[n] += double(tones[k+26])*std::polar(1.0,twoPi*k*double(n)/64.0)/64.0;
    return ref;
}

// Worker-owned bounded history. Search is delayed until the entire bounded timing
// window is available, so results do not depend on input chunk boundaries.
class LltfSynchronizer {
public:
    void reset(uint64_t first) { history_.clear(); pending_.clear(); first_=first; }
    void append(std::span<const std::complex<int16_t>> samples) {
        for (auto x:samples) history_.emplace_back(x.real()/32768.0,x.imag()/32768.0);
    }
    void candidate(const LStfEvent& event) { pending_.push_back(event); }
    std::vector<LltfSyncEvent> process(uint64_t segment, uint64_t& rejected) {
        std::vector<LltfSyncEvent> result;
        while (!pending_.empty()) {
            const auto event=pending_.front();
            // The STF window crossing is approximate. Search first useful LTF
            // between candidate+128 and candidate+288, not at a fixed +160.
            const uint64_t begin=std::max<uint64_t>(192,event.sample_index+128);
            const uint64_t end=event.sample_index+288;
            if (first_+history_.size()<end+128) break;
            pending_.pop_front();
            double best=0;uint64_t bestIndex=0;
            for (uint64_t m=begin;m<=end;++m) {
                if (m<first_+32) continue;
                const auto a=match(m,event.cfo_hz), b=match(m+64,event.cfo_hz);
                const double repeat=repeatScore(m,m+64,64);
                const double cp=repeatScore(m-32,m+32,32);
                const double score=std::min(a,b);
                if (repeat>=0.75 && cp>=0.65 && score>best) { best=score;bestIndex=m; }
            }
            if (best<0.55) { ++rejected; continue; }
            std::complex<double> r{};
            for (size_t k=0;k<64;++k) r+=std::conj(at(bestIndex+k))*at(bestIndex+64+k);
            // Remove the known coarse phase advance, retaining the residual CFO.
            r*=std::polar(1.0,-twoPi*event.cfo_hz*64/sampleRate);
            const double fine=std::arg(r)*sampleRate/(twoPi*64);
            result.push_back({segment,event.sample_index,bestIndex,bestIndex-192,best,event.cfo_hz,fine,event.cfo_hz+fine});
        }
        // append is called in chunks of at most 256; the maximum pending search
        // horizon is 416 samples from its candidate. 1024 retains ample history.
        while (history_.size()>1024) { history_.pop_front(); ++first_; }
        return result;
    }
private:
    std::complex<double> at(uint64_t index) const { return history_.at(static_cast<size_t>(index-first_)); }
    double match(uint64_t index,double cfo) const {
        std::complex<double> r{};double power=0;
        const auto step=std::polar(1.0,-twoPi*cfo/sampleRate);
        std::complex<double> phase=1;
        for (size_t k=0;k<64;++k) {
            const auto x=at(index+k);r+=x*phase*std::conj(reference_[k]);power+=std::norm(x);phase*=step;
        }
        // Reference energy is 52/64 by Parseval.
        return power>1e-20 ? std::clamp(std::norm(r)/(power*(52.0/64)),0.0,1.0) : 0;
    }
    double repeatScore(uint64_t a,uint64_t b,size_t count) const {
        std::complex<double> r{};double p=0,q=0;
        for (size_t k=0;k<count;++k) { auto x=at(a+k),y=at(b+k);r+=std::conj(x)*y;p+=std::norm(x);q+=std::norm(y); }
        return p*q>1e-24 ? std::clamp(std::norm(r)/(p*q),0.0,1.0) : 0;
    }
    const std::array<std::complex<double>,64> reference_=makeLtfReference();
    std::deque<std::complex<double>> history_;
    std::deque<LStfEvent> pending_;
    uint64_t first_=0;
};
}

WiFiDemodulator::WiFiDemodulator(size_t capacity) : queue_(capacity), worker_([this]{run();}) {}
WiFiDemodulator::~WiFiDemodulator() { queue_.close(); worker_.join(); }
void WiFiDemodulator::enable(bool enabled, double rate) {
    std::lock_guard lock(mutex_);
    state_.enabled = enabled && std::isfinite(rate) && std::abs(rate-20e6)<=20;
    ++generation_; state_.recent.clear(); state_.recent_sync.clear(); state_.error.clear();
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
    LltfSynchronizer synchronizer;
    uint64_t generation=~uint64_t{0}, segment=~uint64_t{0}, next=0;
    IqBlock block;
    while (queue_.pop(block)) {
        uint64_t current;
        {
            std::lock_guard lock(mutex_);
            if (!state_.enabled || block.token!=generation_) continue;
            current=generation_;
            if (current!=generation || segment!=block.segment || next!=block.first || block.discontinuity) {
                detector.reset(block.first); synchronizer.reset(block.first); ++state_.resets;
            }
        }
        try {
            std::vector<LStfEvent> events;
            std::vector<LltfSyncEvent> synced;
            uint64_t rejected=0;
            for (size_t offset=0;offset<block.samples.size();offset+=256) {
                const auto part=std::span(block.samples).subspan(offset,std::min<size_t>(256,block.samples.size()-offset));
                synchronizer.append(part);
                auto found=detector.process(part);
                for (const auto& event:found) synchronizer.candidate(event);
                events.insert(events.end(),found.begin(),found.end());
                auto results=synchronizer.process(block.segment,rejected);
                synced.insert(synced.end(),results.begin(),results.end());
            }
            std::lock_guard lock(mutex_);
            if (current==generation_ && state_.enabled) {
                state_.processed_samples += block.samples.size();
                state_.rejected_candidates += rejected;
                for (auto event:synced) {
                    ++state_.synchronized_packets;
                    state_.recent_sync.push_back(event);
                    if (state_.recent_sync.size()>8) state_.recent_sync.erase(state_.recent_sync.begin());
                }
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

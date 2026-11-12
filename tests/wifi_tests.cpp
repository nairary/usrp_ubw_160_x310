#include "LStfDetector.h"
#include "WiFiDemodulator.h"
#include "Sc16Reader.h"
#include "IqAccumulator.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
using IQ=std::complex<int16_t>;
using Samples=std::vector<IQ>;
void require(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
// Independent frequency-domain reference: IEEE 802.11 legacy short training
// sequence on subcarriers -26..26 (nonzero every fourth tone). Direct 64-point
// inverse DFT; deliberately no detector implementation, FFTW or repeated-tone shortcut.
Samples waveform(double amplitude,double cfo,size_t prefix=300,double snr=100) {
    const int tones[]={-24,-20,-16,-12,-8,-4,4,8,12,16,20,24};
    const int signs[]={1,-1,1,-1,-1,1,-1,-1,1,1,1,1};
    std::vector<std::complex<double>> stf(160);
    for (size_t n=0;n<stf.size();++n)
        for (size_t k=0;k<12;++k) stf[n]+=double(signs[k])*std::complex<double>(1,1)*std::polar(1.0,2*std::numbers::pi*tones[k]*double(n)/64.0);
    double energy=0;for(auto x:stf)energy+=std::norm(x);energy=std::sqrt(energy/stf.size());
    std::mt19937 rng(7251);std::normal_distribution<double> noise(0,amplitude*std::pow(10,-snr/20)/std::sqrt(2.0));
    Samples result(prefix+160+300);
    for(size_t n=0;n<result.size();++n) {
        std::complex<double> x(noise(rng),noise(rng));
        if(n>=prefix && n<prefix+160) x+=stf[n-prefix]*(amplitude/energy)*std::polar(1.0,2*std::numbers::pi*cfo*double(n)/20e6);
        auto q=[](double v){return int16_t(std::clamp(std::lround(v*32768),-32768L,32767L));};
        result[n]={q(x.real()),q(x.imag())};
    }
    return result;
}
std::vector<LStfEvent> feed(const Samples& s,size_t chunk) {
    LStfDetector d;std::vector<LStfEvent> result;
    for(size_t i=0;i<s.size();i+=chunk) {
        auto events=d.process(std::span(s).subspan(i,std::min(chunk,s.size()-i)));
        result.insert(result.end(),events.begin(),events.end());
    }
    return result;
}
// O(N*64) oracle with independent window recomputation and plateau tracking.
uint64_t referencePosition(const Samples& s) {
    size_t plateau=0;uint64_t start=0;
    for(size_t n=0;n+80<=s.size();++n) {
        std::complex<double> r{};double a=0,b=0;
        for(size_t k=0;k<64;++k) {
            std::complex<double> x(s[n+k].real()/32768.0,s[n+k].imag()/32768.0),y(s[n+k+16].real()/32768.0,s[n+k+16].imag()/32768.0);
            r+=std::conj(x)*y;a+=std::norm(x);b+=std::norm(y);
        }
        if(a*b>1e-24 && std::norm(r)/(a*b)>=.75 && (a+b)/128>=1e-6) {
            if(!plateau)start=n;
            if(++plateau==32)return start;
        } else plateau=0;
    }
    throw std::runtime_error("oracle found no plateau");
}
void detectorTests() {
    Samples noise(200000);std::mt19937 rng(31);std::normal_distribution<double> normal(0,3000);
    for(auto& x:noise)x={int16_t(normal(rng)),int16_t(normal(rng))};
    require(feed(noise,137).empty(),"noise false positive");
    require(feed(Samples(1000),1).empty(),"silence false positive");
    for(double amplitude:{.025,.15,.35}) for(double cfo:{-190000.,0.,170000.}) for(double snr:{12.,25.,100.}) {
        const auto s=waveform(amplitude,cfo,300,snr);const auto baseline=feed(s,s.size());
        require(baseline.size()==1,"reference STF not detected exactly once");
        const auto e=baseline.front();require(e.sample_index==referencePosition(s),"candidate position differs from independent oracle");
        require(e.sample_index>=260 && e.sample_index<=320,"candidate far from known burst onset");
        require(std::abs(e.cfo_hz-cfo)<10000,"CFO estimate outside 10 kHz tolerance");
        require(e.correlation>=.75 && e.power>1e-6,"bad event metrics");
        for(size_t chunk:{1,7,16,63,79,80,113,4096}) {
            auto events=feed(s,chunk);require(events.size()==1,"chunked detection count");
            require(events[0].sample_index==e.sample_index && std::abs(events[0].cfo_hz-e.cfo_hz)<1e-9,"chunk-dependent result");
        }
    }
    auto s=waveform(.15,25000);auto second=waveform(.1,-30000);s.insert(s.end(),second.begin(),second.end());
    auto e=feed(s,23);require(e.size()==2,"multiple bursts");
    require(e[1].sample_index==waveform(.15,25000).size()+referencePosition(second),"second candidate offset");
    LStfConfig config;config.power_threshold=.1;require(LStfDetector(config).process(waveform(.01,0)).empty(),"power threshold ignored");
    LStfDetector reset;auto packet=waveform(.15,0,0);reset.process(std::span(packet).first(70));reset.reset(9000);
    require(reset.process(std::span(packet).subspan(70,70)).empty(),"history survives reset");
    reset.reset(9070);auto after=reset.process(packet);require(after.size()==1 && after[0].sample_index>=9070,"reset absolute offset");
}
void accumulatorTests() {
    Samples source(531);for(size_t i=0;i<source.size();++i)source[i]={int16_t(i),int16_t(-int(i))};
    for(size_t chunk:{1,17,64,200}) {
        IqAccumulator acc(64);Samples output;
        for(size_t i=0;i<source.size();i+=chunk)
            acc.process(std::span(source).subspan(i,std::min(chunk,source.size()-i)),[&](auto block){output.insert(output.end(),block.begin(),block.end());});
        require(output.size()==512 && std::equal(output.begin(),output.end(),source.begin()),"partial RX samples lost/reordered");
        acc.reset();size_t blocks=0;acc.process(std::span(source).first(45),[&](auto){++blocks;});require(blocks==0,"FFT crosses discontinuity");
    }
}
void queueTests() {
    IqQueue queue(1);Samples s(50,{100,100});IqBlock b;
    require(queue.push(s,0,7),"first push");require(!queue.push(s,50,7),"overflow should reject");
    require(queue.stats().dropped_samples==50 && queue.stats().overflows==1,"drop accounting");
    require(queue.pop(b) && b.first==0,"queue order");require(queue.push(s,100,7),"post overflow push");
    require(queue.pop(b) && b.discontinuity && b.first==100,"missing gap marker");queue.close();require(!queue.pop(b),"queue shutdown");
    // Exercise detector with the same gap handling as worker: half a preamble on
    // each side of a lost region must never be stitched into a candidate.
    auto packet=waveform(.15,0,0);LStfDetector d;
    d.process(std::span(packet).first(70));d.reset(b.first);
    require(d.process(std::span(packet).subspan(70,70)).empty(),"gap bridges preambles");
    WiFiDemodulator worker(8);worker.enable(true,10e6);require(!worker.snapshot().enabled,"rate gate");
    worker.enable(true,20e6);auto full=waveform(.15,30000);worker.submit(full,0,1,true);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(worker.snapshot().candidates==0 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    require(worker.snapshot().candidates==1,"worker did not detect");
    worker.enable(false,20e6);worker.enable(true,20e6);worker.submit(full,0,2,true);
    while(worker.snapshot().candidates<2 && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    require(worker.snapshot().candidates==2 && worker.snapshot().recent.back().segment==2,"worker reset/lifecycle");
    const auto before=worker.snapshot();
    auto waitProcessed=[&](uint64_t target) {
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(worker.snapshot().processed_samples<target && std::chrono::steady_clock::now()<end)std::this_thread::yield();
        require(worker.snapshot().processed_samples>=target,"worker processing deadline");
    };
    worker.submit(std::span(packet).first(70),0,9,true);
    waitProcessed(before.processed_samples+70);
    worker.submit(std::span(packet).subspan(70,70),140,9,true);
    waitProcessed(before.processed_samples+140);
    require(worker.snapshot().candidates==2 && worker.snapshot().resets>=before.resets+2,"worker bridged discontinuity");
}
void readerTests() {
    auto s=waveform(.12,-120000);std::string bytes;
    for(auto x:s)for(int16_t component:{x.real(),x.imag()}) {
        uint16_t u=static_cast<uint16_t>(component);bytes.push_back(char(u&255));bytes.push_back(char(u>>8));
    }
    const auto expected=feed(s,4096);
    for(size_t block:{1,3,127,4096}) {
        std::istringstream file(bytes);LStfDetector d;std::vector<LStfEvent> events;Samples decoded;
        for(;;) {auto iq=readSc16(file,block);if(iq.empty())break;decoded.insert(decoded.end(),iq.begin(),iq.end());auto e=d.process(iq);events.insert(events.end(),e.begin(),e.end());}
        require(decoded==s,"sc16 signed little-endian decoding");
        require(events.size()==expected.size() && events[0].sample_index==expected[0].sample_index,"offline event mismatch");
    }
    std::istringstream broken(bytes+"x");bool failed=false;try{readSc16(broken,s.size()+1);}catch(const std::runtime_error&){failed=true;}require(failed,"truncated file accepted");
}
// Independent direct-IFFT legacy preamble fixture; no production LTF helper used.
Samples preamble(double cfo, double snr=100, size_t prefix=300) {
    const int ltfTones[]={
        1,1,-1,-1,1,1,-1,1,-1,1,1,1,1,1,1,-1,-1,1,1,-1,1,-1,1,1,1,1,
        0,1,-1,-1,1,1,-1,1,-1,1,-1,-1,-1,-1,-1,1,1,-1,-1,1,-1,1,-1,1,1,1,1};
    std::vector<std::complex<double>> ltf(64);
    for (size_t n=0;n<64;++n) {
        for (int bin=0;bin<64;++bin) {
            const int k=bin<=31?bin:bin-64;
            const double value=(k>=-26 && k<=26)?ltfTones[k+26]:0;
            ltf[n]+=value*std::exp(std::complex<double>(0,2*std::numbers::pi*bin*double(n)/64))/64.0;
        }
    }
    require(std::abs(ltf[0].real()-.15625)<1e-12,"reference LTF first-sample check");
    auto result=waveform(.12,cfo,prefix,snr);
    result.resize(prefix+320+300);
    std::mt19937 rng(123);std::normal_distribution<double> noise(0,.12*std::pow(10,-snr/20)/std::sqrt(2.0));
    for (size_t n=prefix+160;n<result.size();++n) {
        std::complex<double> x(noise(rng),noise(rng));
        if (n<prefix+320) {
            const size_t offset=n-prefix-160;
            const size_t k=offset<32?offset+32:(offset-32)%64;
            x+=ltf[k]*(.12/std::sqrt(52.0/4096))*std::polar(1.0,2*std::numbers::pi*cfo*double(n)/20e6);
        }
        auto q=[](double v){return int16_t(std::clamp(std::lround(v*32768),-32768L,32767L));};
        result[n]={q(x.real()),q(x.imag())};
    }
    return result;
}
WiFiDemodulator::Snapshot processWorker(const Samples& samples,size_t chunk) {
    WiFiDemodulator worker;worker.enable(true,20e6);
    for(size_t offset=0;offset<samples.size();offset+=chunk) {
        const size_t count=std::min(chunk,samples.size()-offset);
        worker.submit(std::span(samples).subspan(offset,count),offset,42,offset==0);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(worker.snapshot().processed_samples<offset+count && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        require(worker.snapshot().processed_samples>=offset+count,"LTF worker deadline");
    }
    return worker.snapshot();
}
void lltfTests() {
    for(double cfo:{-190000.,0.,170000.}) for(double snr:{12.,100.}) {
        const auto packet=preamble(cfo,snr);
        for(size_t chunk:{1,73,256,4096}) {
            const auto state=processWorker(packet,chunk);
            require(state.error.empty(),"LTF processing error");
            require(state.synchronized_packets==1,"LTF confirmation count");
            const auto e=state.recent_sync.back();
            require(e.segment==42,"LTF segment");
            require(e.ltf_first_index==492 && e.packet_start_index==300,"LTF timing boundary");
            require(std::abs(e.total_cfo_hz-cfo)<(snr<20?5000:50),"LTF refined CFO error");
            require(e.correlation>.55,"LTF score");
        }
    }
    require(processWorker(preamble(30000,100,0),31).recent_sync.at(0).packet_start_index==0,"segment-start packet timing");
    auto badCp=preamble(0);std::fill(badCp.begin()+460,badCp.begin()+492,IQ{});
    require(processWorker(badCp,83).synchronized_packets==0,"invalid LTF guard interval accepted");
    auto badSecond=preamble(0);std::fill(badSecond.begin()+556,badSecond.begin()+620,IQ{});
    require(processWorker(badSecond,83).synchronized_packets==0,"missing second LTF accepted");
    require(processWorker(waveform(.12,40000),79).synchronized_packets==0,"STF alone confirmed as LTF");
    Samples tone(1200);for(size_t i=0;i<tone.size();++i) {
        const auto x=std::polar(3000.0,2*std::numbers::pi*120000*double(i)/20e6);
        tone[i]={int16_t(x.real()),int16_t(x.imag())};
    }
    require(processWorker(tone,97).synchronized_packets==0,"tone confirmed as LTF");
    auto multi=preamble(50000);auto second=preamble(-70000);multi.insert(multi.end(),second.begin(),second.end());
    auto state=processWorker(multi,137);require(state.synchronized_packets==2,"multiple LTF packets");
    require(state.recent_sync.back().packet_start_index==1220,"second LTF offset");
    // A discontinuity between the LTF copies must cancel the pending candidate.
    WiFiDemodulator worker;worker.enable(true,20e6);auto packet=preamble(80000);
    worker.submit(std::span(packet).first(556),0,1,true);
    worker.submit(std::span(packet).subspan(556),556,1,true);
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(worker.snapshot().processed_samples<packet.size() && std::chrono::steady_clock::now()<end)std::this_thread::yield();
    require(worker.snapshot().processed_samples==packet.size(),"LTF gap test deadline");
    require(worker.snapshot().synchronized_packets==0,"LTF bridges discontinuity");
}
int main() {try {lltfTests();std::cout<<"L-LTF timing/CFO: PASS\n";accumulatorTests();std::cout<<"partial RX accumulator: PASS\n";detectorTests();std::cout<<"detector: PASS (27 amplitude/CFO/SNR cases, 8 chunk sizes, noise, multiple bursts, reset)\n";queueTests();std::cout<<"queue/worker: PASS\n";readerTests();std::cout<<"offline sc16: PASS\n";}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

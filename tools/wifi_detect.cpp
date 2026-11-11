#include "LStfDetector.h"
#include "Sc16Reader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {
    try {
        if (argc<2 || argc>3) throw std::invalid_argument("usage: wifi_detect recording.bin [block_samples=4096] (20 MS/s sc16 LE)");
        size_t size=4096;
        if (argc==3) { size_t used=0; const std::string arg=argv[2]; size=std::stoull(arg,&used); if (used!=arg.size() || arg[0]=='-' || !size || size>16777216) throw std::invalid_argument("block size must be 1..16777216"); }
        std::ifstream file(argv[1],std::ios::binary);
        if (!file) throw std::runtime_error("cannot open IQ recording");
        LStfDetector detector;
        std::cout<<"candidate_sample,correlation,cfo_hz,power,status\n";
        for (;;) {
            auto block=readSc16(file,size); if (block.empty()) break;
            for (auto e:detector.process(block)) std::cout<<e.sample_index<<','<<e.correlation<<','<<e.cfo_hz<<','<<e.power<<",plateau_candidate\n";
        }
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}

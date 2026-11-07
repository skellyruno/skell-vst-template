#include <SkellCore/Core.h>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>

// Informational timings only, NOT a CI timing gate.
int main() {
    using clock=std::chrono::steady_clock;
    constexpr int rate=48000, frames=rate*4, patternSize=4096;
    std::array<std::array<float,2>,patternSize> input{};
    for(int i=0;i<patternSize;++i){
        input[i][0]=.8f*float(std::sin(i*.131));
        input[i][1]=.6f*float(std::cos(i*.089));
    }
    volatile double finalChecksum=0;
    for(auto quality : {skellcore::TruePeakLimiter::Quality::Economy,
                        skellcore::TruePeakLimiter::Quality::Standard}) {
        skellcore::TruePeakLimiter limiter;limiter.prepare(rate,2,quality);
        std::array<float,2> out{};
        for(int i=0;i<patternSize;++i)limiter.processFrame(input[i].data(),out.data(),2);
        double checksum=0;
        const auto start=clock::now();
        for(int i=0;i<frames;++i){
            limiter.processFrame(input[i&(patternSize-1)].data(),out.data(),2);
            checksum+=out[0];
        }
        const double elapsed=std::chrono::duration<double>(clock::now()-start).count();
        finalChecksum=checksum;
        std::cout<<(quality==skellcore::TruePeakLimiter::Quality::Economy?"Economy":"Standard")
                 <<" limiter: "<<std::fixed<<std::setprecision(3)
                 <<elapsed/4.0*100.0<<"% of one 48k real-time stream (wall time), "
                 <<elapsed/frames*1.e9<<" ns/stereo frame\n";
    }
    skellcore::FDNReverb reverb;reverb.prepare(rate);
    double checksum=0;
    const auto start=clock::now();
    for(int i=0;i<frames;++i) {
        const auto out=reverb.processFrame(input[i&(patternSize-1)][0],0.f);
        checksum+=out[0];
    }
    const double elapsed=std::chrono::duration<double>(clock::now()-start).count();
    finalChecksum=checksum;
    std::cout<<"FDN reverb: "<<elapsed/4.0*100.0
             <<"% of one 48k real-time stream (wall time)\n";
    if(finalChecksum==1.e100)std::cerr<<"unreachable\n";
}

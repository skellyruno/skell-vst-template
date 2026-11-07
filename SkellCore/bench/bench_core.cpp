#include <SkellCore/Core.h>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <array>

// A measurement tool, not a CI performance gate. Use Release on the same
// hardware to compare commit-to-commit. Wall-clock timing is machine-dependent.
int main() {
    using clock=std::chrono::steady_clock;
    constexpr int rate=48000, frames=rate*4;
    volatile double sink=0.0;
    for(auto profile : {skellcore::TruePeakLimiter::Quality::Economy,
                        skellcore::TruePeakLimiter::Quality::Standard}) {
        skellcore::TruePeakLimiter lim;
        lim.prepare(rate,2,profile);
        std::array<float,2> in{},out{};
        auto start=clock::now();
        for(int i=0;i<frames;++i){
            in[0]=.8f*float(std::sin(i*.131)); in[1]=.6f*float(std::cos(i*.089));
            lim.processFrame(in.data(),out.data(),2);
            sink+=out[0]*.00001;
        }
        const double elapsed=std::chrono::duration<double>(clock::now()-start).count();
        std::cout << (profile==skellcore::TruePeakLimiter::Quality::Economy?"Economy":"Standard")
                  << " limiter: " << std::fixed << std::setprecision(4)
                  << elapsed/4.0*100.0 << "% of one core at 48 kHz, "
                  << (elapsed/frames*1.e9) << " ns/stereo frame\n";
    }
    skellcore::FDNReverb reverb;
    reverb.prepare(rate);
    auto start=clock::now();
    for(int i=0;i<frames;++i){
        const auto out=reverb.processFrame(.4f*float(std::sin(i*.05)),0.f);
        sink += out[0]*.00001;
    }
    const double elapsed=std::chrono::duration<double>(clock::now()-start).count();
    std::cout << "FDN reverb: " << elapsed/4.0*100.0
              << "% of one core at 48 kHz\n";
    if (sink==1e100) std::cerr << "unreachable";
}

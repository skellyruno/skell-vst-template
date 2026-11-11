#ifdef NDEBUG
#undef NDEBUG
#endif
#include <SkellCore/LiveEngine.h>
#include <cassert>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;
using skellcore::live::LiveMultibandDucker;
using skellcore::live::LiveDuckerParams;
using skellcore::live::LatencyContract;
struct Audio { std::vector<float> left, right; };

Audio waves(int count, double rate, double f1, double f2 = 0) {
    Audio a;
    a.left.resize(static_cast<std::size_t>(count));
    a.right.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i)/rate;
        const float x = static_cast<float>(0.2*std::sin(2*pi*f1*t)
                              + (f2>0 ? 0.18*std::sin(2*pi*f2*t) : 0));
        a.left[static_cast<std::size_t>(i)] = x;
        a.right[static_cast<std::size_t>(i)] = x * 0.85f;
    }
    return a;
}
void render(LiveMultibandDucker& e, Audio& a, const Audio* sc, int block) {
    const int frames = static_cast<int>(a.left.size());
    for (int off = 0; off < frames; off += block) {
        const int size = std::min(block, frames - off);
        float* main[2]{a.left.data()+off,a.right.data()+off};
        const float* side[2]{sc ? sc->left.data()+off : nullptr,
                             sc ? sc->right.data()+off : nullptr};
        e.process(main,side,2,sc?2:0,size);
    }
}
double amplitude(const std::vector<float>& samples, double rate, double freq) {
    const int count = static_cast<int>(samples.size());
    const int start = count/2;
    double a = 0, b = 0;
    for(int i=start;i<count;++i){
        const double phase=2*pi*freq*i/rate;
        a += samples[static_cast<std::size_t>(i)]*std::cos(phase);
        b += samples[static_cast<std::size_t>(i)]*std::sin(phase);
    }
    return 2.0*std::hypot(a,b)/(count-start);
}
void testContract() {
    LatencyContract c;
    c.configure(48000, 0);
    assert(c.milliseconds() == 0 && c.within(1.0));
    assert(LatencyContract::fromMilliseconds(1.0,48000)==48);
    assert(LatencyContract::fromMilliseconds(3.5,48000)==168);
    assert(LatencyContract::fromMilliseconds(-3,48000)==0);
    assert(LatencyContract::fromMilliseconds(std::numeric_limits<double>::quiet_NaN(),48000)==0);
    c.configure(48000,2048);
    assert(c.milliseconds()>42.6 && c.milliseconds()<42.7);
    assert(!c.within(10));
    c.configure(0,-50); assert(c.samples==0 && c.sampleRate==48000);
}
void testNoAddedLatency() {
    for (double fs : {44100.,48000.,96000.,192000.}) {
        LiveMultibandDucker e;
        e.prepare(fs);
        assert(e.latencySamples()==0);
        float sample[1]{1.0f};float* main[1]{sample};
        e.process(main,nullptr,1,0,1);
        assert(sample[0]==1.0f); // impulse is not shifted, even by 1 sample
        e.reset();
        auto in=waves(12345,fs,1000,3000), out=in;
        auto sc=waves(12345,fs,1000);
        LiveDuckerParams p; p.amount=0.0f;
        e.setParameters(p);
        render(e,out,&sc,7);
        assert(out.left==in.left && out.right==in.right);
        e.reset();
        p.amount=1.0f;p.enabled=false;e.setParameters(p);
        out=in;render(e,out,&sc,777);
        assert(out.left==in.left && out.right==in.right);
    }
}
void testSignalAndBlocks() {
    constexpr int n=48000;
    auto in=waves(n,48000,1000,250);
    auto sc=waves(n,48000,1000);
    LiveDuckerParams p;p.amount=1.0f;p.attackMs=5;p.releaseMs=80;
    LiveMultibandDucker reference; reference.prepare(48000);reference.setParameters(p);
    auto out=in;render(reference,out,&sc,211);
    const double original=amplitude(in.left,48000,1000);
    const double gained=amplitude(out.left,48000,1000);
    const double reductionDb=20*std::log10(gained/original);
    std::cout<<"1 kHz dynamic attenuation: "<<reductionDb<<" dB\n";
    float mainLevel[LiveMultibandDucker::bands]{};
    float sideLevel[LiveMultibandDucker::bands]{};
    float cutLevel[LiveMultibandDucker::bands]{};
    reference.copyDisplayBands(0, mainLevel, sideLevel, cutLevel);
    float maximumCut = 0.f;
    for (int b = 0; b < LiveMultibandDucker::bands; ++b) {
        assert(std::isfinite(mainLevel[b]) && std::isfinite(sideLevel[b]));
        assert(std::isfinite(cutLevel[b]) && cutLevel[b] >= 0.f && cutLevel[b] <= 40.f);
        maximumCut = std::max(maximumCut, cutLevel[b]);
    }
    assert(maximumCut > 5.f);

    assert(std::isfinite(reductionDb) && reductionDb < -5.0 && reductionDb >= -36.0);
    for (int block : {1,17,64,512,1024}) {
        LiveMultibandDucker e;e.prepare(48000);e.setParameters(p);
        auto comparison=in;
        render(e,comparison,&sc,block);
        for(std::size_t i=0;i<out.left.size();++i) {
            assert(comparison.left[i]==out.left[i]);
            assert(comparison.right[i]==out.right[i]);
        }
    }
}
void testMonoAndBadSamples() {
    LiveMultibandDucker e;e.prepare(48000);
    LiveDuckerParams p;p.amount=1;p.stereoLink=1;e.setParameters(p);
    std::array<float,9> main {0.1f, 0.2f, 0.3f,
          std::numeric_limits<float>::quiet_NaN(),
          std::numeric_limits<float>::infinity(),0.f,0.8f,-0.4f,0.f};
    const float sc[9]{0.2f,-0.2f,0.4f,0.1f,0.1f,0.f,0.7f,0.8f,0.f};
    float* m[1]{main.data()};const float* side[1]{sc};
    e.process(m,side,1,1,static_cast<int>(main.size()));
    for(float f:main) assert(std::isfinite(f));
    e.process(m,side,1,1,0);
    e.process(nullptr,side,1,1,9);
}
void testRapidChanges() {
    LiveMultibandDucker e;e.prepare(48000);
    auto main=waves(48000,48000,1000);auto side=waves(48000,48000,1000);
    for(int off=0;off<48000;off+=96) {
        LiveDuckerParams p;
        p.amount=(off/96)%6==0 ? 0.f:1.f;
        p.enabled=(off/96)%5 != 0;
        p.freeze=(off/96)%8==0;
        p.lowHz=(off/96)%2==0?80.f:700.f;
        p.highHz=13000;
        e.setParameters(p);
        float* m[2]{main.left.data()+off,main.right.data()+off};
        const float* sc[2]{side.left.data()+off,side.right.data()+off};
        e.process(m,sc,2,2,96);
    }
    float peak=0;
    for(float f:main.left){
        assert(std::isfinite(f));
        peak=std::max(peak,std::abs(f));
    }
    assert(peak < 0.4f);
}
}
int main() {
    testContract();
    testNoAddedLatency();
    testSignalAndBlocks();
    testMonoAndBadSamples();
    testRapidChanges();
    std::cout<<"SkellCore Live Engine checks passed: zero latency, spectral action, variable blocks, invalid input and mode changes\n";
}

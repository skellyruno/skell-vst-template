#ifdef NDEBUG
#undef NDEBUG
#endif
#include <SkellCore/LatencyAlignedMix.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

using skellcore::live::LatencyAlignedMix;
namespace {
constexpr double pi = 3.1415926535897932384626433832795;

struct Stereo {
    std::vector<float> l, r;
    explicit Stereo(int n) : l(static_cast<std::size_t>(n), 0.0f),
                             r(static_cast<std::size_t>(n), 0.0f) {}
};
Stereo input(int size, int rate) {
    Stereo a(size);
    for (int n = 0; n < size; ++n) {
        const double t = static_cast<double>(n) / rate;
        a.l[static_cast<std::size_t>(n)] = static_cast<float>(
            .34 * std::sin(2.0 * pi * 220.0 * t) +
            .15 * std::sin(2.0 * pi * 880.0 * t));
        a.r[static_cast<std::size_t>(n)] = a.l[static_cast<std::size_t>(n)] * .82f;
    }
    return a;
}
Stereo wetDelayed(const Stereo& dry, int delay) {
    Stereo wet(static_cast<int>(dry.l.size()));
    for (std::size_t i = static_cast<std::size_t>(delay); i < dry.l.size(); ++i) {
        wet.l[i] = dry.l[i - static_cast<std::size_t>(delay)] * 0.72f;
        wet.r[i] = dry.r[i - static_cast<std::size_t>(delay)] * 0.93f;
    }
    return wet;
}
Stereo render(LatencyAlignedMix& mixer, const Stereo& dry,
              const Stereo& wet, int block, bool inPlace = false) {
    Stereo result(static_cast<int>(dry.l.size()));
    if (inPlace) {
        result = wet;
    }
    const int n = static_cast<int>(dry.l.size());
    for (int offset = 0; offset < n; offset += block) {
        const int frames = std::min(block, n - offset);
        const float* source[2] { dry.l.data() + offset, dry.r.data() + offset };
        const float* processed[2] {
            inPlace ? result.l.data() + offset : wet.l.data() + offset,
            inPlace ? result.r.data() + offset : wet.r.data() + offset
        };
        float* output[2] { result.l.data() + offset, result.r.data() + offset };
        assert(mixer.process(source, processed, output, 2, frames));
    }
    return result;
}
void testDelayAndZeroDelay() {
    for (int rate : {44100, 48000, 96000, 192000}) {
        for (int latency : {0, 1, 17, 128, 2048}) {
            LatencyAlignedMix mix;
            assert(!mix.isPrepared());
            assert(mix.prepare(rate, latency, 2, 0.0));
            assert(mix.latencySamples() == latency);
            mix.setMixImmediately(0.0f);
            Stereo impulse(latency + 130), wet(latency + 130);
            impulse.l[0] = 0.7f;
            impulse.r[0] = -0.5f;
            const auto result = render(mix, impulse, wet, 17);
            for (std::size_t i = 0; i < result.l.size(); ++i) {
                assert(result.l[i] == (static_cast<int>(i) == latency ? 0.7f : 0.0f));
                assert(result.r[i] == (static_cast<int>(i) == latency ? -0.5f : 0.0f));
            }
        }
    }
}
void testWetAndDryAlignment() {
    for (int latency : {0, 3, 127, 2048}) {
        auto dry = input(16000, 48000);
        auto wet = wetDelayed(dry, latency);
        for (float level : {0.f, 0.5f, 1.f}) {
            LatencyAlignedMix mix;
            assert(mix.prepare(48000, latency, 2, 0.0));
            mix.setMixImmediately(level);
            const auto output = render(mix, dry, wet, 257);
            const float expectedGainL = (1.f - level) + level * .72f;
            const float expectedGainR = (1.f - level) + level * .93f;
            for (int i = latency; i < 16000; ++i) {
                // Finite precision of floating point gain combination is expected.
                assert(std::fabs(output.l[static_cast<std::size_t>(i)] -
                    dry.l[static_cast<std::size_t>(i-latency)] * expectedGainL) < 1e-6f);
                assert(std::fabs(output.r[static_cast<std::size_t>(i)] -
                    dry.r[static_cast<std::size_t>(i-latency)] * expectedGainR) < 1e-6f);
            }
            LatencyAlignedMix inPlace;
            assert(inPlace.prepare(48000, latency, 2, 0.0));
            inPlace.setMixImmediately(level);
            const auto alias = render(inPlace, dry, wet, 23, true);
            assert(alias.l == output.l && alias.r == output.r);
        }
    }
}
void testPartitioningAndBypass() {
    constexpr int rate = 48000;
    constexpr int delay = 173;
    constexpr int frames = 12000;
    const auto dry = input(frames, rate);
    const auto wet = wetDelayed(dry, delay);

    auto variable = [&](int block) {
        LatencyAlignedMix mix;
        assert(mix.prepare(rate, delay, 2, 6.0));
        Stereo output(frames);
        const std::array<int, 6> boundaries {0, 117, 999, 1988, 4080, frames};
        const std::array<float, 5> states {1.f, 0.f, .37f, 1.f, 0.f};
        for (int section = 0; section < 5; ++section) {
            mix.setTargetWet(states[static_cast<std::size_t>(section)]);
            int position = boundaries[static_cast<std::size_t>(section)];
            const int end = boundaries[static_cast<std::size_t>(section+1)];
            while (position < end) {
                const int count = std::min(block, end - position);
                const float* d[2]{dry.l.data()+position,dry.r.data()+position};
                const float* w[2]{wet.l.data()+position,wet.r.data()+position};
                float* o[2]{output.l.data()+position,output.r.data()+position};
                assert(mix.process(d,w,o,2,count));
                position += count;
            }
        }
        assert(mix.currentWetMix() == 0.f);
        return output;
    };
    const auto reference = variable(1);
    for(int block : {7, 64, 128, 511, 8192}) {
        const auto result = variable(block);
        assert(result.l == reference.l);
        assert(result.r == reference.r);
    }
}
void testClickFree() {
    LatencyAlignedMix mix;
    assert(mix.prepare(48000, 0, 1, 5.0));
    mix.setMixImmediately(1.f);
    constexpr int frames = 1200;
    std::array<float, frames> dry{}, wet{}, result{};
    wet.fill(1.f);
    const float* d[1]{dry.data()};
    const float* w[1]{wet.data()};
    float* o[1]{result.data()};
    mix.setBypassed(true);
    assert(mix.process(d,w,o,1,frames));
    float maximumStep = 0.0f;
    for (int i = 1; i < frames; ++i)
        maximumStep = std::max(maximumStep, std::fabs(result[static_cast<std::size_t>(i)] -
                                                    result[static_cast<std::size_t>(i-1)]));
    assert(maximumStep <= 1.f/240.f + 1e-5f);
    assert(result.back() == 0.f);
    assert(mix.currentWetMix() == 0.f);
    mix.setBypassed(false);
    assert(mix.process(d,w,o,1,frames));
    assert(result.back() == 1.f);
    mix.reset();
    assert(mix.currentWetMix() == 1.f);
}
void testSafety() {
    LatencyAlignedMix mix;
    float sample = 1.f;
    const float* d[1]{&sample}, *w[1]{&sample};
    float* o[1]{&sample};
    assert(!mix.process(d,w,o,1,1));
    assert(!mix.prepare(48000,-1,2));
    assert(!mix.prepare(48000,LatencyAlignedMix::maxLatencySamples + 1, 2));
    assert(!mix.prepare(0,1,1));
    assert(!mix.prepare(48000,1,0));
    assert(!mix.prepare(48000,1,9));
    assert(mix.prepare(48000,4,2));
    assert(!mix.prepare(0,8,2) && mix.latencySamples() == 4);
    assert(!mix.process(d,w,o,3,1));
    assert(!mix.process(nullptr,w,o,1,1));
    assert(!mix.process(d,w,o,1,-1));
    float values[8] {1.f, std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity(),0.f,-1.f,0.f,0.f,0.f};
    float zeros[8]{}, result[8]{};
    const float* dry[1]{values}, *wet[1]{zeros};
    float* out[1]{result};
    mix.setMixImmediately(0.f);
    assert(mix.process(dry,wet,out,1,8));
    for(float v:result) assert(std::isfinite(v));
    mix.reset();
    assert(mix.process(dry,wet,out,1,8));
    for(float v:result) assert(std::isfinite(v));
    // NaN target is sanitized; doesn't poison a crossfade.
    mix.setTargetWet(std::numeric_limits<float>::quiet_NaN());
    assert(mix.targetWetMix() == 0.f);
    // Memory bound & 8-channel safety.
    assert(mix.prepare(44100,22,8));
    std::array<std::array<float,40>,8> src{},processed{},output{};
    const float* sources[8]{},*processedPtrs[8]{};
    float* outputs[8]{};
    for(int c=0;c<8;++c) {
        sources[c]=src[static_cast<std::size_t>(c)].data();
        processedPtrs[c]=processed[static_cast<std::size_t>(c)].data();
        outputs[c]=output[static_cast<std::size_t>(c)].data();
        src[static_cast<std::size_t>(c)][0]=static_cast<float>(c+1);
    }
    mix.setMixImmediately(0.f);
    assert(mix.process(sources,processedPtrs,outputs,8,40));
    for(int c=0;c<8;++c)
        assert(output[static_cast<std::size_t>(c)][22] == static_cast<float>(c+1));
}
}
int main() {
    testDelayAndZeroDelay();
    testWetAndDryAlignment();
    testPartitioningAndBypass();
    testClickFree();
    testSafety();
    std::cout << "SkellCore LatencyAlignedMix: impulse, alignment, bypass,"
              << " aliasing, block partition, 8-ch and nonfinite checks passed\n";
}

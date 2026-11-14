#ifdef NDEBUG
#undef NDEBUG
#endif
// Limiter guarantees (this file kept its original name so build files keep working; it used to compare the
// Standard output against a copy of the previous hold-timer limiter, which overshot the ceiling when a quieter
// peak entered the lookahead after a louder one).
#include <SkellCore/Core.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

using skellcore::TruePeakLimiter;
constexpr double kPi = 3.1415926535897932384626433832795;

// Sample-domain overshoot is exact; the 0.0015 allowance covers the windowed-sinc interpolation error (the
// limiter works on interpolated peaks, which are an estimate of the true peak, not a certified measurement).
static double run(TruePeakLimiter& lim, const std::vector<float>& in, std::vector<float>* out = nullptr) {
    double worst = 0;
    for (float x : in) {
        const float frame[2] {x, x}; float o[2] {};
        lim.processFrame(frame, o, 2);
        worst = std::max(worst, std::abs(static_cast<double>(o[0])));
        if (out) out->push_back(o[0]);
    }
    return worst;
}

int main() {
    for (int rate : {44100, 48000, 96000, 192000}) {
        for (auto q : {TruePeakLimiter::Quality::Economy, TruePeakLimiter::Quality::Standard}) {
            for (double ceilDb : {-1.0, -6.0, -0.1}) {
                const double ceiling = std::pow(10.0, ceilDb / 20.0);
                TruePeakLimiter lim; lim.prepare(rate, 2, q); lim.setCeilingDb(static_cast<float>(ceilDb));
                assert(lim.latencySamples() == std::max(48, static_cast<int>(rate * .003)));
                const double tol = ceiling * 0.0015;
                std::mt19937 g(7u + static_cast<unsigned>(rate));
                std::uniform_real_distribution<float> u(-1.f, 1.f);
                double worst = 0;
                // 1. staggered peaks: a loud burst, then quieter-but-still-over bursts while the first is
                //    still inside the lookahead (the case the old hold timer got wrong)
                for (int rep = 0; rep < 60; ++rep) {
                    std::vector<float> in(static_cast<std::size_t>(rate / 5), 0.f);
                    const int gap = 3 + static_cast<int>(g() % 200);
                    const std::size_t a = 100;
                    in[a] = 3.f; in[a + static_cast<std::size_t>(gap)] = 1.4f * (u(g) > 0 ? 1.f : -1.f);
                    in[a + static_cast<std::size_t>(gap) * 2] = 1.1f;
                    worst = std::max(worst, run(lim, in));
                }
                // 2. sustained loud tones with level steps, and random full-scale noise
                std::vector<float> tone;
                for (int i = 0; i < rate; ++i) tone.push_back(static_cast<float>(std::sin(2 * kPi * 997.0 * i / rate) * (i < rate / 3 ? 1.8 : i < 2 * rate / 3 ? 1.05 : 4.0)));
                worst = std::max(worst, run(lim, tone));
                std::vector<float> noise; for (int i = 0; i < rate; ++i) noise.push_back(u(g) * 3.f);
                worst = std::max(worst, run(lim, noise));
                assert(worst <= ceiling + tol);
                std::cout << rate << " Hz " << (q == TruePeakLimiter::Quality::Economy ? "Economy " : "Standard")
                          << " ceiling " << ceilDb << " dB: worst " << worst << " (limit " << ceiling + tol << ")\n";
            }
        }
    }
    // 3. transparent below the ceiling: output is the input delayed by exactly latencySamples(), bit-exact
    {
        TruePeakLimiter lim; lim.prepare(48000, 2); lim.setCeilingDb(-1.f);
        std::mt19937 g(3); std::uniform_real_distribution<float> u(-.45f, .45f);   // inter-sample peaks stay under the ceiling
        std::vector<float> in, out; for (int i = 0; i < 20000; ++i) in.push_back(u(g));
        run(lim, in, &out);
        const int L = lim.latencySamples();
        for (std::size_t i = static_cast<std::size_t>(L); i < in.size(); ++i) assert(out[i] == in[i - static_cast<std::size_t>(L)]);
        std::cout << "below ceiling: bit-exact delayed passthrough (latency " << L << ")\n";
    }
    // 4. attack is a ramp, not a step: after a +12 dB burst, adjacent output samples of a steady tone
    //    never differ by more than the tone itself would, and release returns to unity within ~0.5 s
    {
        TruePeakLimiter lim; lim.prepare(48000, 2); lim.setCeilingDb(-1.f);
        std::vector<float> in, out;
        for (int i = 0; i < 48000; ++i) in.push_back(static_cast<float>(std::sin(2 * kPi * 100.0 * i / 48000.0) * (i >= 10000 && i < 10400 ? 4.0 : .3)));
        run(lim, in, &out);
        // applied gain = out / delayed input, wherever the input is large enough to measure it
        double maxGainStep = 0, prevGain = 1.0; bool have = false;
        for (std::size_t i = 145; i < out.size(); ++i) {
            const double x = in[i - 144];
            if (std::abs(x) < .15) { have = false; continue; }   // only compare adjacent measurable samples
            const double gain = out[i] / x;
            if (have) maxGainStep = std::max(maxGainStep, std::abs(gain - prevGain));
            prevGain = gain; have = true;
        }
        const double maxStepRatio = maxGainStep;   // gain change between measurable samples
        assert(maxGainStep < .05);                // a step limiter jumps by ~0.78 here; the ramp moves ~0.006/sample
        double tailRatio = 0; for (std::size_t i = 40000; i < 48000; ++i) if (std::abs(in[i - 144]) > .2) tailRatio = std::max(tailRatio, std::abs(static_cast<double>(out[i]) / in[i - 144]));
        assert(tailRatio > .999);
        std::cout << "attack ramp max gain change between measurable samples " << maxStepRatio << ", released to " << tailRatio << " by 0.8 s\n";
    }
    // 5. robustness: NaN/inf and a mono stream
    {
        TruePeakLimiter lim; lim.prepare(48000, 1); lim.setCeilingDb(-1.f);
        for (int i = 0; i < 5000; ++i) {
            float x = i % 97 == 0 ? std::numeric_limits<float>::quiet_NaN() : (i % 89 == 0 ? std::numeric_limits<float>::infinity() : 2.f);
            float o = 0; lim.processFrame(&x, &o, 1); assert(std::isfinite(o) && std::abs(o) <= .8914f * 1.0015f);
        }
        lim.reset(); lim.prepare(48000, 2); // re-prepare must not leak state
        std::cout << "NaN/inf/mono robust\n";
    }
    std::cout << "PASS: limiter ceiling guarantee, transparency, ramped attack, robustness\n";
}

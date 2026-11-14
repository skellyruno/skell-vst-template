#pragma once
// SkellCore higher-order feedback delay network with separate low- and high-frequency decay times.
//
//   BandFDN<8>  / FDNReverb8   - 8 lines,  moderate CPU, noticeably denser than the 4-line FDNReverb in Core.h
//   BandFDN<16> / FDNReverb16  - 16 lines, densest tail, about 4x the CPU of FDNReverb
//
// Design (all standard, nothing exotic):
//  * Householder feedback matrix I - (2/N)11^T: orthogonal, so the loop is lossless before the decay filters,
//    and O(N) per sample instead of O(N^2).
//  * Delay lengths are distinct primes (so they share no common factors), spread geometrically between
//    minDelayMs and maxDelayMs. Larger values give a bigger, sparser-sounding room; smaller values a tighter one.
//  * Per-line one-pole "absorption" filter H(z) = g(1-p) / (1 - p z^-1) (Jot): DC gain g gives the low-band RT60 and
//    the pole p is solved so the gain at a treble reference frequency gives the high-band RT60. Because g and p are
//    derived from each line's own length, every line decays at the same rate, whatever its length.
//  * Stereo: input and output use orthogonal +/-1 (Walsh) patterns, so left and right tails are decorrelated and
//    stereo input keeps its image.
//
// Real-time rules match Core.h: allocation only in prepare(); processFrame() is noexcept, sanitises NaN/Inf and
// returns the dry input until prepare() has run. Not production-certified; verify by ear and by test.
#include <SkellCore/Core.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace skellcore {

template <std::size_t N>
class BandFDN {
    static_assert(N == 4 || N == 8 || N == 16, "BandFDN supports 4, 8 or 16 lines");
public:
    static constexpr std::size_t lineCount = N;

    // minDelayMs/maxDelayMs choose the delay-length range (clamped to 2..400 ms, min < max). Allocates.
    void prepare(double sampleRate, double minDelayMs = 24.0, double maxDelayMs = 80.0) {
        ready_ = false;
        rate_ = detail::safeSampleRate(sampleRate);
        const double lo = detail::clamp(minDelayMs, 2.0, 400.0);
        const double hi = std::max(lo * 1.25, detail::clamp(maxDelayMs, 2.0, 400.0));
        for (std::size_t i = 0; i < N; ++i) {
            const double t = N > 1 ? static_cast<double>(i) / static_cast<double>(N - 1) : 0.0;
            const double ms = lo * std::pow(hi / lo, t);
            const std::size_t target = std::max<std::size_t>(31, static_cast<std::size_t>(rate_ * ms * .001));
            lengths_[i] = nextPrimeAbove(i == 0 ? target : std::max(target, lengths_[i - 1] + 1));
            lines_[i].assign(lengths_[i], 0.0f);
        }
        reset();
        valid_ = false;
        setParameters(decayLow_, decayHigh_, wet_);
        ready_ = true;
    }
    void reset() noexcept {
        positions_.fill(0);
        state_.fill(0.0f);
        for (auto& l : lines_) std::fill(l.begin(), l.end(), 0.0f);
    }
    // Frequency at which decayHigh applies (default 4 kHz). Takes effect on the next setParameters() call.
    void setTrebleReferenceHz(double hz) noexcept {
        trebleHz_ = detail::clamp(hz, 500.0, 12000.0);
        valid_ = false;
    }
    // decayLowSeconds: RT60 of the low band (DC). decayHighSeconds: RT60 at the treble reference, clamped to
    // <= decayLow (a tail that gets *brighter* with time is not supported). wet 0..1; 0 is an exact passthrough.
    void setParameters(float decayLowSeconds, float decayHighSeconds, float wet) noexcept {
        const float low = static_cast<float>(detail::clamp(decayLowSeconds, .15, 20));
        const float high = std::min(low, static_cast<float>(detail::clamp(decayHighSeconds, .05, 20)));
        wet_ = static_cast<float>(detail::clamp(wet, 0, 1));
        dryGain_ = 1.f - wet_;
        wetGain_ = wet_ * kWetScale;
        // Hosts may repeat identical values every block; skip the exp()/sqrt() work then.
        if (valid_ && low == decayLow_ && high == decayHigh_) return;
        decayLow_ = low; decayHigh_ = high; valid_ = true;
        constexpr double logMinus60dB = -6.907755278982137052;
        const double w = 2.0 * detail::pi * trebleHz_ / rate_;
        const double c = std::cos(std::min(w, detail::pi));
        for (std::size_t i = 0; i < N; ++i) {
            const double len = static_cast<double>(lengths_[i]);
            const double gLow = std::exp(logMinus60dB * len / (rate_ * decayLow_));
            const double gHigh = std::exp(logMinus60dB * len / (rate_ * decayHigh_));
            const double r = std::min(1.0, gHigh / gLow);   // <= 1 so the pole is >= 0 (a lowpass, always stable)
            double p = 0.0;
            if (r < .9999) {
                const double r2 = r * r;
                const double disc = std::max(0.0, (1.0 - r2 * c) * (1.0 - r2 * c) - (1.0 - r2) * (1.0 - r2));
                p = ((1.0 - r2 * c) - std::sqrt(disc)) / (1.0 - r2);
                p = std::clamp(p, 0.0, .995);
            }
            pole_[i] = static_cast<float>(p);
            gain_[i] = static_cast<float>(gLow * (1.0 - p));
        }
    }
    int delayLengthSamples(std::size_t line) const noexcept { return line < N ? static_cast<int>(lengths_[line]) : 0; }
    bool isPrepared() const noexcept { return ready_; }
    std::size_t estimatedWorkingBytes() const noexcept {
        std::size_t n = 0; for (auto l : lengths_) n += l * sizeof(float);
        return n + sizeof(state_) + sizeof(pole_) + sizeof(gain_);
    }

    std::array<float, 2> processFrame(float left, float right) noexcept {
        const float inL = std::isfinite(left) ? left : 0.f, inR = std::isfinite(right) ? right : 0.f;
        if (!ready_) return {inL, inR};
        std::array<float, N> x {}, y {};
        float sum = 0.f, wetL = 0.f, wetR = 0.f;
        for (std::size_t i = 0; i < N; ++i) {
            x[i] = lines_[i][positions_[i]];
            float s = gain_[i] * x[i] + pole_[i] * state_[i];
            if (!std::isfinite(s) || std::abs(s) < 1.e-20f) s = 0.f;
            state_[i] = s;
            y[i] = s;
            sum += s;
            wetL += signL(i) * x[i];
            wetR += signR(i) * x[i];
        }
        const float h = sum * (2.f / static_cast<float>(N));
        for (std::size_t i = 0; i < N; ++i) {
            float next = kDrive * (signL(i) * inL + signR(i) * inR) + (y[i] - h);
            if (!std::isfinite(next) || std::abs(next) < 1.e-20f) next = 0.f;
            lines_[i][positions_[i]] = next;
            if (++positions_[i] >= lengths_[i]) positions_[i] = 0;
        }
        return {inL * dryGain_ + wetGain_ * wetL, inR * dryGain_ + wetGain_ * wetR};
    }

private:
    static constexpr float signL(std::size_t i) noexcept { return (i & 1U) ? -1.f : 1.f; }
    static constexpr float signR(std::size_t i) noexcept { return (i & 2U) ? -1.f : 1.f; }
    static std::size_t nextPrimeAbove(std::size_t n) noexcept {
        if (n < 2) return 2;
        for (std::size_t c = n | 1U;; c += 2) {
            bool prime = true;
            for (std::size_t d = 3; d * d <= c; d += 2) if (c % d == 0) { prime = false; break; }
            if (prime) return c;
        }
    }
    // Calibrated so that a given RT60 sounds about as loud as FDNReverb's (see tests/test_fdn.cpp); scaled with N.
    static constexpr float kDrive = N == 4 ? .147f : N == 8 ? .104f : .0735f;
    static constexpr float kWetScale = .45f;

    double rate_ = 48000, trebleHz_ = 4000;
    float decayLow_ = 2.5f, decayHigh_ = 1.2f, wet_ = .25f, dryGain_ = .75f, wetGain_ = .1125f;
    bool ready_ = false, valid_ = false;
    std::array<std::vector<float>, N> lines_;
    std::array<std::size_t, N> lengths_ {}, positions_ {};
    std::array<float, N> state_ {}, pole_ {}, gain_ {};
};

using FDNReverb8 = BandFDN<8>;
using FDNReverb16 = BandFDN<16>;

} // namespace skellcore

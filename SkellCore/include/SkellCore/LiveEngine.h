#pragma once
// SkellCore Live Engine v1 — portable C++17; NO JUCE, allocations or locks in process().
// Zero-added-delay multiband sidechain processing for tracking-friendly plugins.
// Not a spectral FFT substitute in terms of sound: phase response and band selectivity differ.
#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace skellcore::live {

enum class Profile : std::uint8_t { Live = 0, Studio = 1 };

// Describes *intrinsic* DSP latency only. Device I/O buffering and host/DAW
// scheduling are outside this contract. Call host setLatencySamples outside audio.
struct LatencyContract {
    double sampleRate = 48000.0;
    int samples = 0;

    static double safeRate(double sr) noexcept {
        return std::isfinite(sr) && sr >= 1000.0 ? std::min(sr, 384000.0) : 48000.0;
    }
    static int fromMilliseconds(double ms, double sr) noexcept {
        if (!std::isfinite(ms) || ms <= 0.0) return 0;
        const double samples = ms * 0.001 * safeRate(sr);
        return samples >= static_cast<double>(std::numeric_limits<int>::max())
            ? std::numeric_limits<int>::max()
            : static_cast<int>(std::ceil(samples));
    }
    void configure(double sr, int intrinsicSamples) noexcept {
        sampleRate = safeRate(sr);
        samples = std::max(0, intrinsicSamples);
    }
    double milliseconds() const noexcept { return 1000.0 * samples / sampleRate; }
    bool within(double budgetMs) const noexcept {
        return std::isfinite(budgetMs) && budgetMs >= 0.0 && milliseconds() <= budgetMs;
    }
};

struct LiveDuckerParams {
    float amount = 0.5f, smooth = 0.3f, stereoLink = 0.0f;
    float attackMs = 10.0f, releaseMs = 150.0f;
    float lowHz = 20.0f, highHz = 20000.0f;
    bool freeze = false, enabled = true;
};

// Perfect *unity-gain* reconstruction through a set of matched complementary
// one-pole splits. Unlike a linear-phase FFT, these causal IIR filters introduce
// NO additional sample delay; band cuts are not sonically identical to FFT cuts.
class LiveMultibandDucker {
public:
    static constexpr int bands = 10;
    static constexpr int crossovers = bands - 1;
    static constexpr float maxCutDb = 24.0f;
    static constexpr std::array<float, crossovers> defaultCrossovers {
        110.0f, 205.0f, 380.0f, 700.0f, 1300.0f,
        2400.0f, 4450.0f, 8200.0f, 14500.0f
    };
    static constexpr std::array<float, bands> centreHz {
        75.f, 155.f, 280.f, 520.f, 960.f,
        1770.f, 3270.f, 6060.f, 11300.f, 18000.f
    };

    LiveMultibandDucker() { prepare(48000.0); }
    int latencySamples() const noexcept { return 0; }
    double sampleRate() const noexcept { return rate_; }

    // Never run prepare() concurrently with process(), or from processBlock.
    void prepare(double sr) noexcept {
        rate_ = LatencyContract::safeRate(sr);
        for (int i = 0; i < crossovers; ++i) {
            const double f = std::min(static_cast<double>(defaultCrossovers[static_cast<std::size_t>(i)]),
                                      rate_ * 0.44);
            splitCoef_[static_cast<std::size_t>(i)] =
                static_cast<float>(-std::expm1(-6.2831853071795864769 * f / rate_));
        }
        fadeStep_ = static_cast<float>(1.0 / (rate_ * 0.010));
        peakDecay_ = static_cast<float>(std::exp(-1.0 / rate_));
        updateCoefficients();
        updateBandMask();
        reset();
    }

    void reset() noexcept {
        for (auto& channel : mainSplit_) channel.fill(0.f);
        for (auto& channel : sideSplit_) channel.fill(0.f);
        for (auto& channel : envelope_) channel.fill(0.f);
        for (auto& channel : gain_) channel.fill(1.f);
        for (auto& channel : gainIncrement_) channel.fill(0.f);
        for (auto& channel : displayGain_) for (auto& value : channel) value.store(1.f, std::memory_order_relaxed);
        peak_.fill(0.f);
        wetMix_ = 0.f;
        gainCountdown_ = 0;
        meterCountdown_ = 0;
    }

    // Fine to call from a host audio callback; no allocations / locks / I/O.
    void setParameters(LiveDuckerParams p) noexcept {
        p.amount = unit(p.amount);
        p.smooth = unit(p.smooth);
        p.stereoLink = unit(p.stereoLink);
        p.attackMs = safe(p.attackMs, 1.f, 200.f);
        p.releaseMs = safe(p.releaseMs, 5.f, 2000.f);
        p.lowHz = safe(p.lowHz, 20.f, 20000.f);
        p.highHz = safe(p.highHz, 20.f, 20000.f);
        if (p.highHz < p.lowHz * 1.25f) {
            p.highHz = std::min(20000.f, p.lowHz * 1.25f);
            if (p.highHz == 20000.f) p.lowHz = std::min(p.lowHz, 16000.f);
        }
        if (p.freeze && !params_.freeze) {
            for (auto& channel : gainIncrement_) channel.fill(0.f);
        }
        if (p.attackMs != params_.attackMs || p.releaseMs != params_.releaseMs) {
            params_ = p;
            updateCoefficients();
        } else {
            params_ = p;
        }
        if (p.lowHz != cachedLow_ || p.highHz != cachedHigh_) updateBandMask();
    }

    // main is in/out; side is optional. Supports mono/stereo and arbitrary
    // host blocks including 1 sample. No output FFT buffering or lookahead.
    void process(float* const* main, const float* const* side,
                 int channels, int sideChannels, int frames) noexcept {
        if (!main || channels < 1 || channels > 2 || frames <= 0) return;
        for (int c = 0; c < channels; ++c) if (!main[c]) return;
        if (!side || sideChannels < 1 || sideChannels > 2) sideChannels = 0;
        if (sideChannels > 0)
            for (int c = 0; c < sideChannels; ++c)
                if (!side[c]) { sideChannels = 0; break; }

        const float desiredWet = params_.enabled && params_.amount > 0.f
                                && (sideChannels > 0 || params_.freeze) ? 1.f : 0.f;
        for (int n = 0; n < frames; ++n) {
            std::array<std::array<float, bands>, 2> components{};
            for (int c = 0; c < channels; ++c) {
                const float dry = finite(main[c][n]);
                split(dry, mainSplit_[static_cast<std::size_t>(c)],
                      components[static_cast<std::size_t>(c)]);
                main[c][n] = dry; // retained until gain application
            }
            for (int c = 0; c < channels; ++c) {
                float sc = 0.f;
                if (sideChannels == 1)
                    sc = finite(side[0][n]);
                else if (sideChannels == 2)
                    sc = channels == 1
                        ? 0.5f * (finite(side[0][n]) + finite(side[1][n]))
                        : finite(side[c][n]);

                std::array<float, bands> splitSc{};
                split(sc, sideSplit_[static_cast<std::size_t>(c)], splitSc);
                float highest = 0.f;
                for (int k = 0; k < bands; ++k) {
                    auto& e = envelope_[static_cast<std::size_t>(c)][static_cast<std::size_t>(k)];
                    const float measurement = std::abs(splitSc[static_cast<std::size_t>(k)]);
                    const float coef = measurement > e ? attackCoef_ : releaseCoef_;
                    e = measurement + (e - measurement) * coef;
                    if (std::abs(e) < 1e-18f) e = 0.f;
                    highest = std::max(highest, e);
                }
                auto& peak = peak_[static_cast<std::size_t>(c)];
                peak = std::max(highest, peak * peakDecay_);
                if (peak < 1e-18f) peak = 0.f;
            }

            if (gainCountdown_ == 0) {
                if (!params_.freeze) updateGainTargets(channels);
                gainCountdown_ = gainUpdateSamples;
            }
            --gainCountdown_;

            if (wetMix_ < desiredWet) wetMix_ = std::min(desiredWet, wetMix_ + fadeStep_);
            else if (wetMix_ > desiredWet) wetMix_ = std::max(desiredWet, wetMix_ - fadeStep_);

            for (int c = 0; c < channels; ++c) {
                float processed = 0.f;
                for (int k = 0; k < bands; ++k) {
                    auto& gain = gain_[static_cast<std::size_t>(c)][static_cast<std::size_t>(k)];
                    gain = std::clamp(gain + gainIncrement_[static_cast<std::size_t>(c)]
                                     [static_cast<std::size_t>(k)], 0.01f, 1.0f);
                    processed += components[static_cast<std::size_t>(c)]
                                          [static_cast<std::size_t>(k)] * gain;
                }
                const float dry = main[c][n];
                const float result = dry + wetMix_ * (processed - dry);
                main[c][n] = finite(result);
            }
            if (++meterCountdown_ >= 256) {
                meterCountdown_ = 0;
                for (int c = 0; c < channels; ++c)
                    for (int k = 0; k < bands; ++k)
                        displayGain_[static_cast<std::size_t>(c)][static_cast<std::size_t>(k)]
                            .store(gain_[static_cast<std::size_t>(c)][static_cast<std::size_t>(k)],
                                   std::memory_order_relaxed);
            }
        }
    }

    // Atomic display copy: safe from a message-thread timer while audio runs.
    void copyDisplayGains(int channel, float* out) const noexcept {
        if (!out) return;
        const auto& snapshot = displayGain_[static_cast<std::size_t>(std::clamp(channel, 0, 1))];
        for (int k = 0; k < bands; ++k)
            out[k] = snapshot[static_cast<std::size_t>(k)].load(std::memory_order_relaxed);
    }

private:
    static constexpr int gainUpdateSamples = 8;
    static float finite(float v) noexcept { return std::isfinite(v) ? v : 0.f; }
    static float unit(float v) noexcept { return std::clamp(finite(v), 0.f, 1.f); }
    static float safe(float v, float lo, float hi) noexcept {
        return std::clamp(std::isfinite(v) ? v : lo, lo, hi);
    }
    void split(float value, std::array<float, crossovers>& history,
               std::array<float, bands>& output) const noexcept {
        float rest = value;
        for (int i = 0; i < crossovers; ++i) {
            auto& state = history[static_cast<std::size_t>(i)];
            state += splitCoef_[static_cast<std::size_t>(i)] * (rest - state);
            if (std::abs(state) < 1e-18f) state = 0.f;
            output[static_cast<std::size_t>(i)] = state;
            rest -= state;
        }
        output[static_cast<std::size_t>(bands - 1)] = rest;
    }
    void updateCoefficients() noexcept {
        attackCoef_ = static_cast<float>(std::exp(-1000.0 /
            (rate_ * std::max(1.0f, params_.attackMs))));
        releaseCoef_ = static_cast<float>(std::exp(-1000.0 /
            (rate_ * std::max(5.0f, params_.releaseMs))));
    }
    void updateBandMask() noexcept {
        cachedLow_ = params_.lowHz;
        cachedHigh_ = params_.highHz;
        for (int k = 0; k < bands; ++k) {
            const float f = centreHz[static_cast<std::size_t>(k)];
            const float lo = unit((std::log2(f / cachedLow_) + 0.25f) * 2.f);
            const float hi = unit((std::log2(cachedHigh_ / f) + 0.25f) * 2.f);
            bandMask_[static_cast<std::size_t>(k)] = lo * hi;
        }
    }
    void updateGainTargets(int channels) noexcept {
        std::array<std::array<float, bands>, 2> strength {};
        for (int c = 0; c < channels; ++c) {
            const float peak = peak_[static_cast<std::size_t>(c)];
            for (int k = 0; k < bands; ++k) {
                const float level = envelope_[static_cast<std::size_t>(c)]
                                            [static_cast<std::size_t>(k)];
                const float relative = level / std::max(peak, 1e-8f);
                const float gate = unit((level - 0.0008f) * 900.f);
                strength[static_cast<std::size_t>(c)][static_cast<std::size_t>(k)]
                    = unit((relative - 0.012f) * 4.5f) * gate;
            }
        }
        for (int k = 0; k < bands; ++k) {
            const float stereoMax = channels == 2
                ? std::max(strength[0][static_cast<std::size_t>(k)],
                           strength[1][static_cast<std::size_t>(k)])
                : strength[0][static_cast<std::size_t>(k)];
            for (int c = 0; c < channels; ++c) {
                auto& row = strength[static_cast<std::size_t>(c)];
                const float original = row[static_cast<std::size_t>(k)];
                const float linked = original + params_.stereoLink * (stereoMax - original);
                // Gentle frequency spread, no frame-based latency; Smooth has
                // an audible effect but does not alter any host parameters.
                const float neighbours = k == 0 ? row[1]
                    : k == bands - 1 ? row[bands - 2]
                    : 0.5f * (row[static_cast<std::size_t>(k - 1)] +
                              row[static_cast<std::size_t>(k + 1)]);
                const float depth = unit(linked * (1.f - 0.55f * params_.smooth) +
                                         neighbours * 0.55f * params_.smooth);
                const float db = maxCutDb * params_.amount * depth *
                                 bandMask_[static_cast<std::size_t>(k)];
                const float target = std::exp(-0.11512925465f * db);
                auto& inc = gainIncrement_[static_cast<std::size_t>(c)]
                                          [static_cast<std::size_t>(k)];
                inc = (target - gain_[static_cast<std::size_t>(c)]
                                    [static_cast<std::size_t>(k)]) /
                      static_cast<float>(gainUpdateSamples);
            }
        }
    }

    double rate_ = 48000.0;
    LiveDuckerParams params_{};
    std::array<float, crossovers> splitCoef_{};
    std::array<std::array<float, crossovers>, 2> mainSplit_{}, sideSplit_{};
    std::array<std::array<float, bands>, 2> envelope_{};
    std::array<std::array<float, bands>, 2> gain_{}, gainIncrement_{};
    std::array<std::array<std::atomic<float>, bands>, 2> displayGain_{};
    std::array<float, bands> bandMask_{};
    std::array<float, 2> peak_{};
    float attackCoef_ = 0.f, releaseCoef_ = 0.f, fadeStep_ = 0.002f,
          peakDecay_ = 0.99999f, wetMix_ = 0.f;
    float cachedLow_ = 0.f, cachedHigh_ = 0.f;
    int gainCountdown_ = 0, meterCountdown_ = 0;
};

} // namespace skellcore::live

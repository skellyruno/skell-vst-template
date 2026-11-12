#pragma once
// JUCE-free UI meter ballistics. Never called on the audio thread.
// All state is bounded and finite; processing performs no allocations.
#include <algorithm>
#include <cmath>

namespace skellcore {

class MeterBallistics {
public:
    struct Reading {
        float level = 0.f;
        float peak = 0.f;
    };

    // These are *UI animation* times, not audio signal processing.
    void configure(float releaseSeconds, float peakHoldSeconds = .30f) noexcept {
        release_ = finitePositive(releaseSeconds) ? std::min(releaseSeconds, 30.f) : .16f;
        hold_ = finitePositive(peakHoldSeconds) ? std::min(peakHoldSeconds, 30.f) : 0.f;
    }

    void reset() noexcept {
        level_ = 0.f;
        peak_ = 0.f;
        holdRemaining_ = 0.f;
    }

    Reading advance(float inputPeak, float deltaSeconds) noexcept {
        const auto input = sanitize(inputPeak);
        const auto dt = finitePositive(deltaSeconds) ? std::min(deltaSeconds, 0.25f) : 0.f;
        const float decay = dt > 0.f ? std::exp(-dt / release_) : 1.f;

        // Attack is immediate to preserve visible transients. The GUI decays
        // between UI frames, independent of the host's audio-block size.
        level_ = input >= level_ ? input : input + (level_ - input) * decay;

        if (input >= peak_ && input > 0.f) {
            peak_ = input;
            holdRemaining_ = hold_;
        } else if (dt > 0.f) {
            // Spend only the frame time *after* the hold expires on decay.
            const float afterHold = std::max(0.f, dt - holdRemaining_);
            holdRemaining_ = std::max(0.f, holdRemaining_ - dt);
            if (afterHold > 0.f) peak_ *= std::exp(-afterHold / release_);
        }
        peak_ = std::max(peak_, level_);
        return { level_, peak_ };
    }

    Reading reading() const noexcept { return { level_, peak_ }; }

private:
    static bool finitePositive(float value) noexcept {
        return std::isfinite(value) && value > 0.f;
    }
    static float sanitize(float value) noexcept {
        return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
    }

    float level_ = 0.f;
    float peak_ = 0.f;
    float release_ = .16f;
    float hold_ = .30f;
    float holdRemaining_ = 0.f;
};

} // namespace skellcore

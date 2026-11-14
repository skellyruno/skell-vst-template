#pragma once
// SkellMovement: deterministic UI-only visual sequencing, adapted from the
// proven visual-only GlitchPulse in SkellGlitch (ui/GlitchPulse.h).
// This pure C++17 header has no JUCE, DSP, audio-thread, timer or allocation
// dependencies. Call tick() on a UI/message-thread timer, normally at 30 Hz.
// Never call tick() from processBlock().
#include <cstdint>
#include <algorithm>

namespace skellcore::ui {

struct SkellMovementConfig {
    // These durations are in UI ticks, not audio samples. At 30 Hz the
    // defaults reproduce SkellGlitch's original quiet and glitch cadence.
    int firstQuietMinTicks = 120;
    int firstQuietSpreadTicks = 151;
    int quietMinTicks = 165;
    int quietSpreadTicks = 196;
    int burstMinTicks = 3;
    int burstSpreadTicks = 3;
    int maximumIntensity = 3;
};

class SkellMovement final {
public:
    static constexpr std::uint32_t defaultSeed = 0x5E11C07u;
    explicit SkellMovement(std::uint32_t seed = defaultSeed,
                           SkellMovementConfig config = {}) noexcept
        : config_(sanitise(config)) { reset(seed); }

    // True only when this tick may change what the editor should paint.
    // Idle ticks return false, so callers can avoid unnecessary repaints.
    bool tick() noexcept {
        if (!enabled_ || reducedMotion_) return false;
        const bool wasActive = remaining_ > 0;
        if (wasActive) {
            --remaining_;
            ++phase_;
        } else if (--quietFrames_ <= 0) {
            seedForBurst_ = next();
            remaining_ = config_.burstMinTicks +
                         static_cast<int>(next() % static_cast<std::uint32_t>(config_.burstSpreadTicks));
            phase_ = 0;
            quietFrames_ = config_.quietMinTicks +
                           static_cast<int>(next() % static_cast<std::uint32_t>(config_.quietSpreadTicks));
            ++burstCount_;
        }
        return wasActive || remaining_ > 0;
    }

    int intensity() const noexcept {
        if (!enabled_ || reducedMotion_ || remaining_ <= 0) return 0;
        return 1 + static_cast<int>((seedForBurst_ + static_cast<std::uint32_t>(phase_))
                                    % static_cast<std::uint32_t>(config_.maximumIntensity));
    }
    std::uint32_t visualSeed() const noexcept {
        return seedForBurst_ ^ (0x9e3779b9u * static_cast<std::uint32_t>(phase_ + 1));
    }
    std::uint32_t bursts() const noexcept { return burstCount_; }
    bool active() const noexcept { return intensity() != 0; }
    bool enabled() const noexcept { return enabled_; }
    bool reducedMotion() const noexcept { return reducedMotion_; }

    bool setEnabled(bool on) noexcept {
        const bool wasVisible = active();
        enabled_ = on;
        return wasVisible != active();
    }
    bool setReducedMotion(bool reduce) noexcept {
        const bool wasVisible = active();
        reducedMotion_ = reduce;
        return wasVisible != active();
    }
    void reset(std::uint32_t seed = defaultSeed) noexcept {
        state_ = seed ? seed : defaultSeed;
        seedForBurst_ = 0;
        burstCount_ = 0;
        remaining_ = 0;
        phase_ = 0;
        quietFrames_ = config_.firstQuietMinTicks +
                       static_cast<int>(next() % static_cast<std::uint32_t>(config_.firstQuietSpreadTicks));
    }
    // Reconfiguring is explicit and restarts scheduling; no global presets
    // or saved plugin automation parameters are added.
    void configure(SkellMovementConfig config, std::uint32_t seed = defaultSeed) noexcept {
        config_ = sanitise(config);
        reset(seed);
    }

private:
    static SkellMovementConfig sanitise(SkellMovementConfig c) noexcept {
        c.firstQuietMinTicks = std::clamp(c.firstQuietMinTicks, 1, 30000);
        c.firstQuietSpreadTicks = std::clamp(c.firstQuietSpreadTicks, 1, 30000);
        c.quietMinTicks = std::clamp(c.quietMinTicks, 1, 30000);
        c.quietSpreadTicks = std::clamp(c.quietSpreadTicks, 1, 30000);
        c.burstMinTicks = std::clamp(c.burstMinTicks, 1, 120);
        c.burstSpreadTicks = std::clamp(c.burstSpreadTicks, 1, 120);
        c.maximumIntensity = std::clamp(c.maximumIntensity, 1, 8);
        return c;
    }
    std::uint32_t next() noexcept {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    SkellMovementConfig config_;
    std::uint32_t state_ = defaultSeed, seedForBurst_ = 0, burstCount_ = 0;
    int quietFrames_ = 0, remaining_ = 0, phase_ = 0;
    bool enabled_ = true, reducedMotion_ = false;
};

} // namespace skellcore::ui

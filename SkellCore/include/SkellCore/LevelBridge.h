#pragma once
// Tiny, portable scalar handoff for UI meters. No JUCE, locks or allocations.
// Only the audio thread publishes; only the message thread consumes.
#include <atomic>
#include <cmath>

namespace skellcore {
class LevelBridge {
public:
    void publish(float peak) noexcept {
        if (!std::isfinite(peak) || peak < 0.f) peak = 0.f;
        if (peak > 1.f) peak = 1.f;
        auto old = latest_.load(std::memory_order_relaxed);
        while (old < peak && !latest_.compare_exchange_weak(
                   old, peak, std::memory_order_relaxed)) {}
    }
    float take() noexcept {
        return latest_.exchange(0.f, std::memory_order_relaxed);
    }
private:
    std::atomic<float> latest_ { 0.f };
};
} // namespace skellcore

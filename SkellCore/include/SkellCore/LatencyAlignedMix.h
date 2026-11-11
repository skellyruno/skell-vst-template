#pragma once
// SkellCore LatencyAlignedMix v1 — shared, sample-accurate dry/wet & bypass.
// Portable C++17, independent of JUCE. Configure on a non-audio thread.
// process() never allocates, locks, resizes, logs or notifies a host.
//
// IMPORTANT: dry must be the ORIGINAL, unprocessed input. The caller must
// preserve it before DSP overwrites the buffer. wet/out may alias each other.
// Report the wet processor's real intrinsic latency to the host separately.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <new>
#include <vector>

namespace skellcore::live {

class LatencyAlignedMix final {
public:
    static constexpr int maxChannels = 8;
    static constexpr int maxLatencySamples = 192000;

    // delaySamples is the WET engine's *actual* output latency.
    // false on invalid configuration or allocation failure, leaving the old
    // prepared configuration untouched. Never call during processBlock().
    bool prepare(double sampleRate, int delaySamples, int channels,
                 double crossfadeMs = 5.0) {
        if (!std::isfinite(sampleRate) || sampleRate < 1000.0
            || sampleRate > 384000.0 || delaySamples < 0
            || delaySamples > maxLatencySamples
            || channels < 1 || channels > maxChannels)
            return false;

        const auto capacity = static_cast<std::size_t>(delaySamples) + 1u;
        const auto count = capacity * static_cast<std::size_t>(channels);
        std::vector<float> next;
        try {
            next.assign(count, 0.0f);
        } catch (const std::bad_alloc&) {
            return false;
        }
        ring_.swap(next);
        capacity_ = capacity;
        delay_ = delaySamples;
        channels_ = channels;
        sampleRate_ = sampleRate;
        writeIndex_ = 0;
        setFadeMs(crossfadeMs);
        currentWet_ = targetWet_;
        prepared_ = true;
        return true;
    }

    // Clears histories. The mix retains its current target; a reset is never
    // a silent mode change or a host PDC change.
    void reset() noexcept {
        std::fill(ring_.begin(), ring_.end(), 0.0f);
        writeIndex_ = 0;
        currentWet_ = targetWet_;
    }

    int latencySamples() const noexcept { return delay_; }
    int preparedChannels() const noexcept { return channels_; }
    bool isPrepared() const noexcept { return prepared_; }
    double sampleRate() const noexcept { return sampleRate_; }
    float currentWetMix() const noexcept { return currentWet_; }
    float targetWetMix() const noexcept { return targetWet_; }

    // Call these from the AUDIO thread, reading APVTS atomics first. Do not
    // mutate concurrently with process() from a GUI thread.
    void setTargetWet(float wet) noexcept { targetWet_ = unit(wet); }
    void setBypassed(bool bypassed) noexcept {
        targetWet_ = bypassed ? 0.0f : 1.0f;
    }

    // For initial bypass/preset restore while not processing, or hard reset.
    // Never use during active playback for regular bypass toggles.
    void setMixImmediately(float wet) noexcept {
        targetWet_ = unit(wet);
        currentWet_ = targetWet_;
    }

    // Pass the wet DSP's output already rendered for this block. dry is the
    // original (pre-effect) block, not the processed wet buffer.
    // out may equal wet, but dry must remain readable and unprocessed.
    // All three pointer arrays must be valid; false leaves output untouched.
    // Every sample advances the dry delay even when fully wet or bypassed.
    bool process(const float* const* dry, const float* const* wet,
                 float* const* out, int channels, int frames) noexcept {
        if (!prepared_ || channels < 1 || channels > channels_ || frames < 0
            || !dry || !wet || !out)
            return false;
        for (int c = 0; c < channels; ++c)
            if (!dry[c] || !wet[c] || !out[c]) return false;
        if (frames == 0) return true;

        for (int i = 0; i < frames; ++i) {
            if (currentWet_ < targetWet_)
                currentWet_ = std::min(targetWet_, currentWet_ + fadeStep_);
            else if (currentWet_ > targetWet_)
                currentWet_ = std::max(targetWet_, currentWet_ - fadeStep_);

            const auto tap = delay_ == 0 ? writeIndex_ :
                (writeIndex_ + capacity_ - static_cast<std::size_t>(delay_)) % capacity_;
            for (int c = 0; c < channels; ++c) {
                const auto index = static_cast<std::size_t>(c) * capacity_;
                const float source = finite(dry[c][i]);
                const float delayed = delay_ == 0 ? source : ring_[index + tap];
                // Read wet before writing out: wet/out may alias.
                const float processed = finite(wet[c][i]);
                ring_[index + writeIndex_] = source;
                float blended;
                if (currentWet_ == 0.0f) blended = delayed;
                else if (currentWet_ == 1.0f) blended = processed;
                else blended = delayed + currentWet_ * (processed - delayed);
                out[c][i] = finite(blended);
            }
            if (++writeIndex_ == capacity_) writeIndex_ = 0u;
        }
        return true;
    }

private:
    static float finite(float value) noexcept {
        return std::isfinite(value) ? value : 0.0f;
    }
    static float unit(float v) noexcept {
        return std::clamp(finite(v), 0.0f, 1.0f);
    }
    void setFadeMs(double milliseconds) noexcept {
        if (!std::isfinite(milliseconds) || milliseconds < 0.0)
            milliseconds = 5.0;
        const double samples = milliseconds * sampleRate_ / 1000.0;
        fadeStep_ = samples < 1.0 ? 1.0f :
            static_cast<float>(1.0 / std::min(samples, 384000.0));
    }

    std::vector<float> ring_;
    std::size_t capacity_ = 0u, writeIndex_ = 0u;
    int delay_ = 0, channels_ = 0;
    double sampleRate_ = 48000.0;
    float currentWet_ = 1.0f, targetWet_ = 1.0f, fadeStep_ = 1.0f;
    bool prepared_ = false;
};

} // namespace skellcore::live

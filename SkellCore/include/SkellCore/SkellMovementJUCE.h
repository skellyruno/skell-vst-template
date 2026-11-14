#pragma once
// Optional JUCE adapter. Own this on the UI/message thread (e.g. as a member
// of your AudioProcessorEditor); starts no timer until start() is called.
// A SafePointer avoids repainting a destroyed editor/component.
// It does not access audio processor state, generate audio or persist anything.
#include <SkellCore/SkellMovement.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace skellcore::ui {
class SkellMovementAnimator final : private juce::Timer {
public:
    explicit SkellMovementAnimator(std::uint32_t seed = SkellMovement::defaultSeed,
                                   SkellMovementConfig config = {}) noexcept
        : movement_(seed, config) {}
    ~SkellMovementAnimator() override { stop(); }
    SkellMovementAnimator(const SkellMovementAnimator&) = delete;
    SkellMovementAnimator& operator=(const SkellMovementAnimator&) = delete;

    void start(juce::Component& target, int ticksPerSecond = 30) {
        target_ = &target;
        startTimer(juce::jmax(8, 1000 / juce::jlimit(1, 120, ticksPerSecond)));
    }
    void stop() noexcept {
        stopTimer();
        target_ = nullptr;
    }
    void setDirtyArea(juce::Rectangle<int> area) noexcept { dirtyArea_ = area; }
    SkellMovement& movement() noexcept { return movement_; }
    const SkellMovement& movement() const noexcept { return movement_; }
    // User's reduced-motion control can disable effects without modifying
    // DAW automation or audio state; immediately clear the visible glitch.
    void setReducedMotion(bool reduce) {
        if (movement_.setReducedMotion(reduce)) repaintTarget();
    }
    bool isRunning() const noexcept { return isTimerRunning(); }

private:
    void repaintTarget() {
        if (auto* target = target_.getComponent()) {
            if (dirtyArea_.isEmpty()) target->repaint();
            else target->repaint(dirtyArea_);
        }
    }
    void timerCallback() override {
        if (target_ == nullptr) { stop(); return; }
        if (movement_.tick()) repaintTarget();
    }
    SkellMovement movement_;
    juce::Component::SafePointer<juce::Component> target_;
    juce::Rectangle<int> dirtyArea_;
};
} // namespace skellcore::ui

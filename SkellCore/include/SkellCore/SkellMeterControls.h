#pragma once
// Shared UI-only stereo meters. Plugins own their sample analysis and may
// publish scalars through the portable LevelBridge on the audio thread.
// This component never reads audio buffers or touches processBlock().
#include <SkellCore/MeterBallistics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace skellcore::ui {

class SkellStereoMeter final : public juce::Component {
public:
    SkellStereoMeter() { setComponentID("skellcore.meter.stereo"); }

    void setColours(juce::Colour fill, juce::Colour track,
                    juce::Colour heldPeak, juce::Colour outline) {
        fill_ = fill; track_ = track;
        heldPeak_ = heldPeak; outline_ = outline;
        repaint();
    }

    void setVertical(bool vertical) {
        if (vertical_ == vertical) return;
        vertical_ = vertical;
        repaint();
    }
    bool isVertical() const noexcept { return vertical_; }

    void configure(float releaseSeconds, float holdSeconds) noexcept {
        left_.configure(releaseSeconds, holdSeconds);
        right_.configure(releaseSeconds, holdSeconds);
    }

    void reset() {
        left_.reset();
        right_.reset();
        repaint();
    }

    // Call from the message thread, typically at 20-30Hz. dt is elapsed
    // seconds; ballistics clamp bad values and do not allocate.
    void pushLevels(float leftPeak, float rightPeak, float dt) {
        const auto oldL = left_.reading(), oldR = right_.reading();
        const auto newL = left_.advance(leftPeak, dt);
        const auto newR = right_.advance(rightPeak, dt);
        if (hasChanged(oldL, newL) || hasChanged(oldR, newR)) repaint();
    }

    skellcore::MeterBallistics::Reading leftReading() const noexcept {
        return left_.reading();
    }
    skellcore::MeterBallistics::Reading rightReading() const noexcept {
        return right_.reading();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(1.f);
        if (bounds.getWidth() < 4.f || bounds.getHeight() < 4.f) return;
        const float gap = juce::jmin(4.f, (vertical_ ? bounds.getWidth() : bounds.getHeight()) * .11f);
        if (vertical_) {
            const auto width = (bounds.getWidth() - gap) * .5f;
            drawBar(g, {bounds.getX(), bounds.getY(), width, bounds.getHeight()},
                    left_.reading(), true);
            drawBar(g, {bounds.getX() + width + gap, bounds.getY(), width, bounds.getHeight()},
                    right_.reading(), true);
        } else {
            const auto height = (bounds.getHeight() - gap) * .5f;
            drawBar(g, {bounds.getX(), bounds.getY(), bounds.getWidth(), height},
                    left_.reading(), false);
            drawBar(g, {bounds.getX(), bounds.getY() + height + gap, bounds.getWidth(), height},
                    right_.reading(), false);
        }
    }

private:
    static bool hasChanged(skellcore::MeterBallistics::Reading a,
                           skellcore::MeterBallistics::Reading b) noexcept {
        // Avoid repainting at 24fps when nothing perceptible changed.
        return std::abs(a.level-b.level) > .002f || std::abs(a.peak-b.peak) > .002f;
    }

    void drawBar(juce::Graphics& g, juce::Rectangle<float> b,
                 skellcore::MeterBallistics::Reading reading, bool vertical) {
        if (b.getWidth() < 1.f || b.getHeight() < 1.f) return;
        const auto radius = juce::jmin(3.f, juce::jmin(b.getWidth(), b.getHeight()) * .23f);
        g.setColour(track_);
        g.fillRoundedRectangle(b, radius);
        g.setColour(outline_);
        g.drawRoundedRectangle(b.reduced(.4f), radius, .8f);

        const float amount = juce::jlimit(0.f, 1.f, reading.level);
        const float marker = juce::jlimit(0.f, 1.f, reading.peak);
        g.setColour(fill_);
        if (vertical) {
            const float h = b.getHeight() * amount;
            if (h >= .5f)
                g.fillRect(b.getX()+1.f, b.getBottom()-h,
                           juce::jmax(0.f, b.getWidth()-2.f), h);
            const auto markerY = b.getBottom() - b.getHeight() * marker;
            if (marker > 0.f) {
                g.setColour(heldPeak_);
                g.drawLine(b.getX()+1.f, markerY, b.getRight()-1.f, markerY, 1.35f);
            }
        } else {
            const float w = b.getWidth() * amount;
            if (w >= .5f)
                g.fillRect(b.getX(), b.getY()+1.f, w,
                           juce::jmax(0.f, b.getHeight()-2.f));
            const auto markerX = b.getX() + b.getWidth() * marker;
            if (marker > 0.f) {
                g.setColour(heldPeak_);
                g.drawLine(markerX, b.getY()+1.f, markerX, b.getBottom()-1.f, 1.35f);
            }
        }
    }

    skellcore::MeterBallistics left_, right_;
    bool vertical_ = false;
    juce::Colour fill_ {0xff90ff44}, track_ {0xff28312b};
    juce::Colour heldPeak_ {0xfff5f6ef}, outline_ {0xff48514a};
};

} // namespace skellcore::ui

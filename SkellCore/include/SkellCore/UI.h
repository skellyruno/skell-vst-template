#pragma once
// JUCE-dependent UI primitives. Include only from editor/UI code, never DSP.
#include <juce_gui_basics/juce_gui_basics.h>

namespace skellcore::ui {
struct Theme {
    juce::Colour background = juce::Colour::fromRGB(9, 14, 12);
    juce::Colour surface = juce::Colour::fromRGB(19, 28, 24);
    juce::Colour accent = juce::Colour::fromRGB(91, 242, 105);
    juce::Colour foreground = juce::Colour::fromRGB(230, 241, 228);
    juce::Colour secondary = juce::Colour::fromRGB(151, 173, 155);
    static Theme neon() { return {}; }
    static Theme ivory() {
        return { juce::Colour::fromRGB(235, 229, 215), juce::Colour::fromRGB(250, 246, 235),
                 juce::Colour::fromRGB(82, 111, 87), juce::Colour::fromRGB(39, 44, 39),
                 juce::Colour::fromRGB(97, 105, 95) };
    }
    static Theme obsidian() {
        return { juce::Colour::fromRGB(12, 12, 14), juce::Colour::fromRGB(29, 29, 33),
                 juce::Colour::fromRGB(201, 213, 217), juce::Colour::fromRGB(235, 237, 240),
                 juce::Colour::fromRGB(151, 156, 166) };
    }
};

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    explicit LookAndFeel(Theme theme = Theme::neon()) : colours_(theme) {
        setColour(juce::Label::textColourId, colours_.foreground);
        setColour(juce::Slider::thumbColourId, colours_.accent);
        setColour(juce::Slider::rotarySliderFillColourId, colours_.accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, colours_.surface);
        setColour(juce::TextButton::buttonColourId, colours_.surface);
        setColour(juce::TextButton::textColourOffId, colours_.foreground);
        setColour(juce::PopupMenu::backgroundColourId, colours_.surface);
        setColour(juce::PopupMenu::textColourId, colours_.foreground);
    }
    Theme palette() const noexcept { return colours_; }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float startAngle, float endAngle, juce::Slider&) override {
        auto b = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(6.0f);
        const auto radius = std::min(b.getWidth(), b.getHeight()) * .5f;
        const auto cx = b.getCentreX(), cy = b.getCentreY();
        g.setColour(colours_.surface);
        g.fillEllipse(cx-radius, cy-radius, radius*2, radius*2);
        juce::Path track, active;
        const auto r = radius * .78f;
        track.addCentredArc(cx,cy,r,r,0,startAngle,endAngle,true);
        active.addCentredArc(cx,cy,r,r,0,startAngle,startAngle+(endAngle-startAngle)*position,true);
        g.setColour(colours_.secondary.withAlpha(.48f)); g.strokePath(track,juce::PathStrokeType(2.2f));
        g.setColour(colours_.accent); g.strokePath(active,juce::PathStrokeType(3.3f));
    }
private:
    Theme colours_;
};

// Render cache on the message/UI thread when size, colour or scale changes.
// paint() only draws a previously prepared image; never regenerate on audio thread.
class GlowCache {
public:
    void rebuild(int width, int height, juce::Colour colour, float radius = 8.f) {
        if (width <= 0 || height <= 0) { image_ = {}; return; }
        image_ = juce::Image(juce::Image::ARGB, width, height, true);
        juce::Graphics g(image_);
        const auto bounds = juce::Rectangle<float>(0,0,(float)width,(float)height).reduced(10);
        for (int layer = 7; layer >= 1; --layer) {
            g.setColour(colour.withAlpha(0.018f * (8-layer)));
            g.drawRoundedRectangle(bounds.expanded((float)layer),radius + layer,1.5f);
        }
    }
    void paint(juce::Graphics& g, int x, int y) const {
        if (image_.isValid()) g.drawImageAt(image_,x,y);
    }
    bool valid() const noexcept { return image_.isValid(); }
private:
    juce::Image image_;
};
} // namespace skellcore::ui

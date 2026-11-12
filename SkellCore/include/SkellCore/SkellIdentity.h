#pragma once
// SkellCore shared identity system. Editor/UI thread only.
// One geometric skull silhouette; a product-specific element changes the
// forehead badge, not the master skull shape. SkellKey's legacy long-key mark
// remains separately available in SkullTheme.h for backwards compatibility.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <utility>

namespace skellcore::ui {

enum class SkullElement {
    Core, Key, Tune, Equalizer, Delay, Silence, Cast, Fan, Master, Custom
};

struct SkellBrandColours {
    juce::Colour bone       { 0xfff1f4ef };
    juce::Colour cutout     { 0xff0b0d0c };
    juce::Colour accent     { 0xff90ff44 };
    juce::Colour secondary  { 0xff7b9485 };
    static SkellBrandColours neon() noexcept { return {}; }
    static SkellBrandColours ivory() noexcept {
        return { juce::Colour(0xff202521), juce::Colour(0xfff2ece1),
                 juce::Colour(0xff437f54), juce::Colour(0xff777a70) };
    }
};

// All coordinates use a fixed 48x48 design space. The same silhouette is
// used for the generic mark and for each element-specific skull.
inline void drawSkellMasterSkull(juce::Graphics& g, const SkellBrandColours& c) {
    juce::Path outline;
    outline.startNewSubPath(24.f, 1.f);
    outline.lineTo(44.f, 12.f); outline.lineTo(44.f, 36.f);
    outline.lineTo(24.f, 47.f); outline.lineTo(4.f, 36.f);
    outline.lineTo(4.f, 12.f); outline.closeSubPath();
    g.setColour(c.bone);
    g.fillPath(outline);
    juce::Path opening;
    opening.startNewSubPath(24.f, 4.4f);
    opening.lineTo(41.f, 14.f); opening.lineTo(41.f, 34.3f);
    opening.lineTo(24.f, 43.6f); opening.lineTo(7.f, 34.3f);
    opening.lineTo(7.f, 14.f); opening.closeSubPath();
    g.setColour(c.cutout);
    g.fillPath(opening);

    // Angular cranium and jaw connected into one compact, legible mark.
    juce::Path skull;
    skull.startNewSubPath(24.f, 13.5f);
    skull.lineTo(36.f, 17.f); skull.lineTo(38.f, 25.f);
    skull.lineTo(34.f, 32.f); skull.lineTo(30.f, 34.f);
    skull.lineTo(30.f, 39.f); skull.lineTo(18.f, 39.f);
    skull.lineTo(18.f, 34.f); skull.lineTo(14.f, 32.f);
    skull.lineTo(10.f, 25.f); skull.lineTo(12.f, 17.f);
    skull.closeSubPath();
    g.setColour(c.bone);
    g.fillPath(skull);
    auto polygon = [&g, &c](std::initializer_list<juce::Point<float>> points) {
        juce::Path shape;
        bool first = true;
        for (auto point : points) {
            if (first) { shape.startNewSubPath(point); first = false; }
            else shape.lineTo(point);
        }
        shape.closeSubPath();
        g.setColour(c.cutout);
        g.fillPath(shape);
    };
    polygon({{12.f,23.f}, {22.f,26.4f}, {19.3f,30.3f}, {15.3f,29.6f}});
    polygon({{36.f,23.f}, {26.f,26.4f}, {28.7f,30.3f}, {32.7f,29.6f}});
    polygon({{24.f,29.f}, {21.f,34.f}, {27.f,34.f}});
    g.setColour(c.cutout);
    for (int i = 0; i < 3; ++i)
        g.fillRect(20.f + i * 3.3f, 35.2f, 1.15f, 4.3f);
}

// This occupies only the forehead. A plugin may provide its own painter in the
// same normalized 48x48 space instead of using an enum preset.
inline void drawSkellElement(juce::Graphics& g, SkullElement element,
                             const SkellBrandColours& c) {
    const auto bright = c.accent;
    g.setColour(bright);
    switch (element) {
        case SkullElement::Core:
        case SkullElement::Custom: break;
        case SkullElement::Key:
            g.drawEllipse(20.f, 8.f, 5.f, 5.f, 1.45f);
            g.fillRect(24.f, 10.f, 6.f, 1.5f);
            g.fillRect(28.5f, 10.f, 1.35f, 3.0f);
            break;
        case SkullElement::Tune: {
            juce::Path note;
            note.startNewSubPath(24.f, 8.f);
            note.lineTo(29.f, 7.f); note.lineTo(29.f, 12.2f);
            g.strokePath(note, juce::PathStrokeType(1.6f));
            g.fillEllipse(25.5f, 11.f, 3.4f, 2.5f);
            break;
        }
        case SkullElement::Equalizer:
            g.fillRect(19.f, 10.f, 2.f, 4.f);
            g.fillRect(23.f, 7.5f, 2.f, 6.5f);
            g.fillRect(27.f, 9.f, 2.f, 5.f);
            break;
        case SkullElement::Delay:
            g.drawEllipse(20.1f, 7.1f, 7.8f, 7.8f, 1.5f);
            g.drawLine(24.f, 9.f, 24.f, 11.f, 1.3f);
            g.drawLine(24.f, 11.f, 26.f, 12.f, 1.3f);
            break;
        case SkullElement::Silence:
            g.drawLine(24.f, 8.f, 24.f, 14.f, 1.8f);
            g.fillEllipse(22.8f, 6.6f, 2.5f, 2.5f);
            break;
        case SkullElement::Cast: {
            g.fillEllipse(22.9f, 11.f, 2.2f, 2.2f);
            juce::Path inner, outer;
            inner.addCentredArc(24.f, 12.f, 3.6f, 3.6f, 0.f,
                                -.9f, .9f, true);
            outer.addCentredArc(24.f, 12.f, 5.6f, 5.6f, 0.f,
                                -.9f, .9f, true);
            g.strokePath(inner, juce::PathStrokeType(1.25f));
            g.strokePath(outer, juce::PathStrokeType(1.0f));
            break;
        }
        case SkullElement::Fan:
            g.fillEllipse(22.9f, 9.5f, 2.4f, 2.4f);
            for (int i = 0; i < 3; ++i) {
                const auto angle = float(i) * juce::MathConstants<float>::twoPi / 3.f;
                g.fillEllipse(24.f + std::cos(angle) * 3.f - 1.5f,
                              10.7f + std::sin(angle) * 3.f - 1.5f, 3.f, 3.f);
            }
            break;
        case SkullElement::Master:
            g.drawLine(19.f, 13.f, 29.f, 13.f, 1.6f);
            g.drawLine(21.f, 10.f, 24.f, 7.f, 1.4f);
            g.drawLine(24.f, 7.f, 27.f, 10.f, 1.4f);
            break;
    }
}

using SkellCustomElement = std::function<void(juce::Graphics&)>;

inline void drawSkellSkull(juce::Graphics& g, juce::Rectangle<float> bounds,
                          SkullElement element = SkullElement::Core,
                          const SkellBrandColours& colours = SkellBrandColours::neon(),
                          const SkellCustomElement& customElement = {}) {
    if (bounds.getWidth() <= 0.f || bounds.getHeight() <= 0.f) return;
    juce::Graphics::ScopedSaveState saved(g);
    const auto scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 48.f;
    const auto left = bounds.getCentreX() - scale * 24.f;
    const auto top = bounds.getCentreY() - scale * 24.f;
    g.addTransform(juce::AffineTransform::scale(scale).translated(left, top));
    drawSkellMasterSkull(g, colours);
    if (element == SkullElement::Custom && customElement)
        customElement(g);
    else
        drawSkellElement(g, element, colours);
}

// Interactive, reusable hit target. Caller supplies a real onClick action:
// theme switching, settings, navigation, etc. No behavior is assumed.
class SkellSkullButton final : public juce::Button {
public:
    SkellSkullButton() : juce::Button("Skell skull") {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setComponentID("skellcore.identity.skull");
    }
    void setElement(SkullElement e) { element_ = e; repaint(); }
    SkullElement element() const noexcept { return element_; }
    void setBrandColours(const SkellBrandColours& colours) {
        colours_ = colours; repaint();
    }
    void setCustomElement(SkellCustomElement painter) {
        custom_ = std::move(painter);
        element_ = SkullElement::Custom;
        repaint();
    }
    void paintButton(juce::Graphics& g, bool, bool) override {
        drawSkellSkull(g, getLocalBounds().toFloat(), element_, colours_, custom_);
    }
private:
    SkullElement element_ = SkullElement::Core;
    SkellBrandColours colours_;
    SkellCustomElement custom_;
};

} // namespace skellcore::ui

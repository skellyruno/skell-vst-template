#pragma once
// SKELL Studio UI v1: opt-in shared 3D knobs and four coordinated plugin skins.
// UI/message-thread only. Never call skin storage or painting from processBlock().
// No plugin IDs, parameter IDs, DSP state or audio processing are changed.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skellcore::ui {

enum class StudioSkin : std::uint8_t {
    Ivory = 0, Obsidian = 1, Amethyst = 2, Crimson = 3
};

inline StudioSkin studioSkinFromIndex(int index) noexcept {
    return index >= 0 && index <= 3 ? static_cast<StudioSkin>(index) : StudioSkin::Ivory;
}
inline const char* studioSkinName(StudioSkin skin) noexcept {
    switch (skin) {
        case StudioSkin::Ivory: return "Ivory";
        case StudioSkin::Obsidian: return "Obsidian";
        case StudioSkin::Amethyst: return "Amethyst";
        case StudioSkin::Crimson: return "Crimson";
    }
    return "Ivory";
}
inline StudioSkin studioSkinFromName(const juce::String& name) noexcept {
    if (name.equalsIgnoreCase("obsidian")) return StudioSkin::Obsidian;
    if (name.equalsIgnoreCase("amethyst")) return StudioSkin::Amethyst;
    if (name.equalsIgnoreCase("crimson")) return StudioSkin::Crimson;
    return StudioSkin::Ivory;
}

struct StudioPalette {
    juce::Colour background, surface, raised, foreground, secondary;
    juce::Colour accent, track, border, metalLight, metalDark;
};
inline StudioPalette studioPalette(StudioSkin skin) noexcept {
    using C = juce::Colour;
    switch (skin) {
        case StudioSkin::Obsidian:
            return { C(0xff111315), C(0xff202326), C(0xff303337), C(0xffefefed),
                     C(0xffb0b4b4), C(0xff61cf84), C(0xff454b49), C(0xff555a5b),
                     C(0xffcbd0ce), C(0xff393e41) };
        case StudioSkin::Amethyst:
            return { C(0xff17131e), C(0xff292331), C(0xff3b3344), C(0xfff1eaf4),
                     C(0xffbdb1c5), C(0xffb58cdb), C(0xff53465b), C(0xff65586b),
                     C(0xffd9cddd), C(0xff51475a) };
        case StudioSkin::Crimson:
            return { C(0xff1b1416), C(0xff2e2427), C(0xff413235), C(0xfff4e9e6),
                     C(0xffc7afaa), C(0xffd47b78), C(0xff594447), C(0xff705656),
                     C(0xffdfcfca), C(0xff5d4443) };
        case StudioSkin::Ivory:
        default:
            return { C(0xffe7e0d2), C(0xfff3eee3), C(0xfffffaf0), C(0xff242b28),
                     C(0xff646e68), C(0xff3cae72), C(0xffb9b8ab), C(0xffaaa999),
                     C(0xfff6f7f2), C(0xff797e79) };
    }
}

// Shared user-level appearance preference, not a DAW parameter or preset value.
// All plugins using this class read the same per-user file; writing is explicit
// on user interaction, on the message thread only. Failing to save is non-fatal.
class StudioSkinStore {
public:
    static juce::File file() {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SkellAudio").getChildFile("studio-skin-v1.txt");
    }
    static StudioSkin load() {
        const auto f = file();
        return f.existsAsFile() ? studioSkinFromName(f.loadFileAsString().trim())
                                : StudioSkin::Ivory;
    }
    static bool save(StudioSkin skin) {
        const auto f = file();
        return f.getParentDirectory().createDirectory().wasOk()
            && f.replaceWithText(juce::String(studioSkinName(skin)) + "\n");
    }
};

inline void populateStudioSkinSelector(juce::ComboBox& selector) {
    selector.clear(juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        selector.addItem(studioSkinName(studioSkinFromIndex(i)), i + 1);
}

// Opt-in interaction setup: does NOT set range, attachment, parameter ID,
// default audio parameter value, or make a new DSP/automation pathway.
// Editing the text box and double-click reset use JUCE's normal Slider API.
inline void configureStudioKnob(juce::Slider& slider, double resetValue,
                                int dragSensitivity = 260) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 22);
    slider.setDoubleClickReturnValue(true,
        juce::jlimit(slider.getMinimum(), slider.getMaximum(), resetValue));
    slider.setMouseDragSensitivity(juce::jmax(80, dragSensitivity));
    slider.setScrollWheelEnabled(true);
    slider.setTooltip("Drag to adjust, double-click to reset, or type a value");
}

class StudioLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    explicit StudioLookAndFeel(StudioSkin skin = StudioSkin::Ivory) { setSkin(skin); }
    StudioSkin skin() const noexcept { return skin_; }
    StudioPalette palette() const noexcept { return colours_; }

    // After calling this, reattach the editor's LookAndFeel (or notify its
    // children) and repaint on the GUI thread to refresh all controls.
    void setSkin(StudioSkin skin) {
        skin_ = skin;
        colours_ = studioPalette(skin);
        setColour(juce::Label::textColourId, colours_.foreground);
        setColour(juce::Slider::thumbColourId, colours_.accent);
        setColour(juce::Slider::rotarySliderFillColourId, colours_.accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, colours_.track);
        setColour(juce::Slider::textBoxTextColourId, colours_.foreground);
        setColour(juce::Slider::textBoxBackgroundColourId, colours_.surface);
        setColour(juce::Slider::textBoxOutlineColourId, colours_.border);
        setColour(juce::TextButton::buttonColourId, colours_.raised);
        setColour(juce::TextButton::buttonOnColourId, colours_.accent);
        setColour(juce::TextButton::textColourOffId, colours_.foreground);
        setColour(juce::TextButton::textColourOnId, colours_.background);
        setColour(juce::ComboBox::backgroundColourId, colours_.surface);
        setColour(juce::ComboBox::outlineColourId, colours_.border);
        setColour(juce::ComboBox::textColourId, colours_.foreground);
        setColour(juce::ComboBox::arrowColourId, colours_.accent);
        setColour(juce::PopupMenu::backgroundColourId, colours_.surface);
        setColour(juce::PopupMenu::textColourId, colours_.foreground);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, colours_.accent);
        setColour(juce::PopupMenu::highlightedTextColourId, colours_.background);
        setColour(juce::TextEditor::backgroundColourId, colours_.surface);
        setColour(juce::TextEditor::textColourId, colours_.foreground);
        setColour(juce::TextEditor::outlineColourId, colours_.border);
        setColour(juce::ToggleButton::textColourId, colours_.foreground);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float startAngle, float endAngle,
                          juce::Slider& slider) override {
        if (width < 12 || height < 12) return;
        const float p = std::isfinite(position) ? juce::jlimit(0.0f, 1.0f, position) : 0.0f;
        const float size = static_cast<float>(std::min(width, height));
        const float r = size * 0.37f;
        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f - size * 0.012f;
        const float angle = startAngle + p * (endAngle - startAngle);
        const auto accent = slider.isEnabled() ? colours_.accent : colours_.secondary;

        // Recessed cavity, metal bezel and restrained shadow: no bloom textures.
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        g.fillEllipse(cx - r - 3.0f, cy - r + 3.5f, 2 * r + 6.0f, 2 * r + 6.0f);
        g.setColour(colours_.border);
        g.fillEllipse(cx - r - 3.0f, cy - r - 3.0f, 2 * r + 6.0f, 2 * r + 6.0f);
        g.setColour(colours_.background);
        g.fillEllipse(cx - r, cy - r, 2 * r, 2 * r);

        const float trackRadius = r * 0.91f;
        juce::Path base, active;
        base.addCentredArc(cx, cy, trackRadius, trackRadius, 0.0f,
                           startAngle, endAngle, true);
        active.addCentredArc(cx, cy, trackRadius, trackRadius, 0.0f,
                             startAngle, angle, true);
        g.setColour(colours_.track);
        g.strokePath(base, juce::PathStrokeType(juce::jmax(1.8f, size * 0.026f),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(accent.withAlpha(slider.isEnabled() ? 0.95f : 0.3f));
        g.strokePath(active, juce::PathStrokeType(juce::jmax(2.0f, size * 0.036f),
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Radial metal face with a fine machined lip, slightly raised.
        const float face = r * 0.77f;
        g.setColour(colours_.metalDark);
        g.fillEllipse(cx-face-1.0f, cy-face-1.0f, face*2+2.0f, face*2+2.0f);
        g.setGradientFill(juce::ColourGradient(
            colours_.metalLight, cx-face*0.36f, cy-face*0.53f,
            colours_.metalDark, cx+face*0.83f, cy+face*0.87f, true));
        g.fillEllipse(cx-face, cy-face, 2.0f*face, 2.0f*face);
        g.setColour(colours_.metalLight.withAlpha(0.45f));
        g.drawEllipse(cx-face+2.0f, cy-face+2.0f,
                      2.0f*(face-2.0f), 2.0f*(face-2.0f), 0.75f);
        g.setColour(colours_.metalDark.withAlpha(0.5f));
        g.drawEllipse(cx-face+4.0f, cy-face+4.0f,
                      2.0f*(face-4.0f), 2.0f*(face-4.0f), 0.5f);

        // One high-contrast position marker: readable at 70px and 4K.
        const auto inner = face * 0.53f, outer = face * 0.87f;
        const float dx = std::sin(angle), dy = -std::cos(angle);
        g.setColour(colours_.background.withAlpha(0.75f));
        g.drawLine(cx + dx*inner + 1.0f, cy + dy*inner + 1.0f,
                   cx + dx*outer + 1.0f, cy + dy*outer + 1.0f, juce::jmax(2.0f, size*0.027f));
        g.setColour(slider.isEnabled() ? colours_.foreground : colours_.secondary);
        g.drawLine(cx + dx*inner, cy + dy*inner,
                   cx + dx*outer, cy + dy*outer, juce::jmax(2.0f, size*0.027f));

        // Centre bearing. Value remains visible in the editable JUCE text box.
        const float hub = juce::jmax(2.1f, size * 0.038f);
        g.setColour(colours_.metalDark);
        g.fillEllipse(cx-hub, cy-hub, hub*2.0f, hub*2.0f);
        g.setColour(colours_.metalLight);
        g.fillEllipse(cx-hub*0.45f, cy-hub*0.45f, hub*0.9f, hub*0.9f);
    }

private:
    StudioSkin skin_ = StudioSkin::Ivory;
    StudioPalette colours_ = studioPalette(StudioSkin::Ivory);
};

} // namespace skellcore::ui

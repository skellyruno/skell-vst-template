#pragma once
// Common JUCE controls for Skell plugin editors. These have no audio-thread
// timers, host menus, or implicit DSP state. Bind power to an actual bypass
// parameter with a JUCE ButtonAttachment; use a custom plugin settings page.
#include <SkellCore/SkellIdentity.h>
#include <SkellCore/SkellHardwareUI.h>
#include <SkellCore/StudioUI.h>
#include <SkellCore/LevelBridge.h>
#include <array>
#include <atomic>
#include <cmath>

namespace skellcore::ui {

// Shared metal dial finish without rewriting the existing StudioUI class.
// The plugin chooses the skin, but the control geometry is consistent.
class SkellHardwareLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    explicit SkellHardwareLookAndFeel(StudioSkin skin = StudioSkin::Obsidian) {
        setSkin(skin);
    }
    void setSkin(StudioSkin skin) {
        skin_ = skin;
        palette_ = studioPalette(skin);
        setColour(juce::Label::textColourId, palette_.foreground);
        setColour(juce::Slider::textBoxTextColourId, palette_.foreground);
        setColour(juce::Slider::textBoxBackgroundColourId, palette_.surface);
        setColour(juce::Slider::textBoxOutlineColourId, palette_.border);
        setColour(juce::TextButton::buttonColourId, palette_.raised);
        setColour(juce::TextButton::textColourOffId, palette_.foreground);
        setColour(juce::TextButton::buttonOnColourId, palette_.accent);
        setColour(juce::TextButton::textColourOnId, palette_.background);
        setColour(juce::ComboBox::backgroundColourId, palette_.surface);
        setColour(juce::ComboBox::textColourId, palette_.foreground);
        setColour(juce::ComboBox::outlineColourId, palette_.border);
        setColour(juce::PopupMenu::backgroundColourId, palette_.surface);
        setColour(juce::PopupMenu::textColourId, palette_.foreground);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, palette_.accent);
        setColour(juce::PopupMenu::highlightedTextColourId, palette_.background);
    }
    StudioSkin skin() const noexcept { return skin_; }
    StudioPalette palette() const noexcept { return palette_; }
    // Normal JUCE ToggleButtons match the same compact status-dot language
    // as SKELL effect slots and settings; the dedicated power glyph keeps its
    // own painter. The plugin controls toggle semantics via APVTS as before.
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool hover, bool pressed) override {
        const auto bounds = button.getLocalBounds().toFloat();
        if (bounds.getHeight() < 12.f || bounds.getWidth() < 20.f) return;
        const auto diameter = juce::jmin(15.f, bounds.getHeight() - 5.f);
        const auto x = bounds.getX() + 2.f;
        const auto y = bounds.getCentreY() - diameter * .5f;
        const auto lit = button.getToggleState();
        const auto activeColour = button.isEnabled()
            ? (lit ? palette_.accent : palette_.secondary)
            : palette_.secondary.withAlpha(.35f);
        g.setColour(palette_.background);
        g.fillEllipse(x-1.f, y-1.f, diameter+2.f, diameter+2.f);
        g.setColour(palette_.border);
        g.drawEllipse(x-1.f, y-1.f, diameter+2.f, diameter+2.f, 1.f);
        g.setColour(activeColour.withAlpha(pressed ? .7f : 1.f));
        g.fillEllipse(x+2.f, y+2.f, diameter-4.f, diameter-4.f);
        if (hover && button.isEnabled()) {
            g.setColour(activeColour.withAlpha(.45f));
            g.drawEllipse(x-2.f, y-2.f, diameter+4.f, diameter+4.f, 1.f);
        }
        const auto textStart = juce::roundToInt(x+diameter+8.f);
        const auto textWidth = juce::jmax(0, button.getWidth()-textStart-2);
        g.setColour(button.isEnabled() ? palette_.foreground : palette_.secondary);
        g.setFont(juce::Font(juce::FontOptions(
            juce::jmin(12.f, bounds.getHeight()*.43f))));
        g.drawFittedText(button.getButtonText(), textStart, 0, textWidth,
                         button.getHeight(), juce::Justification::centredLeft, 1);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float position, float begin, float end,
                          juce::Slider& slider) override {
        if (w < 20 || h < 20) return;
        drawHardwareKnob(g, juce::Rectangle<float>(float(x), float(y),
             float(w), float(h)), position, begin, end,
             slider.isEnabled() ? palette_.accent : palette_.secondary,
             palette_.metalDark, palette_.border,
             slider.isEnabled() ? palette_.foreground : palette_.secondary);
    }
private:
    StudioSkin skin_ = StudioSkin::Obsidian;
    StudioPalette palette_ = studioPalette(StudioSkin::Obsidian);
};

// Standard power toggle. Deliberately does NOT set a bypass state itself.
// APVTS::ButtonAttachment preserves host automation and sessions.
class SkellPowerButton final : public juce::ToggleButton {
public:
    SkellPowerButton() : juce::ToggleButton("Power") {
        setClickingTogglesState(true);
        setComponentID("skellcore.control.power");
        setTooltip("Enable or bypass this plugin");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void setColours(juce::Colour accent, juce::Colour muted,
                    juce::Colour face) {
        accent_ = accent; muted_ = muted; face_ = face; repaint();
    }
    void paintButton(juce::Graphics& g, bool hover, bool pressed) override {
        const auto b = getLocalBounds().toFloat().reduced(2.f);
        const auto side = juce::jmin(b.getWidth(), b.getHeight());
        if (side < 8.f) return;
        const auto center = b.getCentre();
        const auto radius = side * 0.34f;
        g.setColour(face_);
        g.fillEllipse(center.x - radius - 3.f, center.y - radius - 3.f,
                      radius * 2.f + 6.f, radius * 2.f + 6.f);
        const auto colour = getToggleState() ? accent_ : muted_;
        g.setColour(colour.withAlpha(isEnabled() ? (pressed ? 0.7f : 1.f) : 0.35f));
        juce::Path arc;
        arc.addCentredArc(center.x, center.y, radius * .70f, radius * .70f,
                         0.f, 0.42f, juce::MathConstants<float>::twoPi - .42f, true);
        g.strokePath(arc, juce::PathStrokeType(juce::jmax(1.8f, side * .073f),
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        g.drawLine(center.x, center.y - radius * 0.97f,
                   center.x, center.y + radius * 0.12f,
                   juce::jmax(1.8f, side * .073f));
        if (hover && isEnabled()) {
            g.setColour(colour.withAlpha(.15f));
            g.drawEllipse(center.x - radius - 1.f, center.y - radius - 1.f,
                          2.f * radius + 2.f, 2.f * radius + 2.f, 1.5f);
        }
    }
private:
    juce::Colour accent_ { 0xff90ff44 }, muted_ { 0xff7b9485 };
    juce::Colour face_ { 0xff141916 };
};

// Opens the plugin's custom settings page; never invokes the DAW host's
// generic settings or creates a popup automatically.
class SkellSettingsButton final : public juce::Button {
public:
    SkellSettingsButton() : juce::Button("Settings") {
        setComponentID("skellcore.control.settings");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip("Open this plugin's settings");
    }
    void setColours(juce::Colour accent, juce::Colour background) {
        accent_ = accent; background_ = background; repaint();
    }
    void paintButton(juce::Graphics& g, bool hover, bool pressed) override {
        auto area = getLocalBounds().toFloat().reduced(2.f);
        const auto size = juce::jmin(area.getWidth(), area.getHeight());
        if (size < 8.f) return;
        const auto p = area.getCentre();
        g.setColour(background_);
        g.fillRoundedRectangle(juce::Rectangle<float>(size, size)
            .withCentre(p), size * .23f);
        g.setColour(accent_.withAlpha(isEnabled() ? (pressed ? .7f : 1.f) : .35f));
        const float r = size * .23f;
        g.drawEllipse(p.x-r, p.y-r, r*2.f, r*2.f, juce::jmax(1.3f, size*.065f));
        g.fillEllipse(p.x-r*.28f, p.y-r*.28f, r*.56f, r*.56f);
        for (int i = 0; i < 8; ++i) {
            const float a = float(i) * juce::MathConstants<float>::twoPi / 8.f;
            const float dx = std::sin(a), dy = std::cos(a);
            g.drawLine(p.x + dx*r*1.12f, p.y + dy*r*1.12f,
                       p.x + dx*r*1.54f, p.y + dy*r*1.54f,
                       juce::jmax(1.5f, size*.072f));
        }
        if (hover && isEnabled()) {
            g.setColour(accent_.withAlpha(.14f));
            g.drawRoundedRectangle(juce::Rectangle<float>(size, size)
                .withCentre(p), size*.23f, 1.f);
        }
    }
private:
    juce::Colour accent_ { 0xff90ff44 }, background_ { 0xff141916 };
};

// Content-first panel: plugin editor owns the actual controls inside it.
// No fake universal plugin settings (latency, input, scales, etc.).
class SkellSettingsSurface final : public juce::Component {
public:
    explicit SkellSettingsSurface(juce::String title = "PLUGIN SETTINGS")
        : title_(std::move(title)) {
        setComponentID("skellcore.surface.plugin-settings");
    }
    void setColours(juce::Colour background, juce::Colour border,
                    juce::Colour text, juce::Colour accent) {
        background_ = background; border_ = border;
        text_ = text; accent_ = accent; repaint();
    }
    void setTitle(juce::String title) { title_ = std::move(title); repaint(); }
    juce::Rectangle<int> contentBounds() const {
        return getLocalBounds().reduced(12).withTrimmedTop(30);
    }
    void paint(juce::Graphics& g) override {
        const auto b = getLocalBounds().toFloat().reduced(.8f);
        g.setColour(background_);
        g.fillRoundedRectangle(b, 9.f);
        g.setColour(border_);
        g.drawRoundedRectangle(b, 9.f, 1.1f);
        g.setColour(accent_);
        g.fillRoundedRectangle(12.f, 16.f, 3.f, 17.f, 1.5f);
        g.setFont(juce::Font(juce::FontOptions(13.f).withStyle("Bold")));
        g.setColour(text_);
        g.drawFittedText(title_, 22, 14, getWidth()-32, 20,
                         juce::Justification::centredLeft, 1);
    }
private:
    juce::String title_;
    juce::Colour background_ { 0xff141916 }, border_ { 0xff344236 };
    juce::Colour text_ { 0xfff1f4ef }, accent_ { 0xff90ff44 };
};

// Shared low-cost audio visual line. Call pushLevel() on the UI thread at
// 20-30 Hz using SkellLevelBridge::take(). No audio thread painting.
class SkellAudioLine final : public juce::Component {
public:
    void setColours(juce::Colour line, juce::Colour baseline) {
        line_ = line; baseline_ = baseline; repaint();
    }
    void pushLevel(float peak) {
        const auto level = std::isfinite(peak) ? juce::jlimit(0.f, 1.f, peak) : 0.f;
        // An already-flat line does not need redraws on an idle audio stream.
        // After an active signal, still advance zeros until the old trace
        // scrolls off-screen instead of leaving a frozen waveform behind.
        if (level <= 0.f && nonzero_ == 0) return;
        auto& slot = values_[write_];
        if (slot > 0.f) --nonzero_;
        slot = level;
        if (slot > 0.f) ++nonzero_;
        write_ = (write_ + 1) % values_.size();
        repaint();
    }
    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(1.f);
        if (b.isEmpty()) return;
        const float center = b.getCentreY();
        g.setColour(baseline_);
        g.drawHorizontalLine(juce::roundToInt(center), b.getX(), b.getRight());
        juce::Path line;
        for (std::size_t i = 0; i < values_.size(); ++i) {
            const auto v = values_[(write_ + i) % values_.size()];
            const auto x = b.getX() + float(i) * b.getWidth() / float(values_.size() - 1);
            const auto y = center - v * b.getHeight() * .46f;
            if (i == 0) line.startNewSubPath(x, y);
            else line.lineTo(x, y);
        }
        g.setColour(line_);
        g.strokePath(line, juce::PathStrokeType(juce::jmax(.85f, b.getHeight()*.09f)));
    }
private:
    std::array<float, 64> values_ {};
    std::size_t write_ = 0;
    std::size_t nonzero_ = 0;
    juce::Colour line_ { 0xff90ff44 }, baseline_ { 0xff344236 };
};

} // namespace skellcore::ui

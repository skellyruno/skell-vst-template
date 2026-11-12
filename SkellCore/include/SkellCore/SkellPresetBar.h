#pragma once
// Reusable preset *UI*, not a preset-storage or DAW program manager.
// No parameter IDs, bank format, disk IO, or host state assumptions.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace skellcore::ui {

class SkellPresetBar final : public juce::Component {
public:
    SkellPresetBar() : previous_("<"), next_(">") {
        setComponentID("skellcore.presets.bar");
        label_.setText("PRESET", juce::dontSendNotification);
        label_.setJustificationType(juce::Justification::centredLeft);
        label_.setInterceptsMouseClicks(false, false);
        label_.setComponentID("skellcore.presets.label");
        selector_.setComponentID("skellcore.presets.selector");
        selector_.setTextWhenNothingSelected("Choose preset");
        selector_.setTooltip("Choose a preset from this plugin");
        previous_.setComponentID("skellcore.presets.previous");
        next_.setComponentID("skellcore.presets.next");
        previous_.setTooltip("Previous preset");
        next_.setTooltip("Next preset");
        previous_.onClick = [this] { selectPreset(selectedIndex_ - 1, true); };
        next_.onClick = [this] { selectPreset(selectedIndex_ + 1, true); };
        selector_.onChange = [this] {
            const int index = selector_.getSelectedId() - 1;
            if (index >= 0 && index < names_.size())
                selectPreset(index, true);
        };
        addAndMakeVisible(label_);
        addAndMakeVisible(previous_);
        addAndMakeVisible(selector_);
        addAndMakeVisible(next_);
        updateNavigation();
    }

    // On user selection, the owner must load its own real preset.
    // This class deliberately does not synthesize or write preset data.
    std::function<void(int)> onPresetSelected;

    void setPresets(const juce::StringArray& names, int selectedIndex = -1) {
        names_ = names;
        selector_.clear(juce::dontSendNotification);
        for (int i = 0; i < names_.size(); ++i) {
            auto name = names_[i].trim();
            if (name.isEmpty()) name = "Preset " + juce::String(i+1);
            selector_.addItem(name, i+1);
        }
        selectedIndex_ = isValid(selectedIndex) ? selectedIndex : -1;
        selector_.setSelectedId(selectedIndex_ + 1, juce::dontSendNotification);
        updateNavigation();
    }

    bool selectPreset(int index, bool notifyOwner = false) {
        if (!isValid(index) || index == selectedIndex_) return false;
        selectedIndex_ = index;
        selector_.setSelectedId(index+1, juce::dontSendNotification);
        updateNavigation();
        if (notifyOwner && onPresetSelected) onPresetSelected(index);
        return true;
    }
    int selectedIndex() const noexcept { return selectedIndex_; }
    int presetCount() const noexcept { return names_.size(); }

    void setModified(bool value) {
        if (modified_ == value) return;
        modified_ = value;
        repaint();
    }
    bool isModified() const noexcept { return modified_; }

    juce::ComboBox& selector() noexcept { return selector_; }
    juce::TextButton& previousButton() noexcept { return previous_; }
    juce::TextButton& nextButton() noexcept { return next_; }

    void resized() override {
        const auto bounds = getLocalBounds().reduced(2);
        const int h = bounds.getHeight();
        const int gap = 4;
        const bool showButtons = bounds.getWidth() >= 182;
        const bool showLabel = bounds.getWidth() >= 285;
        label_.setVisible(showLabel);
        previous_.setVisible(showButtons);
        next_.setVisible(showButtons);
        int x = bounds.getX();
        if (showLabel) {
            const int labelW = 52;
            label_.setBounds(x, bounds.getY(), labelW, h);
            x += labelW + gap;
        }
        if (showButtons) {
            previous_.setBounds(x, bounds.getY(), 28, h);
            x += 28 + gap;
        }
        int remaining = bounds.getRight() - x - (showButtons ? 28+gap : 0);
        if (modified_) remaining -= 12;
        selector_.setBounds(x, bounds.getY(), juce::jmax(0, remaining), h);
        if (showButtons)
            next_.setBounds(selector_.getRight()+gap, bounds.getY(), 28, h);
    }

    void paintOverChildren(juce::Graphics& g) override {
        if (!modified_) return;
        g.setColour(findColour(juce::TextButton::buttonOnColourId, true));
        g.fillEllipse(float(getWidth()-12), float(getHeight()/2-3), 6.f, 6.f);
    }

private:
    bool isValid(int index) const noexcept {
        return index >= 0 && index < names_.size();
    }

    void updateNavigation() {
        selector_.setEnabled(!names_.isEmpty());
        previous_.setEnabled(isValid(selectedIndex_-1));
        next_.setEnabled(isValid(selectedIndex_+1));
    }

    juce::StringArray names_;
    int selectedIndex_ = -1;
    bool modified_ = false;
    juce::Label label_;
    juce::TextButton previous_, next_;
    juce::ComboBox selector_;
};

} // namespace skellcore::ui

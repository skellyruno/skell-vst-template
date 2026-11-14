#pragma once
// Shared menu/text-input palette. No native DAW dialogs, no global skin and
// no automatic menu creation: the plugin explicitly chooses colours.
// Can be used by the plugin's own LookAndFeel or the SkellHardware default.
#include <SkellCore/StudioUI.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace skellcore::ui {
inline juce::Colour skellMenuSelectionText(const StudioPalette& palette) noexcept {
    return palette.accent.getPerceivedBrightness() >= 0.55f
        ? juce::Colours::black : juce::Colours::white;
}
inline void applySkellMenuPalette(juce::LookAndFeel& look,
                                 const StudioPalette& palette) {
    look.setColour(juce::PopupMenu::backgroundColourId, palette.surface);
    look.setColour(juce::PopupMenu::textColourId, palette.foreground);
    look.setColour(juce::PopupMenu::headerTextColourId, palette.accent);
    look.setColour(juce::PopupMenu::highlightedBackgroundColourId, palette.accent);
    look.setColour(juce::PopupMenu::highlightedTextColourId,
                   skellMenuSelectionText(palette));
    look.setColour(juce::ComboBox::backgroundColourId, palette.surface);
    look.setColour(juce::ComboBox::textColourId, palette.foreground);
    look.setColour(juce::ComboBox::outlineColourId, palette.border);
    look.setColour(juce::ComboBox::arrowColourId, palette.accent);
    look.setColour(juce::TextEditor::backgroundColourId, palette.surface);
    look.setColour(juce::TextEditor::textColourId, palette.foreground);
    look.setColour(juce::TextEditor::outlineColourId, palette.border);
    look.setColour(juce::TextEditor::focusedOutlineColourId, palette.accent);
    look.setColour(juce::TextEditor::highlightColourId,
                   palette.accent.withAlpha(0.30f));
    look.setColour(juce::Label::textColourId, palette.foreground);
}
inline void styleSkellPopup(juce::PopupMenu& menu, juce::LookAndFeel& look) {
    // The LookAndFeel must outlive any asynchronously shown popup.
    menu.setLookAndFeel(&look);
}
} // namespace skellcore::ui

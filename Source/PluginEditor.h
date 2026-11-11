#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <SkellCore/StudioUI.h>
#include <SkellCore/WorldLink.h>

class SkellAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
  explicit SkellAudioProcessorEditor(SkellAudioProcessor&);
  ~SkellAudioProcessorEditor() override;
  void paint(juce::Graphics&) override;
  void resized() override;

  // UI-only theme switch for the shared headless renderer and any host editor.
  // persist=false permits repeatable screenshot tests without changing preferences.
  void setStudioSkin(skellcore::ui::StudioSkin skin, bool persist = false);
  skellcore::ui::StudioSkin studioSkin() const noexcept { return skin_.skin(); }

private:
  void timerCallback() override;
  skellcore::ui::StudioLookAndFeel skin_;
  skellcore::world::WorldLink& world_;
  // A transparent hit target over the vector skull-key mark; no extra UI chrome.
  struct SkullMarkHit final : juce::Button {
    SkullMarkHit() : juce::Button("Cycle SkellCore skin") {
      setMouseCursor(juce::MouseCursor::PointingHandCursor);
      setComponentID("skellcore.skull-theme");
      setTooltip("Click skull to change the SKELL appearance");
    }
    void paintButton(juce::Graphics&, bool, bool) override {}
  } skullMark_;
  juce::ComboBox skinSelector_;
  juce::TextButton worldButton_{"JOIN WORLD"};
  juce::Label worldStatus_;
  juce::Slider gain;
  juce::Label title;
  juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellAudioProcessorEditor)
};

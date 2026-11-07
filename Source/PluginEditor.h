#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <SkellCore/UI.h>
class SkellAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
  explicit SkellAudioProcessorEditor(SkellAudioProcessor&);
  ~SkellAudioProcessorEditor() override;
  void paint(juce::Graphics&) override;
  void resized() override;
private:
  skellcore::ui::LookAndFeel skin_;
  juce::Slider gain;
  juce::Label title;
  juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellAudioProcessorEditor)
};

#include "PluginEditor.h"

SkellAudioProcessorEditor::SkellAudioProcessorEditor(SkellAudioProcessor& p)
    : AudioProcessorEditor(&p), gainAttachment(p.parameters, "gain", gain) {
  // Compact vector UI; setLookAndFeel never touches the audio callback.
  setLookAndFeel(&skin_);
  setResizable(true, true);
  setResizeLimits(370, 230, 840, 520);
  title.setText("Skell Quickstart", juce::dontSendNotification);
  title.setFont(juce::Font(juce::FontOptions(22.0f).withStyle("Bold")));
  title.setJustificationType(juce::Justification::centred);
  gain.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  gain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 24);
  addAndMakeVisible(title);
  addAndMakeVisible(gain);
  setSize(460, 292);
}

SkellAudioProcessorEditor::~SkellAudioProcessorEditor() {
  setLookAndFeel(nullptr);
}

void SkellAudioProcessorEditor::paint(juce::Graphics& g) {
  const auto theme = skin_.palette();
  const auto bounds = getLocalBounds().toFloat();
  g.setGradientFill(juce::ColourGradient(
      theme.surface, bounds.getCentreX(), bounds.getY(),
      theme.background, bounds.getCentreX(), bounds.getBottom(), false));
  g.fillAll();
  const auto panel = bounds.reduced(12.f);
  g.setColour(theme.background.withAlpha(.6f));
  g.fillRoundedRectangle(panel, 12.f);
  g.setColour(theme.accent.withAlpha(.52f));
  g.drawRoundedRectangle(panel, 12.f, 1.25f);
  g.setColour(theme.accent.withAlpha(.86f));
  g.fillRoundedRectangle(juce::Rectangle<float>(25.f, 25.f, 34.f, 3.f), 1.5f);
  g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
  g.setColour(theme.secondary);
  g.drawFittedText("SKELLCORE  /  VST TEMPLATE",
      30, 25, getWidth()-60, 18,
      juce::Justification::centredRight, 1);
  g.setColour(theme.accent);
  g.setFont(juce::Font(juce::FontOptions(12.5f).withStyle("Bold")));
  g.drawFittedText("GAIN", getWidth()/2 - 58, 80, 116, 18,
                   juce::Justification::centred, 1);
  g.setColour(theme.secondary.withAlpha(.82f));
  g.setFont(juce::Font(juce::FontOptions(10.0f)));
  g.drawFittedText("SHARED DSP  |  REALTIME SAFE",
      28, getHeight() - 31, getWidth()-56, 16, juce::Justification::centred, 1);
}

void SkellAudioProcessorEditor::resized() {
  const int w = getWidth(), h = getHeight();
  title.setBounds(30, 48, w-60, 30);
  const int knobSize = juce::jmin(156, juce::jmax(76, h-152));
  gain.setBounds((w-knobSize)/2, 103, knobSize, knobSize);
}

#include "PluginEditor.h"
#include <SkellCore/SkullTheme.h>

SkellAudioProcessorEditor::SkellAudioProcessorEditor(SkellAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), world_(p.worldLink), gainAttachment(p.parameters, "gain", gain) {
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
  skullHit_.setTooltip("Click the skull-key emblem to cycle SKELL themes.");
  skullHit_.onClick=[this] {
    processor_.setSkullSkin(skellcore::ui::nextSkin(processor_.getSkullSkin()));
    updateSkullSkin();
  };
  addAndMakeVisible(skullHit_);
  updateSkullSkin();
  worldButton_.onClick = [this] {
    if (world_.requested()) world_.leave();
    else world_.join(); // explicit only: never at plugin load
    timerCallback();
  };
  worldStatus_.setJustificationType(juce::Justification::centredLeft);
  worldStatus_.setFont(juce::Font(juce::FontOptions(11.0f)));
  addAndMakeVisible(worldButton_);
  addAndMakeVisible(worldStatus_);
  startTimerHz(4); // UI/status only, never processBlock
  setResizeLimits(390, 250, 840, 560);
  setSize(480, 330);
}

SkellAudioProcessorEditor::~SkellAudioProcessorEditor() {
  stopTimer();
  // Keep connection alive when only the editor closes.
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
  // One shared skull-key vector, extracted from SkellKey's actual header.
  const auto keyPalette = skellcore::ui::skullPalette(activeSkin_);
  skellcore::ui::drawSkullKeyMark(g, 25.f, 15.f, keyPalette.bone, theme.surface);
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
  g.drawFittedText("SKELLWORLD  |  OPTIONAL LOCAL CONNECTION",
      28, getHeight() - 31, getWidth()-56, 16, juce::Justification::centred, 1);
}

void SkellAudioProcessorEditor::resized() {
  const int w = getWidth(), h = getHeight();
  title.setBounds(30, 48, w-60, 30);
  const int knobSize = juce::jmin(156, juce::jmax(70, h-195));
  gain.setBounds((w-knobSize)/2, 103, knobSize, knobSize);
  skullHit_.setBounds(24, 14, 76, 34);
  worldButton_.setBounds(w/2 - 151, h-77, 122, 29);
  worldStatus_.setBounds(w/2 - 22, h-77, 170, 29);
}

void SkellAudioProcessorEditor::timerCallback() {
  if (activeSkin_ != processor_.getSkullSkin()) updateSkullSkin();
  const bool joined=world_.requested();
  worldButton_.setButtonText(joined ? "LEAVE WORLD" : "JOIN WORLD");
  if (world_.online())
    worldStatus_.setText("WORLD ONLINE  |  " + juce::String(world_.peerCount()) + " peers",
                         juce::dontSendNotification);
  else
    worldStatus_.setText(joined ? "HUB NOT RUNNING" : "OFFLINE",
                         juce::dontSendNotification);
}

void SkellAudioProcessorEditor::updateSkullSkin() {
  activeSkin_=processor_.getSkullSkin();
  skin_.setTheme(skellcore::ui::Theme::fromSkullSkin(activeSkin_));
  gain.repaint();
  worldButton_.repaint();
  worldStatus_.repaint();
  title.repaint();
  repaint();
}

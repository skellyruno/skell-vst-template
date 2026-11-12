#include "PluginEditor.h"
#include <SkellCore/SkellAppearanceStore.h>

SkellAudioProcessorEditor::SkellAudioProcessorEditor(SkellAudioProcessor& p)
    : AudioProcessorEditor(&p),
      skin_(skellcore::ui::SkellAppearanceStore::load("skellcore.quickstart")),
      world_(p.worldLink),
      processor_(p),
      gainAttachment(p.parameters, "gain", gain),
      powerAttachment_(p.parameters, "enabled", powerButton_) {
  setLookAndFeel(&skin_);
  setResizable(true, true);
  setResizeLimits(390, 250, 840, 560);

  title.setText("Skell Quickstart", juce::dontSendNotification);
  title.setFont(juce::Font(juce::FontOptions(22.0f).withStyle("Bold")));
  title.setJustificationType(juce::Justification::centredLeft);

  // Does not change the gain APVTS range, parameter ID, automation or DSP.
  skellcore::ui::configureStudioKnob(gain, 1.0);
  addAndMakeVisible(title);
  addAndMakeVisible(gain);

  skellcore::ui::populateStudioSkinSelector(skinSelector_);
  skinSelector_.setSelectedId(static_cast<int>(skin_.skin()) + 1, juce::dontSendNotification);
  skinSelector_.setTooltip("Appearance (shared across SKELL plugins that use StudioUI)");
  skinSelector_.onChange = [this] {
    if (skinSelector_.getSelectedId() > 0)
      setStudioSkin(skellcore::ui::studioSkinFromIndex(
          skinSelector_.getSelectedId() - 1), true);
  };
  addAndMakeVisible(skinSelector_);

  // The template uses the neutral skull, not SkellKey's logo.
  // Every adopting plugin selects its own SkullElement or custom forehead mark.
  skullMark_.setElement(skellcore::ui::SkullElement::Core);
  skullMark_.setTooltip("Click the SKELL skull to cycle appearance");
  skullMark_.onClick = [this] {
    const auto next = (static_cast<int>(skin_.skin()) + 1) % 4;
    setStudioSkin(skellcore::ui::studioSkinFromIndex(next), true);
  };
  addAndMakeVisible(skullMark_);

  addAndMakeVisible(powerButton_);
  addAndMakeVisible(settingsButton_);
  settingsButton_.onClick = [this] {
    settingsPanel_.setVisible(!settingsPanel_.isVisible());
    if (settingsPanel_.isVisible()) settingsPanel_.toFront(true);
  };
  addAndMakeVisible(audioLine_);
  showAudioLine_.setToggleState(true, juce::dontSendNotification);
  showAudioLine_.onClick = [this] {
    audioLine_.setVisible(showAudioLine_.getToggleState());
  };
  settingsPanel_.addAndMakeVisible(showAudioLine_);
  addAndMakeVisible(settingsPanel_);
  settingsPanel_.setVisible(false);
  setStudioSkin(skin_.skin(), false);

  worldButton_.onClick = [this] {
    if (world_.requested()) world_.leave();
    else world_.join(); // explicit only: never join at plugin load
    worldPollTick_ = 5; // force a status refresh on the next UI tick
    timerCallback();
  };
  worldStatus_.setComponentID("skellcore.world.status");
  worldStatus_.setJustificationType(juce::Justification::centredLeft);
  worldStatus_.setFont(juce::Font(juce::FontOptions(11.0f)));
  addAndMakeVisible(worldButton_);
  addAndMakeVisible(worldStatus_);
  setSize(480, 330);
  // Make OFFLINE/connection state visible on the very first rendered frame,
  // while keeping subsequent World polls at ~4 Hz.
  worldPollTick_ = 5;
  timerCallback();
  startTimerHz(24); // UI visual and status only; no work in processBlock
}

SkellAudioProcessorEditor::~SkellAudioProcessorEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}

void SkellAudioProcessorEditor::setStudioSkin(skellcore::ui::StudioSkin selection,
                                              bool persist) {
  if (selection != skin_.skin()) {
    // Reattach to propagate updated LAF colours to each child control.
    setLookAndFeel(nullptr);
    skin_.setSkin(selection);
    setLookAndFeel(&skin_);
  }
  skinSelector_.setSelectedId(static_cast<int>(selection) + 1,
                              juce::dontSendNotification);
  if (persist)
    (void) skellcore::ui::SkellAppearanceStore::save("skellcore.quickstart", selection);
  const auto p = skin_.palette();
  skullMark_.setBrandColours({p.foreground, p.background, p.accent, p.secondary});
  settingsButton_.setColours(p.accent, p.surface);
  powerButton_.setColours(p.accent, p.secondary, p.surface);
  settingsPanel_.setColours(p.surface, p.border, p.foreground, p.accent);
  audioLine_.setColours(p.accent, p.track);
  repaint();
}

void SkellAudioProcessorEditor::paint(juce::Graphics& g) {
  const auto theme = skin_.palette();
  const auto bounds = getLocalBounds().toFloat();
  g.setGradientFill(juce::ColourGradient(
      theme.surface, bounds.getCentreX(), bounds.getY(),
      theme.background, bounds.getCentreX(), bounds.getBottom(), false));
  g.fillAll();
  const auto panel = bounds.reduced(12.f);
  g.setColour(theme.surface.withAlpha(.8f));
  g.fillRoundedRectangle(panel, 12.f);
  g.setColour(theme.border);
  g.drawRoundedRectangle(panel, 12.f, 1.25f);

  // Shared neutral skull is an interactive child component; no SkellKey art.
  g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
  g.setColour(theme.secondary);
  g.drawFittedText("SKELLCORE  /  VST TEMPLATE",
      78, 25, getWidth()-200, 18,
      juce::Justification::centredRight, 1);
  g.setColour(theme.accent);
  g.setFont(juce::Font(juce::FontOptions(12.5f).withStyle("Bold")));
  g.drawFittedText("GAIN", getWidth()/2 - 58, 80, 116, 18,
                   juce::Justification::centred, 1);
  g.setColour(theme.secondary);
  g.setFont(juce::Font(juce::FontOptions(10.0f)));
  g.drawFittedText("SKELLWORLD  |  OPTIONAL LOCAL CONNECTION",
      28, getHeight() - 31, getWidth()-56, 16, juce::Justification::centred, 1);
}

void SkellAudioProcessorEditor::resized() {
  const int w = getWidth(), h = getHeight();
  title.setBounds(30, 48, w-198, 30);
  skinSelector_.setBounds(w-158, 49, 126, 27);
  skullMark_.setBounds(24, 14, 37, 34);
  settingsButton_.setBounds(w-65, 15, 32, 30);
  powerButton_.setBounds(w-105, 15, 32, 30);
  const int knobSize = juce::jmin(156, juce::jmax(70, h-195));
  gain.setBounds((w-knobSize)/2, 103, knobSize, knobSize);
  worldButton_.setBounds(w/2 - 151, h-77, 122, 29);
  worldStatus_.setBounds(w/2 - 22, h-77, 170, 29);
  audioLine_.setBounds(32, h-47, w-64, 12);
  settingsPanel_.setBounds(w - 277, 82, 244, 96);
  showAudioLine_.setBounds(settingsPanel_.contentBounds()
      .removeFromTop(28));
}

void SkellAudioProcessorEditor::timerCallback() {
  // Meter UI updates at ~24 Hz; World presence labels need just ~4 Hz.
  audioLine_.pushLevel(processor_.takeOutputPeak());
  if (++worldPollTick_ < 6) return;
  worldPollTick_ = 0;
  const bool joined = world_.requested();
  const juce::String buttonText = joined ? "LEAVE WORLD" : "JOIN WORLD";
  if (worldButton_.getButtonText() != buttonText)
    worldButton_.setButtonText(buttonText);
  // Presence needs the actual Hub. The separate hub-free tuning mapping does
  // not imply any SkellWorld peers or a running presence broker.
  const juce::String status = world_.hubOnline()
      ? "WORLD ONLINE  |  " + juce::String(world_.peerCount()) + " peers"
      : (joined ? "HUB NOT RUNNING" : "OFFLINE");
  if (worldStatus_.getText() != status)
    worldStatus_.setText(status, juce::dontSendNotification);
}

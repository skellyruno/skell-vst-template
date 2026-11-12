// Smoke test: capture the template's real JUCE AudioProcessorEditor.
// Compare every shared Studio UI skin at multiple sizes, never a mockup.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <SkellCore/StudioUI.h>
#include <SkellCore/GuidedTour.h>
#include <JuceHeader.h>
#include <cstdio>
#include <cmath>
#include <limits>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceGui;
    SkellAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    SkellAudioProcessorEditor editor(processor);

    // Shared identity and UI-only meter regression, no mock DAW audio.
    skellcore::LevelBridge level;
    level.publish(0.2f);
    level.publish(0.8f); // retain the maximum peak between GUI frames
    level.publish(0.1f);
    if (std::abs(level.take() - 0.8f) > 0.0001f || level.take() != 0.f) {
        std::fprintf(stderr, "Shared peak bridge dropped a transient\n");
        return 1;
    }
    level.publish(std::numeric_limits<float>::quiet_NaN());
    if (level.take() != 0.f) {
        std::fprintf(stderr, "Shared peak bridge accepted non-finite input\n");
        return 1;
    }
    skellcore::ui::SkellPowerButton power;
    power.setToggleState(true, juce::dontSendNotification);
    if (!power.getToggleState() || !power.getClickingTogglesState()) {
        std::fprintf(stderr, "Shared power toggle semantics broken\n");
        return 1;
    }
    skellcore::ui::SkellAudioLine audioLine;
    audioLine.setBounds(0, 0, 180, 18);
    audioLine.pushLevel(0.7f);
    skellcore::ui::SkellSettingsSurface settings("TEST PLUGIN SETTINGS");
    settings.setSize(220, 120);
    if (settings.contentBounds().isEmpty()) {
        std::fprintf(stderr, "Shared settings panel has no content area\n");
        return 1;
    }
    juce::Image plain(juce::Image::ARGB, 96, 96, true);
    juce::Image eqMark(juce::Image::ARGB, 96, 96, true);
    {
        juce::Graphics g(plain);
        skellcore::ui::drawSkellSkull(g, {0.f, 0.f, 96.f, 96.f});
    }
    {
        juce::Graphics g(eqMark);
        skellcore::ui::drawSkellSkull(g, {0.f, 0.f, 96.f, 96.f},
                                      skellcore::ui::SkullElement::Equalizer);
    }
    bool differs = false;
    for (int y = 0; y < 96 && !differs; ++y)
        for (int x = 0; x < 96; ++x)
            if (plain.getPixelAt(x,y) != eqMark.getPixelAt(x,y)) {
                differs = true; break;
            }
    if (!differs) {
        std::fprintf(stderr, "Plugin skull element did not change the mark\n");
        return 1;
    }

    // Test converted coordinate handling through a nested JUCE component
    // and a per-step panel activation callback without touching plugin DSP.
    juce::Component geometryRoot, nestedPanel;
    juce::TextButton nestedButton { "Nested Target" }, hiddenButton { "Hidden Tab" };
    geometryRoot.setSize(600, 400);
    geometryRoot.setVisible(true);
    geometryRoot.addAndMakeVisible(nestedPanel);
    nestedPanel.setBounds(50, 60, 260, 140);
    nestedPanel.addAndMakeVisible(nestedButton);
    nestedButton.setBounds(20, 30, 90, 27);
    geometryRoot.addAndMakeVisible(hiddenButton);
    hiddenButton.setBounds(340, 230, 120, 28);
    hiddenButton.setVisible(false);
    std::vector<skellcore::ui::GuidedTourStep> geometrySteps;
    geometrySteps.emplace_back(&nestedButton, "NESTED", "A control inside its own panel");
    geometrySteps.emplace_back(&hiddenButton, "OTHER TAB", "The correct page becomes visible",
                                [&hiddenButton] { hiddenButton.setVisible(true); });
    skellcore::ui::GuidedTourOverlay geometryTour(
        std::move(geometrySteps), {}, [](skellcore::ui::GuidedTourFinish){});
    geometryRoot.addAndMakeVisible(geometryTour);
    geometryTour.setBounds(geometryRoot.getLocalBounds());
    geometryTour.begin();
    // Navigation must use its own reliable LookAndFeel; Spectre and Silence
    // have editor-wide button painters that make NEXT dark-on-dark otherwise.
    bool nextHasIsolatedLookAndFeel = false;
    for (int i = 0; i < geometryTour.getNumChildComponents(); ++i) {
        if (auto* button = dynamic_cast<juce::TextButton*>(geometryTour.getChildComponent(i))) {
            if (button->getButtonText() == "NEXT")
                nextHasIsolatedLookAndFeel =
                    &button->getLookAndFeel() != &geometryRoot.getLookAndFeel();
        }
    }
    if (!nextHasIsolatedLookAndFeel) {
        std::fprintf(stderr,"GuidedTour NEXT button inherited host LookAndFeel\\n");
        return 1;
    }
    const auto actualTarget = geometryTour.spotlightBounds();
    if (actualTarget != juce::Rectangle<int>(64, 84, 102, 39)) {
        std::fprintf(stderr, "GuidedTour nested spotlight: actual %d,%d %dx%d; expected 64,84 102x39\\n",
            actualTarget.getX(), actualTarget.getY(),
            actualTarget.getWidth(), actualTarget.getHeight());
        return 1;
    }
    geometryTour.advance();
    if (geometryTour.stepIndex() != 1 || !hiddenButton.isVisible()
        || geometryTour.spotlightBounds().isEmpty()) {
        std::fprintf(stderr, "GuidedTour did not reveal or locate the next tab\n");
        return 1;
    }
    geometryTour.previous();
    if (geometryTour.stepIndex() != 0) {
        std::fprintf(stderr, "GuidedTour BACK navigation failed\n");
        return 1;
    }
    geometryTour.setColours({ juce::Colour(0xffd4a3f2),
                               juce::Colour(0xff201528),
                               juce::Colours::white,
                               juce::Colours::lightgrey,
                               juce::Colour(0xdd000000) });

    // Validate interaction defaults without changing a host parameter/attachment.
    juce::Slider knob;
    knob.setRange(0.0, 2.0, 0.001);
    skellcore::ui::configureStudioKnob(knob, 1.0);
    if (knob.getSliderStyle() != juce::Slider::RotaryHorizontalVerticalDrag) {
        std::fprintf(stderr, "Studio knob did not install rotary drag style\n");
        return 1;
    }

    // Actual template editor, actual component pointers and actual tour overlay.
    // The quickstart plugin itself stays tour-free; this is a renderer-only
    // demonstration to validate the shared UI before complex plugins opt in.
    juce::Slider* realKnob = nullptr;
    juce::ComboBox* realSkin = nullptr;
    for (int i = 0; i < editor.getNumChildComponents(); ++i) {
        auto* child = editor.getChildComponent(i);
        if (realKnob == nullptr) realKnob = dynamic_cast<juce::Slider*>(child);
        if (realSkin == nullptr) realSkin = dynamic_cast<juce::ComboBox*>(child);
    }
    if (realKnob == nullptr || realSkin == nullptr) {
        std::fprintf(stderr, "Tour targets not found in real JUCE editor\n");
        return 1;
    }
    skellcore::ui::GuidedTourController testTour(editor, "skellcore-ci-render-only", 1);
    std::vector<skellcore::ui::GuidedTourStep> editorSteps;
    editorSteps.emplace_back(realKnob, "YOUR GAIN", "Drag the metal knob to adjust the audio gain.");
    editorSteps.emplace_back(realSkin, "CHOOSE A SKIN", "Switch your plugin appearance independently of the sound.");
    testTour.setSteps(std::move(editorSteps));

    skellcore::ui::headless::Options options;
    options.project = "SkellCore template Studio UI";
    options.scenes = {
        {"ivory_small",     390, 250, 0},
        {"obsidian_small",  390, 250, 1},
        {"amethyst_small",  390, 250, 2},
        {"crimson_small",   390, 250, 3},
        {"ivory_default",   480, 330, 0},
        {"obsidian_default",480, 330, 1},
        {"amethyst_default",480, 330, 2},
        {"crimson_default", 480, 330, 3},
        {"ivory_large",     840, 560, 0},
        {"obsidian_large",  840, 560, 1},
        {"tour_ivory_default", 480, 330, 0, 1},
        {"tour_obsidian_small", 390, 250, 1, 1},
        {"tour_amethyst_large", 840, 560, 2, 1}
    };
    options.configure = [&editor, &testTour](const skellcore::ui::headless::Scene& scene) {
        testTour.dismiss();
        const auto selection = skellcore::ui::studioSkinFromIndex(scene.theme);
        editor.setStudioSkin(selection, false);
        const auto p = skellcore::ui::studioPalette(selection);
        testTour.setPalette({p.accent, p.surface, p.foreground, p.secondary,
                             juce::Colour(0xd9000000)});
        if (scene.mode == 1 && !testTour.replay())
            std::fprintf(stderr, "Tour did not show in renderer scene\n");
    };
    options.inspect = [&editor, &testTour] {
        juce::StringArray problems;
        for (int i = 0; i < editor.getNumChildComponents(); ++i) {
            const auto* child = editor.getChildComponent(i);
            if (child->isVisible() && !editor.getLocalBounds().contains(child->getBounds()))
                problems.add("Visible control extends outside window: " + child->getName());
        }
        if (const auto* overlay = testTour.overlay()) {
            if (overlay->informationBounds().intersects(overlay->spotlightBounds()))
                problems.add("GuidedTour info panel overlaps spotlight target");
            if (!editor.getLocalBounds().contains(overlay->informationBounds()))
                problems.add("GuidedTour info panel clipped outside editor");
        }
        return problems;
    };
    options.mustDiffer = {
        {"ivory_default", "obsidian_default"},
        {"ivory_default", "amethyst_default"},
        {"ivory_default", "crimson_default"},
        {"obsidian_default", "amethyst_default"},
        {"amethyst_default", "crimson_default"},
        {"ivory_default", "tour_ivory_default"},
        {"obsidian_small", "tour_obsidian_small"},
        {"tour_ivory_default", "tour_obsidian_small"}
    };

    const auto directory = argc > 1
        ? juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile("ui-screenshots");
    const auto report = skellcore::ui::headless::render(editor, directory, options);
    for (const auto& error : report.errors)
        std::fprintf(stderr, "%s\n", error.toRawUTF8());
    std::printf("Captured %d / %zu Studio UI and GuidedTour scenes\n",
                report.captured, options.scenes.size());
    processor.releaseResources();
    return report.passed() && report.captured == static_cast<int>(options.scenes.size()) ? 0 : 1;
}

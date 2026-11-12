// Smoke test: capture the template's real JUCE AudioProcessorEditor.
// Compare every shared Studio UI skin at multiple sizes, never a mockup.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <SkellCore/StudioUI.h>
#include <SkellCore/SkellAppearanceStore.h>
#include <SkellCore/SkellPresetBar.h>
#include <SkellCore/SkellMeterControls.h>
#include <SkellCore/SkellResponsiveLayout.h>
#include <SkellCore/GuidedTour.h>
#include <JuceHeader.h>
#include <cstdio>
#include <cmath>
#include <limits>
#include <chrono>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceGui;
    SkellAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    SkellAudioProcessorEditor editor(processor);

    // Independent product theme preferences must not alias each other or
    // permit IDs to escape the SkellAudio/Appearance directory.
    const auto coreSkin = skellcore::ui::SkellAppearanceStore::fileFor("skellcore.quickstart");
    const auto delaySkin = skellcore::ui::SkellAppearanceStore::fileFor("skell.delay");
    const auto traversalSkin = skellcore::ui::SkellAppearanceStore::fileFor("../../outside");
    if (coreSkin == delaySkin || coreSkin == juce::File()
        || delaySkin == juce::File()
        || traversalSkin.getParentDirectory() != coreSkin.getParentDirectory()
        || skellcore::ui::SkellAppearanceStore::fileFor(" skell.delay ") != delaySkin
        || skellcore::ui::SkellAppearanceStore::fileFor(" ") != juce::File()
        || skellcore::ui::SkellAppearanceStore::load(
               " ", skellcore::ui::StudioSkin::Crimson)
               != skellcore::ui::StudioSkin::Crimson) {
        std::fprintf(stderr, "Product appearance isolation or path safety failed\n");
        return 1;
    }


    // Preset browser never modifies host parameters or storage itself.
    skellcore::ui::SkellPresetBar presets;
    presets.setBounds(0, 0, 360, 36);
    int callbackCount = 0, lastPreset = -1;
    presets.onPresetSelected = [&](int index) {
        ++callbackCount;
        lastPreset = index;
    };
    presets.setPresets(juce::StringArray{"Clean", "Warm", "Bright"}, 0);
    if (presets.selectedIndex() != 0 || presets.presetCount() != 3
        || presets.previousButton().isEnabled()
        || !presets.nextButton().isEnabled()
        || presets.selectPreset(0, true)) {
        std::fprintf(stderr, "Preset bar initial state or no-op selection failed\n");
        return 1;
    }
    if (!presets.selectPreset(1, true) || lastPreset != 1 || callbackCount != 1) {
        std::fprintf(stderr, "Preset bar did not report one selection\n");
        return 1;
    }
    presets.nextButton().onClick();
    if (presets.selectedIndex() != 2 || lastPreset != 2 || callbackCount != 2
        || presets.nextButton().isEnabled()) {
        std::fprintf(stderr, "Preset bar exceeded its last item\n");
        return 1;
    }
    presets.previousButton().onClick();
    if (presets.selectedIndex() != 1 || callbackCount != 3) {
        std::fprintf(stderr, "Preset bar navigation callback failed\n");
        return 1;
    }
    presets.setModified(true);
    if (!presets.isModified()) {
        std::fprintf(stderr, "Preset dirty state failed\n");
        return 1;
    }
    for (int width : {128, 225, 420}) {
        presets.setBounds(0, 0, width, 32);
        for (int i = 0; i < presets.getNumChildComponents(); ++i) {
            auto* child = presets.getChildComponent(i);
            if (child->isVisible()
                && !presets.getLocalBounds().contains(child->getBounds())) {
                std::fprintf(stderr, "Preset layout clipped at width %d\n", width);
                return 1;
            }
        }
    }
    presets.setPresets({});
    if (presets.selectedIndex() != -1
        || presets.selector().isEnabled()
        || presets.nextButton().isEnabled()
        || presets.previousButton().isEnabled()
        || presets.selectPreset(0, true)) {
        std::fprintf(stderr, "Empty preset bank was not handled safely\n");
        return 1;
    }

    // Responsive cells must stay non-overlapping within three common editor
    // widths; undersized regions must fail closed, never clip controls.
    skellcore::ui::SkellBreakpoints breakpoints;
    if (breakpoints.classify(390) != skellcore::ui::SkellLayoutDensity::Compact
        || breakpoints.classify(540) != skellcore::ui::SkellLayoutDensity::Regular
        || breakpoints.classify(840) != skellcore::ui::SkellLayoutDensity::Wide) {
        std::fprintf(stderr, "Responsive breakpoints misclassified widths\n");
        return 1;
    }
    for (int width : {320, 480, 840}) {
        const juce::Rectangle<int> region {12, 24, width-24, 320};
        const auto cells = skellcore::ui::skellResponsiveCells(region, 9, 135, 72, 8, 4);
        if (cells.size() != 9) {
            std::fprintf(stderr, "Responsive grid failed width %d\n", width);
            return 1;
        }
        for (std::size_t i = 0; i < cells.size(); ++i) {
            if (cells[i].isEmpty() || !region.contains(cells[i])) {
                std::fprintf(stderr, "Responsive cell clipped width %d\n", width);
                return 1;
            }
            for (std::size_t j = i+1; j < cells.size(); ++j) {
                if (cells[i].intersects(cells[j])) {
                    std::fprintf(stderr, "Responsive grid overlap at width %d\n", width);
                    return 1;
                }
            }
        }
    }
    if (!skellcore::ui::skellResponsiveCells({0, 0, 3, 2}, 20).empty()) {
        std::fprintf(stderr, "Undersized grid did not fail safely\n");
        return 1;
    }

    // Two-channel visual meter: left and right remain independent, values
    // stay finite, and both horizontal and vertical painting complete.
    skellcore::ui::SkellStereoMeter stereo;
    stereo.setBounds(0, 0, 220, 36);
    stereo.pushLevels(.8f, .3f, 1.f/24.f);
    if (stereo.leftReading().level < .799f
        || stereo.rightReading().level < .299f) {
        std::fprintf(stderr, "Stereo meter dropped input peaks\n");
        return 1;
    }
    {
        juce::Image horizontal(juce::Image::ARGB, 220, 36, true);
        juce::Graphics g(horizontal);
        stereo.paint(g);
    }
    stereo.setVertical(true);
    stereo.setBounds(0, 0, 36, 220);
    stereo.pushLevels(std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity(), 1.f/24.f);
    if (!std::isfinite(stereo.leftReading().level)
        || !std::isfinite(stereo.rightReading().level)) {
        std::fprintf(stderr, "Stereo meter accepted invalid values\n");
        return 1;
    }
    {
        juce::Image vertical(juce::Image::ARGB, 36, 220, true);
        juce::Graphics g(vertical);
        stereo.paint(g);
    }

    // Informational-only GUI paint baseline. Not a latency guarantee.
    {
        juce::Image performanceSurface(juce::Image::ARGB, 220, 36, true);
        stereo.setVertical(false);
        stereo.setBounds(0, 0, 220, 36);
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 500; ++i) {
            stereo.pushLevels(float(i%50)/50.f, float(i%30)/30.f, 1.f/24.f);
            juce::Graphics g(performanceSurface);
            stereo.paint(g);
        }
        const auto elapsed = std::chrono::duration<double,std::micro>(
            std::chrono::steady_clock::now()-start).count();
        std::printf("SkellCore stereo meter paint: %.2f us/frame (500 frames, non-gating)\n",
                    elapsed / 500.);
    }

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
    // The shared toggle must have two visibly distinct states and retain
    // normal JUCE Button behavior instead of being a decorative status dot.
    juce::Image toggleOff(juce::Image::ARGB, 168, 26, true);
    juce::Image toggleOn(juce::Image::ARGB, 168, 26, true);
    skellcore::ui::SkellHardwareLookAndFeel toggleLook(
        skellcore::ui::StudioSkin::Obsidian);
    juce::ToggleButton testToggle {"SHOW AUDIO LINE"};
    testToggle.setSize(168, 26);
    {
        juce::Graphics g(toggleOff);
        toggleLook.drawToggleButton(g, testToggle, false, false);
    }
    testToggle.setToggleState(true, juce::dontSendNotification);
    {
        juce::Graphics g(toggleOn);
        toggleLook.drawToggleButton(g, testToggle, false, false);
    }
    bool toggleStatesDiffer = false;
    for (int y = 0; y < 26 && !toggleStatesDiffer; ++y)
        for (int x = 0; x < 168; ++x)
            if (toggleOff.getPixelAt(x, y) != toggleOn.getPixelAt(x, y)) {
                toggleStatesDiffer = true;
                break;
            }
    if (!toggleStatesDiffer) {
        std::fprintf(stderr, "Shared UI toggle has indistinguishable on/off states\n");
        return 1;
    }
    // Tiny drawing bounds must be a safe no-op, not negative geometry.
    testToggle.setSize(10, 9);
    juce::Image tinyToggle(juce::Image::ARGB, 10, 9, true);
    juce::Graphics tinyG(tinyToggle);
    toggleLook.drawToggleButton(tinyG, testToggle, false, false);

    skellcore::ui::SkellAudioLine audioLine;
    audioLine.setBounds(0, 0, 180, 18);
    audioLine.pushLevel(0.7f);
    for (int i = 0; i < 65; ++i) audioLine.pushLevel(0.f);
    juce::Image settled(juce::Image::ARGB, 180, 18, true);
    juce::Image pristine(juce::Image::ARGB, 180, 18, true);
    juce::Graphics settledGraphics(settled);
    audioLine.paint(settledGraphics);
    skellcore::ui::SkellAudioLine quietLine;
    quietLine.setBounds(0, 0, 180, 18);
    juce::Graphics pristineGraphics(pristine);
    quietLine.paint(pristineGraphics);
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 180; ++x)
            if (settled.getPixelAt(x, y) != pristine.getPixelAt(x, y)) {
                std::fprintf(stderr, "Audio line did not return to flat idle state\\n");
                return 1;
            }
    // Non-finite knob input must render exactly like a safe zero value.
    juce::Image validDial(juce::Image::ARGB, 96, 96, true);
    juce::Image invalidDial(juce::Image::ARGB, 96, 96, true);
    const auto drawDial = [](juce::Image& image, float position) {
        juce::Graphics g(image);
        skellcore::ui::drawHardwareKnob(g, {16.f, 16.f, 64.f, 64.f},
            position, -2.5f, 2.5f,
            juce::Colours::green, juce::Colours::grey,
            juce::Colours::darkgrey, juce::Colours::white);
    };
    drawDial(validDial, 0.f);
    drawDial(invalidDial, std::numeric_limits<float>::quiet_NaN());
    for (int y = 0; y < 96; ++y)
        for (int x = 0; x < 96; ++x)
            if (validDial.getPixelAt(x, y) != invalidDial.getPixelAt(x, y)) {
                std::fprintf(stderr, "Invalid hardware knob position changed paint\\n");
                return 1;
            }
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
    // Verify the real product settings gear toggles its own panel, and that
    // the World label is available before the editor's first timer tick.
    skellcore::ui::SkellSettingsButton* realGear = nullptr;
    skellcore::ui::SkellSettingsSurface* realSettings = nullptr;
    bool initialOffline = false;
    for (int i = 0; i < editor.getNumChildComponents(); ++i) {
        auto* child = editor.getChildComponent(i);
        if (auto* gear = dynamic_cast<skellcore::ui::SkellSettingsButton*>(child))
            realGear = gear;
        if (auto* panel = dynamic_cast<skellcore::ui::SkellSettingsSurface*>(child))
            realSettings = panel;
        if (auto* label = dynamic_cast<juce::Label*>(child))
            if (label->getComponentID() == "skellcore.world.status"
                && label->getText() == "OFFLINE") initialOffline = true;
    }
    if (!realGear || !realSettings || !realGear->onClick
        || realSettings->isVisible() || !initialOffline) {
        std::fprintf(stderr, "Settings or first-frame World status is broken\\n");
        return 1;
    }
    realGear->onClick();
    if (!realSettings->isVisible()) {
        std::fprintf(stderr, "Settings gear did not open plugin settings\\n");
        return 1;
    }
    realGear->onClick();
    if (realSettings->isVisible()) {
        std::fprintf(stderr, "Settings gear did not close plugin settings\\n");
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

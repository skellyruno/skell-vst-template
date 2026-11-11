// Smoke test: capture the template's real JUCE AudioProcessorEditor.
// Compare every shared Studio UI skin at multiple sizes, never a mockup.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <SkellCore/StudioUI.h>
#include <JuceHeader.h>
#include <cstdio>

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceGui;
    SkellAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    SkellAudioProcessorEditor editor(processor);

    // Validate interaction defaults without changing a host parameter/attachment.
    juce::Slider knob;
    knob.setRange(0.0, 2.0, 0.001);
    skellcore::ui::configureStudioKnob(knob, 1.0);
    if (knob.getSliderStyle() != juce::Slider::RotaryHorizontalVerticalDrag) {
        std::fprintf(stderr, "Studio knob did not install rotary drag style\n");
        return 1;
    }

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
        {"obsidian_large",  840, 560, 1}
    };
    options.configure = [&editor](const skellcore::ui::headless::Scene& scene) {
        editor.setStudioSkin(skellcore::ui::studioSkinFromIndex(scene.theme), false);
    };
    options.inspect = [&editor] {
        juce::StringArray problems;
        for (int i = 0; i < editor.getNumChildComponents(); ++i) {
            const auto* child = editor.getChildComponent(i);
            if (child->isVisible() && !editor.getLocalBounds().contains(child->getBounds()))
                problems.add("Visible control extends outside window: " + child->getName());
        }
        return problems;
    };
    options.mustDiffer = {
        {"ivory_default", "obsidian_default"},
        {"ivory_default", "amethyst_default"},
        {"ivory_default", "crimson_default"},
        {"obsidian_default", "amethyst_default"},
        {"amethyst_default", "crimson_default"}
    };

    const auto directory = argc > 1
        ? juce::File::getCurrentWorkingDirectory().getChildFile(juce::String::fromUTF8(argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile("ui-screenshots");
    const auto report = skellcore::ui::headless::render(editor, directory, options);
    for (const auto& error : report.errors)
        std::fprintf(stderr, "%s\n", error.toRawUTF8());
    std::printf("Captured %d / %zu Studio UI scenes\n",
                report.captured, options.scenes.size());
    processor.releaseResources();
    return report.passed() && report.captured == static_cast<int>(options.scenes.size()) ? 0 : 1;
}

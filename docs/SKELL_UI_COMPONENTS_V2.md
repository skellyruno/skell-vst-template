# SKELL shared UI components v2 — provisional visuals

These components share tested interaction and rendering behavior. They do not
decide a universal theme, skull geometry, knob look, or plugin layout.
Host program IDs, preset formats, automation and DSP stay product-owned.

## Preset bar

Header: SkellCore/SkellPresetBar.h, link SkellCore::UI.

    skellcore::ui::SkellPresetBar presets;
    presets.setPresets(juce::StringArray{"Default", "Warm", "Bright"}, 0);
    presets.onPresetSelected = [this](int index) {
        loadOurOwnVerifiedPreset(index); // product-owned implementation
    };
    addAndMakeVisible(presets);

Indices are zero-based; JUCE ComboBox IDs are index+1. Programmatic
setPresets() and selectPreset(index, false) do not load presets.
User selection (and selectPreset(index, true)) reports the changed index
once through the callback. Invalid and duplicate selections do nothing.
Empty banks disable controls. Dirty status is just a visual flag controlled
by the owning plugin. There is no fake save action or filesystem behavior.

Navigation buttons appear at >=182px; the label appears at >=285px;
the ComboBox remains usable when compact. Check different host DPI sizes.

## Stereo meters

Header: SkellCore/SkellMeterControls.h, link SkellCore::UI.
JUCE-free math: SkellCore/MeterBallistics.h.

    // Audio thread: publish already-calculated scalar peaks only.
    leftPeakBridge.publish(leftBlockPeak);
    rightPeakBridge.publish(rightBlockPeak);

    // Message-thread editor timer, approximately 24Hz.
    stereoMeter.pushLevels(leftPeakBridge.take(),
                           rightPeakBridge.take(), 1.f/24.f);

Supports two channels, vertical/horizontal orientations, instantaneous rise,
bounded exponential falloff, peak hold, and no repaint when unchanged.
No audio buffers are read and no GUI work occurs on the audio thread.
Peak markers are NOT true clipping alarms: the bridge stores 0..1 levels,
not actual overshoot or true-peak flags. Themes are configurable and
provisional.

## Responsive layout

Header: SkellCore/SkellResponsiveLayout.h, link SkellCore::UI.

    skellcore::ui::SkellBreakpoints breakpoints;
    const auto density = breakpoints.classify(getWidth());
    const auto cells = skellcore::ui::skellResponsiveCells(
        controlArea, 9, 135, 72, 8, 4);

This API returns rectangles without reparenting controls or changing their
semantics. The plugin still determines which controls belong in each cell.
No output rectangles overlap or escape the available region. Insufficient
space returns an empty vector: the plugin must scroll or collapse instead
of drawing clipped controls. Values and breakpoints are per-plugin choices.

## Benchmark and verification contracts

SkellCoreMeterBallisticsTests: portable CTest covering invalid inputs,
attack, peak hold, release and different GUI update rates.
SkellCoreMeterBenchmark: non-gating CPU microbenchmark, built with
-DSKELLCORE_BUILD_BENCHMARKS=ON; prints timing from the current machine.
The real-editor headless renderer checks preset callback counts, empty
banks, responsive bounds and stereo meter paint in both orientations,
and prints an informational GUI paint timing.

A new generated-plugin Windows build checks the generator's immutable pin
against a full VST3 compilation. This already exposed an obsolete pin to a
JUCE MD5 type; the pin was corrected to a revision using the supported
appearance implementation.

Release still requires final Windows build, screenshot capture, fresh
generated-plugin VST3, portable tests and FL Studio behavior/automation
validation. Documentation is not a substitute for those tests.

Final product visuals (skull, theme, knobs, power icon) remain undecided.

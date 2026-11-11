# SkellCore Guided Tours — first-run onboarding for complex plugins

Inspired by the **real OmniChoir FX v0.8 source**, whose editor dims the interface, spots one target at a time, shows an explanation and Next/Skip controls, remembers `tour-done` in user app data, and offers **Show the tour** from its style menu. SkellCore keeps that familiar pattern but makes it reusable and host-safe.

## Scope: opt in only where the workflow needs explanation

**Recommended first adopters:** Skell Preset, Skell Afar, Skell Silence, OmniChoir. Other likely complex editors: Skell Separator, SkellProjectManager-like apps (with appropriate adaptation), modular racks, mastering suites with advanced processing tabs.

**Do not automatically add this to simple effects** (Delay, Cast, Gate, EQ, ScratchPad, etc.) or every plugin created by the template. A complex plugin must *explicitly instantiate* `GuidedTourController`. The SkellCore example renderer tests it without adding an on-launch tour to the template's simple gain editor.

## Shared API: one header and one editor-owned controller

```cpp
#include <SkellCore/GuidedTour.h>

// PluginEditor.h — keep the controller AFTER the real UI controls
// so it is destroyed before the controls it references.
skellcore::ui::GuidedTourController tour_ { *this, "skell-preset-vst", 1 };
```

**IMPORTANT: C++ member construction order.** Define tour_ after all controls that its steps reference, or construct it as a `std::unique_ptr` in the editor constructor after the controls are created. It doesn't alter JUCE APVTS or DSP.

```cpp
// PluginEditor.cpp — once all widgets exist and resized() has set bounds:
std::vector<skellcore::ui::GuidedTourStep> steps;

steps.emplace_back(&presetBrowser, "CHOOSE A SOUND",
    "Select a sound to begin. This changes only the selected preset.");
steps.emplace_back(&slotOne, "EFFECT SLOTS",
    "Click a slot to configure it, or drag the handle to reorder effects.");
steps.emplace_back(&advancedControl, "ADVANCED CONTROLS",
    "Fine-tune a selected effect here. These controls remain optional.",
    [this] {
        showAdvancedPanel(); // actual editor function: reveal page BEFORE spotlight
        resized();
    });

// Inherit colors from StudioUI if the plugin uses the shared four skins:
const auto p = skellcore::ui::studioPalette(skin_);
tour_.setPalette({ p.accent, p.surface, p.foreground, p.secondary,
                   juce::Colour(0xd9000000) });
tour_.setSteps(std::move(steps));
tour_.startIfFirstRun(); // call only AFTER the initial layout is ready.

// In the settings/help menu, always offer a way back:
helpTourButton.onClick = [this] { tour_.replay(); };
```

For editors that create controls dynamically, call `setSteps` again only from the GUI thread after the controls are reconstructed. The controller stores weak/safe references, and invalid controls are skipped rather than dereferenced.

A step accepts `(primary, heading, explanation, optionalOnEnter, optionalSecondary)`. The optional `onEnter` can switch tabs, open an advanced drawer, or reveal a collapsed section. Target rectangles are converted into the overlay's local coordinate system so nested control layouts work. Reposition the editor using `resized` as normal: the controller observes the editor and automatically resizes the overlay.

## Behavior and UX contract

- First open: spotlight tutorial runs **once per user, per plugin ID and tour version** when `startIfFirstRun()` is explicitly invoked. Never show it in a simple plugin by default.
- Steps: small dark panel with short title, one practical sentence, progress (1/N), **Back / Next / Skip**, prominent focus outline, no excessive glow.
- Keyboard: Left/Right, Enter/Space and Escape.
- When complete or skipped: mark as seen under the user's application data `SkellAudio/GuidedTours/tour-{stable-id}-v{version}.done` — not in preset state. If a control is unavailable and the tour cannot start, **do not** mark it as seen.
- Replay: a visible **? / HELP / SHOW TUTORIAL** menu item calls `tour_.replay()`; manual replay works regardless of the stored flag.
- Context: steps explain a *workflow* ("how to load an effect and change its routing"), not every small control. Limit to roughly 4–7 steps, no walls of text; never show a step for a missing/hidden target.
- The highlight overlay consumes mouse events so accidental clicks cannot change host parameters mid-tour.
- The tour has no DSP callback, timers, host state changes, audio-thread I/O, allocations or synchronization. Preference reads/writes occur on the JUCE message thread only.

## Specific recommended onboarding sequences

**Skell Preset** (5–6): choose a preset; add an effect slot (+); change effect controls; move/reorder slots; open external VST3 if supported; save the sound and use Help to replay.

**Skell Afar** (5–6): what the space/ambience effect does; start with its main macro; choose or edit a bed; work with detail panel; preview input/output or wet mix; use Studio/Live option only if implemented in this plugin.

**Skell Silence** (5): what the detector/waveform does; adjust threshold/silence; learn only if explicitly supported; lock only if actually implemented; refine advanced controls; preview the result.

Do not make claims about controls that do not exist in the actual plugin. Inspect each real editor/processor first. Tailor step wording and target pointers to the implemented UI.

## Testing and review requirements

1. Pin an immutable released SkellCore commit that contains `SkellCore/GuidedTour.h` and link `SkellCore::UI`.
2. Build Windows VST3, Standalone if supported, and actual JUCE editor smoke test.
3. Use the SkellCore real-editor headless renderer. Test tour at compact/default/large resolutions and all four skins, including controls nested inside tabs/panels.
4. Verify **Back/Next/Skip**, keyboard, first run, replay, tab reveal, missing/deleted target, two simultaneous plugin instances, resize while tutorial is visible, and no parameter changes when clicking the dimmed area.
5. Check text readability and no clipped bubble in minimum supported editor size. Audio DSP, latency, CPU, automation IDs and saved DAW projects must remain unchanged.

## Failure handling

If user application data is unwritable, the tutorial still runs, but can't persist the completion flag. Don't block audio or plugin creation. Never run disk I/O in the processor's audio thread. Use a stable ID (not a display name that might change). Increment the version only when the tour has materially changed.

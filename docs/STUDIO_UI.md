# SKELL Studio UI — shared controls and plugin skins

This is a **UI-only** SkellCore addition. It implements design-research items **3** (premium 3D knobs with precise interactions) and **5** (four interchangeable skins). No pitch-engine work, MIDI, DSP, latency or plugin parameter IDs are modified.

## The shared skin contract

`#include <SkellCore/StudioUI.h>` provides:

- `skellcore::ui::StudioSkin`: Ivory (default), Obsidian, Amethyst, Crimson.
- `StudioLookAndFeel`: consistent restrained raised-metal dials, readable value boxes, buttons, menus, edit fields and accent rings.
- `configureStudioKnob(slider, resetValue)`: rotary drag, double-click reset, typed values, mouse-wheel adjustment and 260-pixel drag sensitivity; leaves host parameter ranges and attachment in place.
- `StudioSkinStore::load/save`: shared per-user selection in `SkellAudio/studio-skin-v1.txt` under the OS application-data directory.
- `populateStudioSkinSelector(comboBox)`: four stable selector IDs, 1 through 4.
- `studioPalette(skin)`: a single palette shared by custom graphics, iconography and standard JUCE controls.

Ivory and Obsidian use subtle emerald accents; Amethyst has a muted violet accent, and Crimson a muted warm red. Glow is intentionally restrained and there are no image textures or frame-by-frame animation dependencies.

## Integrate into existing plugin editors

Keep all existing plugin IDs, parameter IDs, automation attachments, presets, and DSP. Opt in one editor at a time.

```cpp
// Editor header, not the processor:
#include <SkellCore/StudioUI.h>
skellcore::ui::StudioLookAndFeel studioLook {
    skellcore::ui::StudioSkinStore::load()
};
juce::ComboBox skinChoice;

// Editor constructor:
setLookAndFeel(&studioLook);
skellcore::ui::configureStudioKnob(retuneSlider, 18.0); // existing retune parameter default
skellcore::ui::configureStudioKnob(humanizeSlider, 0.0);
skellcore::ui::populateStudioSkinSelector(skinChoice);
skinChoice.setSelectedId(static_cast<int>(studioLook.skin()) + 1,
                         juce::dontSendNotification);
skinChoice.onChange = [this] {
    const auto id = skinChoice.getSelectedId();
    if (id <= 0) return;
    const auto selected = skellcore::ui::studioSkinFromIndex(id - 1);
    setLookAndFeel(nullptr);
    studioLook.setSkin(selected);
    setLookAndFeel(&studioLook);
    repaint();
    (void) skellcore::ui::StudioSkinStore::save(selected);
};
addAndMakeVisible(skinChoice);

// Editor destructor before member LookAndFeel destruction:
setLookAndFeel(nullptr);
```

Use your existing APVTS `SliderAttachment`; **do not** create separate UI-only parameters or override `onValueChange` for host automation. `resetValue` is the actual default for that knob, not a hardcoded universal default. SkellCore intentionally does not set a plugin's parameter range, identity, or preset state.

A single global *default appearance* is loaded by each newly opened editor that uses this module. Existing open windows and older plugins that have not adopted the module do not magically reskin. Skin selection is intentionally **not** added to each DAW state chunk, so changing appearance does not change the sound or alter saved sessions. If a plugin explicitly requires project-specific appearance, implement its own separate persistence after deciding that behavior.

Use `studioPalette` in custom vector components. For actual clickable knobs, stay with JUCE `Slider` so host automation and text entry still work. Don't paint only a dummy knob graphic on top of an unresponsive control.

## Testing

The quickstart editor now opts in, as a real working sample. Build and run its existing headless renderer:

```sh
cmake -S . -B build -DSKELLCORE_BUILD_UI_RENDERER=ON
cmake --build build --config Release --target SkellCoreUISmokeRenderer
# Run built renderer with an output directory, using Xvfb on Linux.
```

The matrix contains **ten** scene captures: all four skins at 390×250 and 480×330, plus Ivory and Obsidian at 840×560. It rejects blank screenshots, clipped visible children and identical skin images. Validate more granular interactions manually or with component-level host tests, including double-click reset, keyboard entry, wheel adjustments and host automation on Windows/FL Studio before releasing any migrated plugin.

The plugin's AUDIO callback must never call UI painting, disk settings storage, `setSkin`, or `configureStudioKnob`. Palette loading/saving is user-UI-thread I/O; there is no audio-thread allocation or runtime DSP change.

## Adopting in other SKELL plugins

1. Pin an immutable SkellCore commit in the plugin's CMake `FetchContent` configuration (or update its vendored SkellCore subtree).
2. Link `SkellCore::UI`, which already depends on JUCE GUI targets.
3. Convert the real editor's LookAndFeel and its existing knob components. Preserve editor layout unless the plugin design explicitly changes.
4. Use a single shared skull master geometry from `SkullTheme.h`; tint it from the selected Studio palette.
5. Verify real editor screenshots at small/default/large windows and 4 skins, then run VST3 build and host tests.

Do not migrate all independent repos blindly; each editor has its own widget and parameter semantics. The shared system is ready for reuse, but a per-plugin integration change and regression test are still required.

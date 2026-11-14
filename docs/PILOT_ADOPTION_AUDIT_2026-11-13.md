# SkellCore pilot adoption audit — Skell EQ and Skell Delay

**Status: audit only.** No product repository was edited, no plugin UI
was silently changed, and there is no claim of new FL Studio compatibility.
Source: repositories at their main branches inspected November 13, 2026.

## Skell EQ

Repository: https://github.com/skellyruno/skell-eq-vst
Inspected main tree: 5825e196cab6668eeb5d577f2e647f559f5d4344

- CMake uses JUCE 8.0.12 and the existing product ID audio.skelly.eq / SeQ1.
- CMake already fetches a pinned SkellCore source snapshot for *header-only*
  SkellWorld presence at c4ae09c896fa84279d3cc42f19925f97a7e5c124.
- The EQ has multiple editor implementations / safety modes, including
  PluginEditor, RestoredEditor and StableEditor. The default CMake flags
  favor a safe editor for FL Studio crash isolation.
- Custom EQ graph, analyzer, meter panel, band controls, typography and
  binary artwork are core product identity. Existing APVTS attachments
  and parameter IDs must not change.

**Safe pilot:** use one separate EQ branch; update the shared source pin
only after checking the presence API and build/host compatibility. Introduce
product-scoped appearance preferences or isolated non-invasive helpers
first. Retain existing look-and-feel, meters, graph, skin and editors.
Do not automatically substitute the generic skull or preset bar.

**Critical regression:** compare safe/restored editor modes, frequent
open/close/resize in FL Studio, 8-band automation and saved-project recall.

## Skell Delay

Repository: https://github.com/skellyruno/skell-delay-vst
Inspected main tree: 7f95aa8a3d2e7972c28239e156f6f2dc9f4036d3

- CMake uses JUCE 8.0.17-security, C++20, and its existing product ID
  com.skellyaudio.skelldelay / Dlay. It currently lists VST3 and AU targets.
- Source/PresetBrowser.h is a real, feature-rich browser: search, category
  tags, favorites, audition, user save/delete, randomise, undo/redo.
- Source/SkellComponents.h defines a custom segmented vertical meter that
  precisely follows the plugin's border artwork.
- Source/SkellLookAndFeel.h already provides custom knobs, switches,
  responsive text sizing, tooltips and preset menu styles.

**Safe pilot:** preserve Delay's preset browser and custom border meters.
Reusing SkellCore's portable MeterBallistics/LevelBridge is possible
if tested, but the general-purpose SkellStereoMeter must not replace its
shape. Introduce shared accessibility semantics, safer primitives or
theme preference isolation before visual replacements.

**Critical regression:** preset loading/saving, favorites, search/filter,
undo/redo, timing/feedback automation, custom meter geometry and Windows
host CPU. Never alter audio processing or preset file identifiers during
a visual integration pass.

## Cross-product adoption policy

1. Keep one master skull family *with per-product identity*; do not copy
   Skell Key's skull or generic Quickstart artwork into another product.
2. Shared code is a library, not a mandatory global editor. Product
   skins, layout, graph, preset features and unusual meters remain owned
   by each product.
3. Preserve every VST3 product/bundle/manufacturer code, APVTS parameter ID
   and DAW state encoding. Any identity change breaks prior sessions.
4. Avoid conflicting FetchContent checkouts and mixed JUCE dependencies.
   Test the target plugin's existing JUCE version independently.
5. Baseline current UI with the fast local renderer: only 4 scenes
   (main, menus, alternate skin, small size) plus separate build validation.
6. Make one isolated pilot integration at a time; compare FL Studio
   saved projects, automation, crash and CPU behavior before merging.
7. No universal skull, power, knob, settings, preset or meter design
   should be locked without explicit design approval.

## Next integration decision

Start with low-risk infrastructure (appearance-store isolation / shared
interface contracts) in an EQ side branch only *after* the current
SkellCore PR and manual FL Studio acceptance are resolved. The Delay
pilot should explicitly keep its specialized preset browser and meter.

Host gate: https://github.com/skellyruno/skellcore-vst-template/issues/29
SkellCore draft PR: https://github.com/skellyruno/skellcore-vst-template/pull/26

# SKELL unified identity and controls (v1)

**Goal:** plugins must feel related without inheriting Skell Key's identity.
SkellCore supplies *parts and rules*, not a one-size-fits-all editor. The
default mark is a neutral geometric skull. Every plugin can add an element
without changing the skull silhouette or adopting another plugin's logo.

## Shared versus individual

| Shared from SkellCore | Chosen by each plugin |
| --- | --- |
| 48x48 master skull and outer shield proportions | Forehead element (key, tuning note, EQ bars, clock, cast, fan, custom) |
| Matte-hardware rotary knob renderer + value entry | Knob count, labels, ranges, reset values and arrangement |
| Power toggle appearance and 32px settings gear | Actual bypass parameter, on/off logic and menu content |
| Embedded rounded settings surface | Tabs, theme, resolution, oversampling, Help, tutorial, license as relevant |
| 64-point envelope line and lock-free level handoff | Where line appears, whether it is displayed, accent colour, level source |
| Palette and contrast practices | Product identity, special controls and feature layout |

### Skull marks

Include <SkellCore/SkellIdentity.h> in the editor. Do **not**
call drawSkullKeyMark() for new general-purpose plugins. That legacy function
remains in <SkellCore/SkullTheme.h> for existing Skell Key consumers.

    skellcore::ui::SkellSkullButton brand;
    brand.setElement(skellcore::ui::SkullElement::Equalizer); // Skell EQ
    brand.setBrandColours(skellcore::ui::SkellBrandColours::neon());
    brand.onClick = [this] { cyclePluginTheme(); };
    addAndMakeVisible(brand);

Built-in elements: Core, Key, Tune, Equalizer, Delay, Silence, Cast, Fan,
Master, Custom. The brand can be made unique without changing its
outline. To add an uncommon plugin element, call
setCustomElement([](juce::Graphics& g) { ... }); which receives the 48x48
design coordinate space. Keep custom artwork confined to the forehead; do not
erase or replace the base eyes, nose, jaw, teeth or shield.

**Existing plugin marks are not automatically replaced.** Migrate one real
plugin editor at a time and preserve its existing appearance until approved.
Skell Silence's original shushing mark and Skell Key's long skull-key are
intentionally still present for backwards compatibility. A future visual
migration can align them with the master skull without breaking code.

### Knobs

<SkellCore/SkellUnifiedControls.h> supplies SkellHardwareLookAndFeel.
This reuses the existing drawHardwareKnob() in SkellHardwareUI.h and the
existing Studio palettes, so no duplicated metal knob geometry is needed.

    skellcore::ui::SkellHardwareLookAndFeel look {
        skellcore::ui::StudioSkin::Obsidian };
    setLookAndFeel(&look);
    skellcore::ui::configureStudioKnob(retune, 18.0);

Call setLookAndFeel(nullptr) in the editor destructor **before** the LAF
member is destroyed. Keep JUCE APVTS SliderAttachments on original controls.
No changes to parameter IDs, ranges, DSP or project states are required.

### Power

SkellPowerButton is a real juce::ToggleButton, not a decorative icon. Bind it
to an actual plugin enable parameter using an APVTS ButtonAttachment, or a
tested per-plugin bypass implementation. Do not add a fake button that merely
lights up while audio continues unchanged.

    skellcore::ui::SkellPowerButton power;
    // 'enabled' is a hypothetical plugin parameter: use the real ID.
    // juce::AudioProcessorValueTreeState::ButtonAttachment attachment(
    //     processor.apvts, "enabled", power);

The core intentionally does not add a parameter to existing plugins. New
plugin templates should define an explicit enable/bypass parameter if the
creator wants the power control included by default.

### Settings

SkellSettingsButton draws a common gear, and SkellSettingsSurface draws a
matching embedded panel. The editor owns its settings and callback:

    skellcore::ui::SkellSettingsButton gear;
    skellcore::ui::SkellSettingsSurface panel { "SKELL DELAY SETTINGS" };
    // In the editor constructor:
    addAndMakeVisible(gear);
    addAndMakeVisible(panel);
    panel.setVisible(false);
    gear.onClick = [this] { panel.setVisible(!panel.isVisible()); };
    // Add *real* settings controls as children of panel.

**Never substitute the DAW host's default options dialog as the plugin
settings interface.** Avoid universal fake settings: only show controls
actually supported by each plugin. Settings are part of the standard new-
plugin visual kit but a project can intentionally remove the gear and panel
for a tiny single-purpose tool.

### Shared audio visual line

Portable <SkellCore/LevelBridge.h> does not depend on JUCE and can be used
in the processor. It publishes peak scalars with atomics and retains the
maximum until the editor reads it, to avoid losing transients between frames.
Only GUI code uses the visual <SkellCore/SkellUnifiedControls.h>:

    // Processor (after applying output gain, audio thread):
    // peakBridge.publish(clampedFiniteOutputPeak);

    // Editor timer at ~24Hz (message thread):
    levelLine.pushLevel(peakBridge.take());

This is a common **visual language** (small audio-feedback line), not a
feature that must always occupy the same location. It may be removed when a
plugin has a more useful spectrum, waveform or piano view. It never
resynthesizes audio, records input or changes host latency.

## Default versus choice

- **Default in a new Skell plugin design:** neutral master skull, matched
  hardware knobs, real power toggle when bypass exists, plugin-owned
  settings gear/panel, and the lightweight audio line.
- **Intentionally removable per plugin:** the power button if no power
  semantics exist, settings panel for ultra-simple tools, or the audio line
  if it duplicates a primary visualizer.
- **Never forced by simply linking SkellCore:** UI components are explicit
  editor instances. An old plugin can link DSP without changing its brand.
- **Guided Tour:** remains separately opt-in for complex plugins only.
- **Colours:** plugin skin and accents may differ; master skull geometry,
  control proportions, icons and interaction affordances stay familiar.

## Migration checklist

1. Pin a known SkellCore revision; do not mix headers from unrelated commits.
2. Replace any accidental drawSkullKeyMark calls in non-Key editors with
   SkellSkullButton or drawSkellSkull(...).
3. Use the existing APVTS attachments for knobs and any real bypass parameter.
4. Put settings into the product's own settings panel; no host-generic UI.
5. Feed the line only through the scalar meter bridge, never directly from
   JUCE GUI code inside processBlock.
6. Test small/default/large renderer sizes, all supported skins, click targets,
   keyboard and mouse interaction, GUI repaint at idle and under load,
   host automation, CPU, latency and saved-project recovery.

## Compatibility and implementation status

The Skell Quickstart example adopts the neutral master skull, hardware knob
finish, embedded settings panel for its own UI preference, and a real signal
peak line. Its original gain parameter ID and automation semantics remain
unchanged. The reusable power control is provided, but the **demo itself does
not invent a new bypass parameter** just to display a power symbol.

There is no global migration of existing VST3 editors in this change. Real
Windows / FL Studio validation is required for each adopting product.

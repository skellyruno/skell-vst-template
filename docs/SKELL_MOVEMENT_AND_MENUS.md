# SkellCore Movement + plugin-owned themed menus (development)

## Source and intent
SkellMovement adapts the deterministic xorshift burst logic from **SkellGlitch/ui/GlitchPulse.h**.
It does not replace that plugin's own header, artwork, animation, saved project
state, CPU path or current UI. Reuse is opt-in for future or explicitly approved
migrations. No master skull, default menu page or plugin settings is imposed.

## Portable, zero-allocation visual schedule
\`\`\`cpp
#include <SkellCore/SkellMovement.h>
skellcore::ui::SkellMovement movement; // 30 Hz profile, same as SkellGlitch
// In a JUCE editor's message-thread timer callback:
if (movement.tick()) repaint(glitchBounds);
auto strength = movement.intensity();   // 0 or 1..3
auto seed = movement.visualSeed();       // deterministic variation
movement.setReducedMotion(userPrefersLessMotion);
\`\`\`
Only the UI/message thread calls tick(). It performs no dynamic allocation,
locks, file I/O or DSP access. An idle tick returns false, avoiding spurious
repaints. For other cadence, configure ticks in SkellMovementConfig.

## Opt-in JUCE adapter
\`\`\`cpp
#include <SkellCore/SkellMovementJUCE.h>
// Member of your editor; the editor owns it.
skellcore::ui::SkellMovementAnimator visual;
visual.setDirtyArea({0, 0, 180, 80}); // repaint just the visual, not the mixer
visual.start(*this, 30);             // no timer until this call
visual.setReducedMotion(true);       // instantly remove animated state
// stop() in editor teardown if desired; destructor already stops timer.
\`\`\`

## Product-specific menu style, not host defaults
\`\`\`cpp
#include <SkellCore/SkellMenuTheme.h>
skellcore::ui::SkellHardwareLookAndFeel look;
auto p = look.palette();
p.accent = juce::Colour(0xff46de53); // product identity, not a global skin
look.setPalette(p);
combo.setLookAndFeel(&look);          // unbind before look is destroyed
juce::PopupMenu menu;
menu.addItem(1, "Preset A");
skellcore::ui::styleSkellPopup(menu, look);
menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(combo),
                   [/* editor-owned safe callback */](int result) { /* ... */ });
\`\`\`
For an existing custom LookAndFeel, call
\`applySkellMenuPalette(existingLook, productPalette)\` without replacing it.
Use the same custom palette for settings surfaces and preset browsers.
Menu callbacks must not capture already-destroyed editors; keep look objects
alive until an asynchronous menu closes. Do **not** mutate host/DAW-native
settings dialogs, plugin processor parameters or APVTS IDs.

## Adoption / acceptance
- Portable tests compare 100,000 ticks exactly with the original GlitchPulse.
- UI checks paint dropdowns/popup items across four skins, exercise custom
  accent overrides, verify no timer starts implicitly.
- Real generated VST3 and GUI smoke must compile against the pinned SkellCore
  integration revision before any broader rollout.
- Honor accessibility/reduced-motion preferences and run animations at UI
  refresh rates, independent from sample rate and realtime DSP deadlines.
- Existing plugins retain their current icon, knobs, settings and menus until
  an explicit product migration is tested and approved.

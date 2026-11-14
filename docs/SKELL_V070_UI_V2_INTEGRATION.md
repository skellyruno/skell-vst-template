# SkellCore v0.7 DSP + UI v2 integration — development compatibility contract

This branch deliberately combines the two existing development efforts without merging either one to `main`. Parent: UI v2 draft PR #26, head `7d291f1741510e649439e6a7e06575a2744099bb`. Source: v0.7.0 DSP draft PR #30, verified Windows/Ubuntu/sanitizers at `7694ffee`.

## What is included
- `SkellCore/include/SkellCore/Core.h` and `SkellCore/tests/test_limiter_parity.cpp` from v0.7.0. The generic limiter *behavior changes*; existing plugin IDs do not. No automatic rollout to Skell Master or other shipped DSP.
- `FDN.h` and `test_fdn.cpp` (8/16-line reverb, original 4-line FDN preserved).
- v0.7.0 performance guide, version increment to 0.7.0, and FDN CMake regression target.
- UI v2 foundation left intact: neutral skull-family, product-scoped appearance store, preset bar, stereo meters, responsive grid, guided tour, local SkellWorld tuning and existing host lifecycle matrix.

## Compatibility gates
- Portable DSP test suites must pass on Linux/Windows and Linux ASan+UBSan.
- Template Windows VST3 and Standalone must compile, real JUCE renderer must capture all scenes, and JUCE processor state/automation/8-16 instance tests must pass.
- Generated plugin VST3 must compile against the **new integrated revision** after immutable pin is updated; do not accidentally verify an older source pin.
- No swapping the new reverb into shipped plugins or assuming a certified true-peak limiter until real recorded-audio A/B and an external inter-sample peak check.
- FL Studio saved-project, automation, latency and real CPU tests remain **manual/unverified** until an exact Windows VST3 build is checked.

## Boundaries
Do not merge either draft into production automatically. This branch is a *reconciliation candidate* against PR #26, not an instruction to overwrite it. Preserve all existing parameter IDs, presets, generator semantics, product artwork and user-approved interfaces.
## Generator pinned-SHA check

The template's `tools/new_plugin.py` now pins **`ace10c61835a7622fbaf58067d4ed74042261427`**, the first immutable integration commit, instead of the older UI-only commit. The generated-plugin Windows CI compiles the v2 UI headers and the v0.7 FDN headers from that exact pin; this prevents a misleading pass against an older release. Product identity and generator validation remain unchanged.

## Contrast bug fix and performance baseline — 2026-11-14

The headless renderer caught a barely visible combo-arrow on the light Ivory skin. Root cause was a missing `juce::ComboBox::arrowColourId` in the shared `SkellHardwareLookAndFeel`. The shared arrow now uses each skin's high-contrast foreground colour; a real JUCE renderer regression checks all four skins. No skull, knob geometry, parameter IDs or DSP are changed by this visual bug fix.

The portable CPU benchmark now reports the opt-in FDN 8/16 variants' per-frame processing cost and estimated memory, without threshold-based CI failure. Prior independent GCC 14.2 single-machine measurements were about 26 ns (8-line) and 54 ns (16-line) per stereo frame at 48 kHz; CI runner measurements must be recorded separately and are not FL Studio CPU figures.

The generated plugin source pin is now immutable commit `75d45942bc2343b837217b123e2538372d409cbf`, which contains both the v0.7 DSP + shared UI and the readable arrow fix.

CI workflows now share one automatic concurrency group per source branch to avoid duplicate push/PR jobs, but use unique groups for manual, main, production, release and tag runs so those protected runs are never silently cancelled.

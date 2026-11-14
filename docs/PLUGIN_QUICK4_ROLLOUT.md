# SKELL QUICK4 — LOCAL UI RENDERER ROLLOUT 🩻
Last reviewed: 2026-11-13

## Rule when the user asks "current UI" of ANY Skelly plugin
1. Find that plugin's actual GitHub source repository and exact current revision.
2. Use the **real JUCE AudioProcessorEditor**, built locally from that source and captured under Xvfb; no concept art, no ImageGen, no rendering from audio DSP WAV utilities.
3. Capture **four** meaningful views by default: main, relevant real control/menu state, active state, alternate *supported* skin or other real state. When a plugin has only one skin, don't fabricate one. A selected ComboBox isn't an open menu.
4. Show a real PNG in chat, with source/build status. **If source or dependencies are unavailable, say so explicitly.** Do NOT substitute stale previews and call them current.
5. Maintain Windows x64 VST3, actual DSP and FL Studio tests as a separate gate. Routine screenshots are local. Manual-only CI screenshot workflows may be used only for a deliberate deep audit.

## Real renderer build targets (machine-readable router: `tools/quick4_projects.py --list`)

| Plugin source repo | Local CMake flag | Renderer target | Actual local capture status |
|---|---|---|---|
| skell-master-vst | `SKELLY_MASTER_BUILD_UI_RENDERER` | SkellyMasterUIRenderer | **VERIFIED** source recompilation + Quick4 |
| skelltune-mini | `SKELLY_BUILD_UI_QUICK4` | SkellyTuneQuick4 | **VERIFIED** local native C++ compile, Quick4 **4/4**, optional 8/8 |
| skell-silence-vst | `SKELL_SILENCE_BUILD_UI_RENDERER` | SkellSilenceUIRenderer | **WIRED**, native regression needs verification |
| skell-deesser-vst | `SKELLY_DEESSER_BUILD_UI_RENDERER` | SkellyDeEsserUIRenderer | **WIRED**, native regression needs verification |
| skell-preset-vst | `SKELL_PRESET_BUILD_UI_RENDERER` | SkellPresetUIRenderer | **WIRED**, native regression needs verification |
| skell-flang-vst | `SKELLFLANG_BUILD_UI_SMOKE` | SkellFlangUiSmoke | **VERIFIED**, real native C++ Quick4 4/4 at 2.77s |
| skell-fan-vst | `SKELLFAN_BUILD_UI_QUICK4` | SkellFanQuick4 | **WIRED**, native compiler verification pending |

**Wired ≠ built/tested.** Silence, DeEsser, Preset, and Fan have code paths ready but require local C++ compile + capture validation. Flang and Mini now passed local compilation and actual 4/4 screenshot tests; screenshots don't prove Windows VST3 success. Screenshot-only GitHub workflows (where present) are manual-only.

## Execute from one local workspace

Check out source (or use a verified offline source archive), supply pinned JUCE 8 and SkellCore headers, source the isolated Linux development overlay if running in a restricted container:

```bash
python3 tools/quick4_projects.py --list
python3 tools/quick4_projects.py \
  --plugin skelltune-mini \
  --project /path/to/skelltune-mini \
  --juce /path/to/JUCE-8.0.12 \
  --skellcore /path/to/pinned-skellcore-vst-template \
  --output /tmp/mini-quick4
```

This script **actually configures and compiles** the native renderer and validates the manifest, four PNGs, and contact sheet; it refuses to silently reuse old binary previews. `--full` is deliberately nondefault. Use `--jobs 1` on 13GB or constrained memory systems. Skell-Preset has additional pinned headers-only dependencies; in fully isolated environments provide those via repeated `--define FETCHCONTENT_SOURCE_DIR_...=<local path>`. Do not claim complete offline compilation of those repos until proven.

**Next integrations:** Validate Silence/DeEsser/Preset/Fan native compilation, then wire EQ, Key, Spacer, Delay, Afar, Separator, AdlibLab, Necromancer, Chess, Choir, Cast, Scratch, Stop, Glitch/Gate, others. Each plugin needs a verified real renderer target and per-plugin scene semantics; copying a generic mock screenshot is not a migration.

## Mini screenshot audit result

The Mini CMake target is **new** and not named `tools/render.cpp` (that tool exports WAV audio). Verified directly on Linux with pinned sources and the isolated development overlay. Quick4 captured: `main_default_432.png`, `key_scale_selected_432.png`, `retune_active_720.png`, `custom_notes_432.png`, `contact-sheet.png`, and `manifest.json`. Full eight-view responsive audit also passed. On the compiled binary Quick4 took approximately **1.3 seconds**; eight scenes approximately **2.5 seconds**. Initial C++ compilation was significantly slower. The real UI remains neon-green-only by design. Windows/FL Studio release validation is separate.

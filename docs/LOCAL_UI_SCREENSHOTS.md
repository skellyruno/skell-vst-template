# SKELL shared local UI screenshot policy (2026-11-12)

This is the default workflow for **all Skelly JUCE plugins** as each plugin adopts the shared renderer. These instructions preserve the owner's preferences; they do not claim every repository has already been migrated.

## Fast design loop (default)
1. Edit the plugin's UI/DSP source.
2. Build its native development UI renderer **locally** (not a Windows release build).
3. Capture **four** genuine live-editor scenes locally using SkellCore HeadlessUIRenderer and Xvfb, without GitHub Actions:
   - MAIN: current default size, readable at a glance.
   - CONTROLS / MENU: key controls, factory preset selection or a genuinely open menu **when capture tooling supports it**. Never fake an open popup by painting it into an image.
   - ACTIVE: meaningful real parameter/activity state.
   - ALTERNATE: one alternate colour/theme.
4. Inspect the four PNGs (including layout, labels, controls, contrast); fix problems and repeat. Do not generate mockup images or present old binary captures as current source.
5. Full deep UI audit (all sizes/skins) **only when requested**, before release, or after major responsive layout changes.

## Release engineering (SEPARATE)
- **Windows x64 VST3 compilation, real DSP correctness, automation, loading, silence/NaN/latency/CPU checks** remain independent mandatory validation.
- GitHub can remain source control / fallback CI for Windows builds, but **do not trigger a GitHub screenshot run for routine UI iteration**. No screenshot pass is evidence that Windows VST3 compiled or works in FL Studio.
- An exported Linux screenshot executable is immutable at its compiled commit. To preview new source, it MUST be rebuilt. A local runtime without JUCE/SkellCore development dependencies can only replay the existing binary.
- Test actual menus by rendering/photographing the real popup; an ordinary ComboBox with a chosen item is NOT an open menu.

## Generic runner contract
A plugin-specific CLI renderer accepts `--output <folder>` and defaults to four scenes; `--full` opts into its exhaustive full list. It writes `manifest.json`, `contact-sheet.png`, and the original captured PNGs. Names and configurations may differ per plugin.
The generic local runner checks subprocess exit code, exact count, sizes, contrast, nonblank images, PNG file presence, and writes a timed validation report. Prefer fresh folders or explicitly clear old screenshots between captures.


## Locally compiling changed source (not merely replaying a binary)
Run `python3 tools/skell_ui_rebuild.py --project /path/to/plugin --juce /path/to/JUCE-8.0.12 --skellcore /path/to/skellcore-vst-template --output /path/to/quick4`.
The local Linux build requires the C++ toolchain, Ninja, CMake, Xvfb, JUCE and SkellCore source, and the relevant system development headers/libraries (typically FreeType, Fontconfig, ALSA, X11 and GUI dependencies). This runner invokes CMake + an actual per-plugin renderer target before taking any screenshots. **No fallback to stale binaries.** For plugins not yet wired to a renderer target, add that target before claiming local rebuild support.

The reusable pinned JUCE and SkellCore sources may be bootstrapped once on a network-enabled machine; do not depend on GitHub Actions for routine screenshots. Initial builds are significantly slower than subsequent captures; screenshots of a precompiled editor have been measured at ~1.7 seconds for four scenes in the reference container, but fresh C++ builds vary by machine and changes.


## Local source-compilation proof — SKELL Master M-01 (2026-11-13)
A fully isolated **Debian 13** compiler environment now supports real JUCE **8.0.12** + pinned SkellCore source without a network dependency at build time. We used a one-time export of Ubuntu development *headers, pkg-config metadata, and link-only symlinks*; stored them in a scratch sysroot and resolved the linker shims against the container's existing Debian runtime libraries. We intentionally **did not** prepend Ubuntu's full `/usr/include` (that overrides the Debian glibc/pthread ABI). The `CMAKE_CXX_FLAGS_RELEASE=-O0 -DNDEBUG` development-only option speeds the editor build; Windows release validation is unchanged.

**Verified real source edit** in `skellyruno/skell-master-vst`: commit `de943ac5` changes preset-window glass reflection and adds a tiny skin-linked engraved line. A locally compiled native `SkellyMasterUIRenderer` captured four PNGs at 0 errors. Same-machine before/after pixel comparison found visible changes around the preset UI, with stable four-scene dimensions. Incremental C++ rebuild took **8.7 s** and subsequent Quick4 capture **2.9 s** (not including release build). This workflow must be independently adopted/tested in each plugin: do not imply that all plugin repos already have native render targets. Continue keeping Windows VST3/DSP/FL Studio validation separate.

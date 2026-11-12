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

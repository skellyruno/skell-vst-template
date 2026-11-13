# SkellCore / FL Studio host acceptance (Windows VST3)

This checklist is a manual **release gate**, not a substitute for continuous
integration. The JUCE processor smoke tests instantiate Quickstart directly;
they do not prove actual FL Studio host compatibility. Record a pass only
after testing the latest compiled and identified Windows VST3 binary.

## Reproducible acceptance materials

Generate an original WAV and a results sheet from the repository:

    python tools/Make-FLStudio-TestKit.py --out host-test-kit

Or use the downloadable SkellCore FL Studio acceptance-kit ZIP. The WAV
contains an original 12-second 48 kHz stereo signal (different L/R tones),
silent segments and transient probes. Import it into a clean FL Studio project
without normalization or other effects. Fill in results.csv while testing.

For **local Windows CPU baselines** on your own Ryzen CPU, run:

    powershell -ExecutionPolicy Bypass -File .\\tools\\Run-SkellHostValidation.ps1

That creates a 36-row matrix and a binary hash. The numbers do not measure
FL Studio's audio thread; the real FL Studio CPU fields in results.csv must
still be collected while the DAW is running. See
docs/LOCAL_WINDOWS_HOST_VALIDATION.md for details.

## 1. Exact binary under test

- Commit SHA: `____________`
- VST3 bundle filename/path: `____________`
- FL Studio version/build: `____________`
- Windows version: `____________`
- CPU/RAM: `____________`
- Audio driver: `____________`
- Sample rate / buffer size: `____________`
- Baseline FL Studio project CPU with plugin removed: `____________`

Do **not** install a differently compiled VST3 under the same display name
and claim the old binary's results apply. Unload all previous instances,
install the exact artifact, rescan plugins, and restart the host if needed.

## 2. Plugin discovery and GUI lifecycle

1. Open FL Studio plugin manager, scan the new VST3 and confirm **Skell
   Quickstart** is found, with no duplicate plugin entries.
2. Load the VST3 on a mixer insert and verify stereo audio passes.
3. Repeatedly open/close the editor (at least 20 cycles); observe no crash,
   blank UI, orphaned popups, unexpected network activity, or stuck timer.
4. Test editor at 390x250, 480x330, and 840x560 and at the display's
   normal Windows scaling. Controls must remain reachable and legible.
5. Confirm SkellWorld remains OFFLINE unless the user explicitly opts in.

## 3. Automation and persisted state

1. Set Gain to 1.625 and Enabled to on; run audio until gain stabilizes.
   Compare level against the unprocessed reference (expected ~1.625x).
2. Automate Gain from 1.0 to 0.2 to 1.8, including fast moves while audio
   plays. Listen for sharp clicks unrelated to source transients; confirm
   final values match the automation.
3. Toggle Enabled off. The plugin must smoothly bypass gain and pass
   clean, **unmuted** dry audio. Toggle back on and confirm smoothing.
4. Save project with the plugin Enabled off and Gain at 1.625; close and
   reopen FL Studio, reopen the project, and confirm both parameter values.
5. Save while Enabled on, change controls, reload the earlier project and
   confirm the saved state returns. Undo/redo and automation must not corrupt
   state or silently duplicate plugin parameters.
6. Presets/settings stored as UI preferences must not replace or overwrite
   the host's actual song/project parameter state.

## 4. Audio & multi-instance load

Test each row at 44.1, 48, and 96 kHz where the system supports it.

| Instances | Host buffer sizes | Required outcomes |
| --- | --- | --- |
| 1 | 64 / 128 / 512 | Stable audio, zero reported added latency |
| 4 | 64 / 128 / 512 | No crashes, no unexpected parameter cross-talk |
| 8 | 64 / 128 / 512 | Stable playback and reopen, no shared-state interference |
| 16 | 128 / 512 | Record CPU and glitches; do not presume all PCs sustain it |

For each case, record baseline CPU with all instances bypassed, CPU with
all enabled, project peak CPU during active Gain automation, audible
dropouts/glitches, and session reopen behavior. Record the actual FL Studio
audio/CPU monitor reading rather than treating CI microbenchmark nanoseconds
as host CPU measurements.

Monitor output for silence, unexpected levels, clips, stuck notes, denormals,
and audio artifacts. A 0-sample *reported* latency does not establish zero
real-world input-to-output driver round-trip latency.

## 5. Expected automated evidence (GitHub CI)

- Quickstart JUCE processor host-readiness runner must pass parameter
  roundtrip, automation, editor lifecycle and 8-instance signal stress.
- Portable SkellCore sanitizer tests must remain green.
- The Windows VST3 binary and standalone executable must build.
- The screenshot renderer must continue to capture all 13 expected scenes.
- The generated-plugin VST3 smoke must still compile the v2 shared controls.

**Current result:** FL Studio manual tests **not yet verified**.
Only change this line with actual host evidence and build commit SHA.

## 6. Release sign-off

- [ ] Real FL Studio discovery/load passed
- [ ] Automation and state recovery passed
- [ ] 1/4/8/16 instance checks completed; CPU numbers recorded
- [ ] UI small/default/large tested and approved
- [ ] Final skull / power / knobs / theme approved
- [ ] Windows VST3 exact release binary matched to CI test SHA
- [ ] No remaining severity-1 defects; PR reviewed before merge

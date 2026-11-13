# SkellCore Windows Host-Readiness Verification — 2026-11-13

## Automated release candidate status

**All newly added automated host-readiness checks passed**, with one
remaining **manual FL Studio acceptance gate**. The work is in draft PR #26;
no existing plugin has been automatically migrated or merged.

| Verification | Code / Run | Result |
| --- | --- | --- |
| Portable SkellCore DSP, Windows + Ubuntu and Linux sanitizers | 43375487375 (f526c02) | PASS |
| Real-editor 13-scene screenshot runner | 43375476704 (094aa81) | PASS |
| Fresh generated Windows VST3 + shared UI v2 headers | 43375476796 (094aa81) | PASS |
| Windows VST3 and Standalone build + actual JUCE processor host smoke | 43375506494 (6074178) | PASS |
| External pluginval Windows VST3 validation | 43375959132 (506bdfd) | PASS |
| Real user-machine FL Studio load/automation/song recovery | Requires manual host run | NOT VERIFIED |

Commits after the tested source changes are documentation-only or add the
pluginval workflow. The plugin source and CMake host runner were unchanged
between the Windows host smoke and pluginval validation runs.

## What the real JUCE host harness tested

Source: `tools/SkellCoreHostReadinessMain.cpp`.
It builds against the **actual Skell Quickstart AudioProcessor and editor**:

- Stereo layout acceptance; mismatched input/output buses rejected.
- 0 reported plugin-added latency.
- Parameter automation (Gain, Enabled) through real JUCE parameter APIs.
- Real output gain convergence after smoothing.
- Output peak bridge reads correct finite levels and clears on drain.
- `getStateInformation` / `setStateInformation` roundtrip into a fresh instance.
- Enabled=off state persistence; smooth dry unity bypass instead of muting.
- Malformed host-state input does not change the saved parameters.
- Editor create/destroy x4, resizing and subsequent audio processing.
- Eight simultaneous independent Quickstart processors with automated bypass
  during simulated 48 kHz / 128-sample blocks; outputs stayed finite.

CI measurement: **8.218 ms wall time**, 8 serial instances, **1.6 seconds**
of simulated audio, measured ratio **0.00514**, sum checksum 0.77775.
This number also includes test signal creation and validation; it is **not**
per-plugin CPU percentage, FL Studio CPU usage, or end-to-end latency.

## External VST3 binary validation

Pluginval version **v1.0.4**, Windows, strictness **5**, GUI tests skipped,
run in-process against the actual packaged `Skell Quickstart.vst3`.
The pinned download is used **only in CI** and is not distributed in plugins.

Pluginval reported **SUCCESS** after audio processing, state,
automation, parameter enumeration, bus layout and enable/disable checks
at 44100 / 48000 / 96000 Hz and several block sizes.

**Scope caveat:** pluginval printed
"INFO: Skipping vst3 validator as validator path hasn't been set".
Therefore, do not claim the standalone Steinberg SDK VST3 validator ran.
Also, this is **not a substitute for testing inside FL Studio**.

## Artifacts

- SkellCore-Template-Windows-VST3 (exact run 43375506494)
- SkellCore-Template-Windows-Standalone (same run)
- SkellCore-Host-Readiness-Report (same run)
- SkellCore-pluginval-Windows log artifact (run 43375959132)

Verify the build commit SHA and artifact zip prior to FL Studio installation.
The default repo branch was not updated; PR #26 stays in draft.

## Remaining acceptance steps

See `docs/FL_STUDIO_HOST_ACCEPTANCE.md` for explicit host and
multi-instance test matrix. Validate the exact artifact under Windows
FL Studio, automate, save/reopen, test at 1/4/8/16 instances, and record
observed CPU/dropouts. The final skull, knob, power-icon and theme design
choices remain pending. Do not merge or force a universal product theme.

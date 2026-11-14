# SkellCore Windows host matrix: first verified baseline

Windows CI: https://github.com/skellyruno/skellcore-vst-template/actions/runs/43401089674
Commit: abcbe36e93092489551013d52d373d9b72465dc4 (November 13, 2026)

**Passed:** 36/36 direct JUCE processor combinations plus mono layout,
state restore, gain automation, bypass and editor open/close. Windows x64
VST3 and Standalone packages were compiled successfully.

Exact x64 VST3 binary SHA-256, independently compared with its manifest:

    FA5105966FCA8906008737B5B02988B9A3E5CE975B0ABD20C63205B54976D9E2

## Serial processing CPU benchmark

The serial ratio is processBlock CPU time divided by simulated audio time
for all instances. It is **not** FL Studio CPU, audio driver latency,
UI compositing cost, or a realtime deadline guarantee.

| Instances | Highest measured serial ratio | Scenario |
| ---: | ---: | --- |
| 1 | 0.000278 | 96000 Hz / 64 samples |
| 4 | 0.001159 | 96000 Hz / 64 samples |
| 8 | 0.002658 | 96000 Hz / 64 samples |
| 16 | 0.005005 | 96000 Hz / 64 samples |

The separate eight-instance test including input generation and output
validation took 11.273 ms for 1.6 seconds of simulated audio. Its timing
is not directly comparable to the processBlock-only matrix.

## Enhanced test pending

Follow-up work adds per-sample finite checks, explicit silent blocks,
independent Gain/Enabled expected-state assertions, and p95/max
per-processBlock call timings. This baseline predates those changes.
Do not certify the enhanced matrix until the newer Windows run passes.

## Release gates

Actual FL Studio plugin discovery, project save/reopen, CPU/dropouts,
and final user-approved skull/knob/power/theme remain unverified.
See: https://github.com/skellyruno/skellcore-vst-template/issues/29
Draft: https://github.com/skellyruno/skellcore-vst-template/pull/26

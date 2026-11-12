# SKELL UI performance baseline — November 12, 2026

These are **informational GitHub-hosted CI microbenchmarks**, not measurements
from the user's AMD Vega 8 / Ryzen PC and not end-to-end DAW latency.
Captured by the new SkellCoreMeterBenchmark on the v2 development branch
at commit c59c521100c4350bb81411fd2652fdf3bc31735b.

| Build | Peak-bridge publish/drain pattern | MeterBallistics advance | Workload |
| --- | ---: | ---: | --- |
| Windows Release x64 CI | 4.14 ns/iteration | 4.07 ns/iteration | 1,000,000 iterations |
| Ubuntu Release CI | 4.16 ns/iteration | 3.53 ns/iteration | 1,000,000 iterations |

Source: GitHub Actions portable DSP tests, run #43329759144.
Both OS jobs passed all 16 CTests (including MeterBallistics, LevelBridge
and SkellCoreMeterBenchmark), and the Linux ASan+UBSan job passed.

## Interpreting the measurements

- Peak-bridge test performs atomic peak publishes with periodic drains. It
  does not benchmark true-peak computation or audio-buffer scanning.
- Meter ballistics test updates a scalar display envelope and peak hold.
  It does not include repaint, GUI event routing, automation or whole-editor
  cost. The headless renderer separately prints stereo-meter paint time.
- Absolute timings depend on runner hardware, OS, compiler and load; only
  compare them under roughly identical conditions, preferably reporting
  multiple samples or a distribution over time. No release threshold is
  based on this baseline.
- No product audio latency or FL Studio CPU percent can be inferred from
  these values. Real multi-instance projects and FL Studio require their
  own measurements.

## Regression and future optimization

Retest with -DSKELLCORE_BUILD_BENCHMARKS=ON. Keep the workload constant,
record the commit SHA, runner, iteration counts, and result. Record a
regression only if repeatable under comparable conditions; avoid overfitting
to noise in 1-2 measurements. The current final screenshots and generated
VST3 checks are separate release gates, not benchmark inputs.

## Offscreen stereo-meter paint baseline

The corrected Windows JUCE headless-renderer run #43332134308 measured
**924.84 microseconds per synthetic frame** across 500 offscreen paint
iterations at 220x36. This measurement includes creating/closing a JUCE
Graphics context for each frame and must NOT be interpreted as GPU cost,
FL Studio CPU usage, real-time audio latency, or a sustained per-plugin load.
Host compositing, JUCE rendering backends, DPI and the plugin window's
visibility all affect the result. Use this figure only as a starting
point for like-for-like headless renderer comparisons.

The same renderer successfully captured **13/13** UI/guided-tour scenes,
tested preset selection/callbacks, stereo meter paint and responsive bounds.

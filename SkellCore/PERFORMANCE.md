# SkellCore low-CPU / safety profile (v0.3 candidate)

SkellCore DSP is for real-time audio. No heap allocation, file reads, mutexes or image rendering should occur in processFrame/processBlock. Always call prepare while the audio thread is suspended.

## Two limiter profiles

| Profile | Interpolation detector work per stereo sample | Purpose |
| --- | ---: | --- |
| Economy | 2 phases x 8 taps x 2 channels = 32 multiply-accumulate iterations | Lower-cost preview and many-instance chains |
| Standard (default) | 4 phases x 16 taps x 2 channels = 128 iterations | Detection unchanged; gain stage rewritten in v0.6.1 (see below) |

Economy reduces detector arithmetic, **not** total plugin CPU by exactly 75%. Neither profile is certified BS.1770 true-peak compliant. For final mastering, the limiter requires further verification with intersample peak reference vectors. The sampling latency remains 3 ms in either profile. Always report limiter.latencySamples() to the host during preparation.

Choose the profile in prepareToPlay while stopped (do not change quality within audio callback):

    skellcore::TruePeakLimiter limiter;
    limiter.prepare(sampleRate, 2,
        skellcore::TruePeakLimiter::Quality::Economy);
    const auto hostLatency = limiter.latencySamples();

## Core safety fixes

- Before prepare, FDNReverb and TruePeakLimiter return sanitized dry frames instead of dereferencing uninitialized buffers.
- Sample rates are sanitized and capped at 192 kHz in portable foundation components to reject corrupt values and unbounded allocations.
- Reverb caches gain multipliers outside the per-sample hot path and flushes extremely small damping state to zero.
- Limiter uses a mirrored history ring, removing modulo operations from each FIR tap.
- UI GlowCache rejects abnormal image sizes (>1,572,864 pixels or >2,048 on an axis) and reuses identical cached images, preventing huge memory allocations during resize.
- Existing Release-mode tests have NDEBUG explicitly undefined so assert() actually tests behavior in CI.
- A low-CPU safety regression checks no new/delete calls from reverb/limiter processing, multiple rates, NaN/Inf input and unprepared processing.
- `tests/test_limiter_parity.cpp` (name kept for build files) now checks the limiter's guarantees at 44.1/48/96/192 kHz, both qualities, three ceilings: output never exceeds the ceiling (plus a 0.15% interpolation allowance), bit-exact delayed passthrough below the ceiling, a ramped attack, and NaN/Inf/mono robustness. The previous cross-check against a copy of the old limiter was retired because the old algorithm was the bug.

## CPU measurements

Benchmark is an informational wall-clock time measurement of 4 s of stereo 48-kHz audio, not a CI pass/fail threshold. Hosted runner timings are **not** proof of performance on old PCs.

    cmake -S SkellCore -B build-core -DCMAKE_BUILD_TYPE=Release -DSKELLCORE_BUILD_TESTS=ON -DSKELLCORE_BUILD_BENCHMARKS=ON
    cmake --build build-core --config Release --parallel 2
    ctest --test-dir build-core --build-config Release --output-on-failure
    ctest --test-dir build-core --build-config Release -R SkellCoreBenchmark -V

Recommended real target: 5, 10 and 20 simultaneous instances in FL Studio on low-end Windows hardware, with 64/128/256 sample buffers at 44.1/48 kHz. Profile worst-case loud vocal material, not silence, and measure overruns/dropouts, CPU per callback, RAM and plugin latency. Keep all production DSP modules on their existing defaults until A/B and CPU verification.

## Still outside this release

SkellyTune's imported realtime pitch engine, DeEsser phrase-aware backend and MasteringEngine are not altered by these changes. Their multi-instance CPU use, latency stability and sound quality require separate host-level profiling and golden WAV comparisons. No real FL Studio host validation has been performed in this commit.
## v0.6.1 limiter gain stage
The previous limiter held its gain for a fixed time after a gain drop and then released toward the *current* input's need. If a quieter peak entered the lookahead while a louder one was still in it, the release could start before the quieter peak left the delay line, and the output exceeded the ceiling (measured up to +1.4 dB over a -1 dBFS ceiling in extreme material). Gain reduction also dropped in a single-sample step, which can click.

Now: the required gain is minimised over every sample still in the delay line (monotonic deque, O(1) amortised, preallocated in `prepare()`), smoothed by a boxcar of `latency - (taps/2 + 1)` samples so attack is a ~3 ms ramp that finishes as the peak leaves, then released by the same 70 ms one-pole as before. Latency is unchanged (`max(48, 3 ms)`). Below the ceiling the output is the input delayed by exactly `latencySamples()`. Measured cost on the 48 kHz stereo benchmark: about +35 ns/frame (110 to 146 ns, roughly +0.17% of one core).

## v0.7.0 higher-order FDN (`<SkellCore/FDN.h>`)
`BandFDN<N>` (N = 4, 8 or 16; aliases `FDNReverb8`, `FDNReverb16`) is a new class; the existing `FDNReverb` in `Core.h` is unchanged and bit-identical. It uses a Householder matrix (O(N) per sample), prime delay lengths spread geometrically over `prepare(rate, minDelayMs, maxDelayMs)` (default 24-80 ms), and a per-line one-pole absorption filter so low-band RT60 (`decayLow`) and RT60 at a treble reference (`decayHigh`, 4 kHz by default, `setTrebleReferenceHz`) are set independently. `decayHigh` is clamped to `<= decayLow`. Input and output use orthogonal +/-1 patterns, so left/right tails decorrelate (measured |correlation| < 0.13). Input drive is calibrated so the wet level at equal RT60 matches `FDNReverb` to within about 0.5 dB.

Measured (tests/test_fdn.cpp, 48 kHz): low-band RT60 within 1% of the setting; RT60 around 4 kHz about 15% above the setting (a first-order absorption filter has a gentle slope, so a band-pass measurement around the reference also sees slower-decaying frequencies below it). Normalised echo density at 150 ms: 4-line 0.06, 8-line 0.16, 16-line 0.23 (Gaussian noise = 0.317).

CPU on a noisy shared container, one stereo sample, 48 kHz: existing `FDNReverb` about 30 ns/frame; `FDNReverb8` about 35-60 ns; `FDNReverb16` about 77-140 ns (roughly 0.4-0.7% of one core). Measure on target hardware before choosing N. Not production-certified: no modulation, no input diffusion, no per-band filters beyond the two-band absorption.

# SkellCore Live Engine v1 — low-latency tracking foundation

**Goal:** make low-latency tracking a *per-plugin implementation requirement* rather than an unsupported promise that any effect can be made "zero latency" by routing through a rack.

## What this module adds

Portable `#include <SkellCore/LiveEngine.h>`:

- `live::LatencyContract` for exact sample-rate-aware *intrinsic* plugin latency, millisecond reporting and explicit sample budgets. Host latency reporting is done by the owning plugin via JUCE `setLatencySamples()`; never call that function from `processBlock()`.
- `live::Profile` (`Live`, `Studio`), representing a deliberate quality/performance tradeoff, not just a bypass.
- `live::LiveMultibandDucker` for zero-additional-delay sidechain spectral *approximation* using ten complementary causal IIR bands, per-band ducking and linked stereo controls. `latencySamples() == 0` is an algorithmic sample delay guarantee in the zero-lookahead implementation; **it is not a 0ms hardware round-trip latency guarantee**.
- A portable DSP test using impulses, 1/17/64/512/1024-sample blocks, 44.1–192kHz processing, sidechain test tones, bypass, disabled modes, parameter churn and nonfinite inputs.

The `LiveMultibandDucker` is a separate tracking-friendly **sound**, NOT bitwise/audio-equivalent to the studio FFT spectral ducker. Its causal IIR filters change frequency/phase behaviour and finite bands cannot give the original 2,048-bin frequency selectivity. Do not replace Studio mode silently. It uses only stack/fixed-array state and avoids heap allocations, blocking, locks, I/O, FFT frame buffering, and host notifications in the audio callback.

## Apply to a plugin

1. **Audit** its full audio path: input resampling/FFT, detector analysis, lookahead, convolution, oversampling, FIR filtering, pad delays, wet/dry and bypass. Record actual output delay via a test impulse.
2. **Design two honest paths**. Studio retains the existing algorithm, existing plugin IDs/automation and the reported sample delay. Live is an independently testable lower-latency algorithm, using `LiveMultibandDucker` only for effects whose processing maps to multiband ducking.
3. **Set host PDC to the active intrinsic path** on initialization or while explicitly changing mode from a safe non-audio context. Do not claim PDC reduction until the host sees the new contract and the actual waveform is verified. A low-latency bypass must still preserve timing when the active path has latency.
4. **Treat mode transitions as lifecycle changes**, not immediate automation: if a host cannot safely reconfigure plugin delay while playing, defer until the plugin is re-prepared or reloaded, or implement and validate a separate seamless PDC switch with overlap buffers.
5. **Test real hosts and hardware**: host buffer + interface round-trip + plugin PDC across the entire monitor chain determine the performer's audible delay. Prefer an audio interface's ASIO native driver and 64/128 sample buffers when stable. Avoid unverified promises of 1 ms full round-trip monitoring.
6. **Publish real numbers** per sample rate and profile, and show "Live" vs "Studio" clearly. Mastering/linear-phase/long lookahead may deliberately retain higher latency.
7. **Performance**: compare RMS and cut depth, stereo, phase, silence, NaN/Inf, denormals, CPU and worst-case callback time across block sizes. Realtime code must be bounded and allocation-free once prepared.

### Example

```cpp
#include <SkellCore/LiveEngine.h>
skellcore::live::LiveMultibandDucker live;
live.prepare(sampleRate); // NON-audio-thread, e.g. prepareToPlay()
skellcore::live::LiveDuckerParams params;
params.amount = 0.5f;
params.attackMs = 10.f;
// inside audio callback:
live.setParameters(params);
live.process(mainChannelPointers, sidechainPointers, mainChannels, sideChannels, sampleCount);
// On the owning JUCE processor: report live.latencySamples() while live is active.
// Report the studio engine's actual latency while the studio path is active.
```

### MetaTune and pitch synthesis

MetaTune's current Skell Live path reports ~3.4 ms but cannot fully correct pitch below ~155 Hz. This is intrinsic pitch-period evidence, not a generic buffer we can remove. SkellCore's new shared multiband ducker does **not** replace its tuner or change its PDC. Full low-register high-quality tuning needs its own synthesis redesign and vocal benchmarks.

### Spacer

Spacer's current FFT path reports 2,048 samples at 48 kHz (42.667 ms), not exactly the same as a DAW's displayed end-to-end latency (e.g. 70 ms with device buffers). The new live ducker is a candidate separate mode; it should be exposed only with clear mode/host PDC semantics, benchmarked against real sidechain audio and rolled out with an opt-in parameter. Existing project states should retain the Studio processing mode by default.

## Additional shared building block: latency-aligned bypass

For any plugin whose *wet* path necessarily incurs intrinsic latency, use [`LatencyAlignedMix`](LATENCY_ALIGNED_MIX.md) to align dry/wet and bypass so switching does not create phase combing or an uncompensated timing jump. This utility does **not** remove wet algorithm latency and should be used in addition to (not in place of) an explicit host PDC contract.

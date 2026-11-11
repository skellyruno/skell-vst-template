# SkellCore Latency-Aligned Mix / Bypass

## Why this exists

For a latent DSP algorithm (FFT spectral effects, lookahead dynamics, pitch correction, convolution), mixing the raw input with a delayed wet path creates comb filtering, clicks, or misaligned bypass. A normal JUCE dry/wet mixer does not automatically know the wet algorithm's intrinsic delay.

`SkellCore/include/SkellCore/LatencyAlignedMix.h` is a **portable, C++17, real-time-safe** utility that delays the dry path by the wet engine's true sample latency. It then performs sample-accurate, click-smoothed dry/wet blending and bypass, preserving the original host-reported PDC.

### Features

- Mono through eight-channel processing, 44.1–192 kHz tested; supports host buffers of any positive length, including single-sample blocks.
- True zero-added latency when `delaySamples = 0`. For a delayed processor, it aligns dry to exactly the already-present **wet latency**: it does **not** create extra independent latency beyond that wet delay.
- Smooth wet/dry and bypass transitions using a fixed-duration linear ramp, independent of host buffer size.
- Both dry history and crossfade state stay active while fully wet/bypassed, so switching does not cold-start the delay.
- Accepts wet/output aliasing for in-place wet output; the dry pointer must reference the original unprocessed samples.
- All working memory allocated in `prepare()`, with bounded maximum latency (192,000 samples) and channel count (8). `process()` does no allocation, resize, mutex, logging or host notifications.
- Sanitizes NaN/Inf samples and invalid automation wet targets, rejects unsupported layouts before touching output.
- Supports immediate initial bypass/preset restore without a startup fade.

## Correct JUCE integration

Use `#include <SkellCore/LatencyAlignedMix.h>` in an existing plugin, or add it to a future plugin through `SkellCore::DSP`. The real processor owns its separate wet DSP.

```cpp
skellcore::live::LatencyAlignedMix dryWet;

// In prepareToPlay (no audio concurrently):
const int actualWetDelaySamples = effectEngine.latencySamples();
const bool ok = dryWet.prepare(sampleRate, actualWetDelaySamples, 2, 5.0);
if (!ok) { /* fail safely: do not run mismatched delayed DSP */ }
dryWet.setMixImmediately(startBypassed ? 0.0f : 1.0f);
setLatencySamples(actualWetDelaySamples); // non-audio thread

// In processBlock:
// 1. Keep the ORIGINAL input in preallocated scratch (allocated in prepare).
// 2. Run wet DSP into wet/out buffer.
// 3. Read host parameter atomics on the AUDIO thread:
//    dryWet.setBypassed(bypassParam->load() > 0.5f);
// 4. Call dryWet.process(dryPointers, wetPointers, outPointers, channels, frames).
//    wetPointers may equal outPointers; dryPointers must remain untouched.
// 5. Validate the bool return and handle a rejected layout safely.
```

**Important:** If the DSP engine itself changes between Live/Studio algorithms, first reconfigure the wet engine, dry alignment and host PDC together from a safe non-audio lifecycle. This utility **does not** change host PDC and cannot make a fundamentally lookahead-based processor zero-latency.

For full plugin-bus support, preserve dry scratch for the entire main bus only (never sidechain data), choose a capacity matching the host's maximum block size, and split unexpectedly large blocks safely. An 8-channel plugin should not allocate buffers inside `processBlock()`.

Do not call `setMixImmediately`, `prepare`, `reset`, or `setFadeMs` during active concurrent processing; `setTargetWet` and `setBypassed` are meant to be driven on the audio thread. To update UI state, read APVTS atomics on the audio thread rather than concurrently writing the mixer's members from the message thread.

## Tests

`SkellCore/tests/test_latency_aligned_mix.cpp` covers:
- Impulse offset at zero, one sample, 17, 128 and 2,048 samples; 44.1/48/96/192 kHz.
- Mathematical dry/wet alignment and in-place wet/output aliasing.
- Exact results across 1, 7, 64, 128, 257, 511 and 8,192-sample block boundaries, including automated bypass/mix changes.
- Five-millisecond click-smoothing with monotonic bounded steps.
- Invalid layout/rate/latency refusal, NaN/Inf sanitation, resets and eight-channel operation.

Run:
```bash
cmake -S SkellCore -B build-core -DSKELLCORE_BUILD_TESTS=ON
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

This test suite verifies internal DSP delay alignment. Actual end-to-end microphone-to-headphone latency includes host/interface buffers, and must be measured in a host such as FL Studio. If a plugin uses a variable or nonlinear inherent time-domain delay, fixed-sample alignment may not match it; instrument and validate the actual wet path first.

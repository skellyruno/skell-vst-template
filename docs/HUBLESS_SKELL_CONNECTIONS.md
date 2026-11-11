# SkellCore hub-free plugin connections

## Customer experience (Windows)

Install the regular VST3 bundles. SkellKey publishes Key, Scale and A4
only when SEND TO TUNE is clicked. Mini, main SkellyTune, OmniChoir and
AdlibLab can receive this without a Hub, Python, external app or internet.

## Transport contract

- SkellCore/include/SkellCore/LocalTuningBus.h uses Windows Local\
  shared memory and a single interlocked 64-bit complete tuning packet.
- Source: WorldLink product skell.key joins on UI thread, and calls
  publishMusicalTuning only after an explicit Send action.
- Receiver: call WorldLink::join at initialization, then poll
  latestMusicalTuning on a JUCE timer at 4–5 Hz. Never from processBlock.
- The packet includes root (0–11), mode (major/minor), absolute 12-note
  mask and A4 millihertz, with a monotonic 100-ms timestamp.
- Normal sender close withdraws only its last packet; crashed senders
  expire after five seconds. When more than one sender is active, the
  most recent publisher wins. Current DAW parameter values persist.
- Keep existing APVTS parameter IDs, preserve custom note masks, and
  avoid changes to audio callbacks or a plugin's reported latency.
- It is same-Windows-session informational IPC, not a cryptographic
  security boundary against programs running as the same Windows user.
- The optional SW1 Hub remains available for non-Windows platforms and
  non-tuning events (themes, presets, transport, meters). These other
  events are NOT hub-free yet; do not claim otherwise.

## Recommended receiver code

Create skellcore::world::WorldLink link{"skell.myplugin", "My plugin"};
From JUCE initialization/message thread: link.join(); startTimerHz(5);
In timerCallback use skellcore::world::MusicalTuning and
link.latestMusicalTuning(tuning, version). Validate and change the
plugin's existing Key/Scale/A4 APVTS fields on the message thread.
During teardown: stopTimer(); link.leave();

## Test evidence

SkellCoreLocalTuningTests covers timestamp wraparound, invalid packets,
shutdown isolation, and an actual Windows CreateProcessW cross-process
roundtrip. SkellyTune Mini Windows UI smoke runs a real SkellKey sender
and Mini receiver with no Hub process, proving key, scale and A4 arrive.

Pin an immutable, tested SkellCore revision in dependent plugin CMake.
Initial macro-safe revision: 4c499c58cb3b2791eed2d5dcef6d8d543fcaea8d.

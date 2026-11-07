# SkellCore source provenance (2026-11-07)

Original production DSP preserved in SkellCore. References are immutable upstream blob SHAs in the migration records.

- SkellyTune real-time pitch and related DSP: `skellyruno/skelltune-mini`, `dsp/*.hpp` at commit around v3.3. License: MIT (copied in `licenses/SkellyTune-MIT.txt`). The backend is *not* a new pitch algorithm; it adapts this implementation.
- Phrase-aware de-esser: `skellyruno/skell-deesser-vst`, `Source/dsp/PhraseAwareDetector.h`. The file is copied with only the enclosing namespace changed from global to `skellcore::deesser` to avoid collisions.
- Mastering engine: `skellyruno/skell-master-vst`, `Source/DSP/MasteringEngine.h`. Copied with an enclosing namespace change from `skelly` to `skellcore::master`, avoiding symbol conflicts with SkellyTune.

When mixing dependencies from other authors or plugins, verify their redistribution and commercial-use terms *before import*. Do not assume all GitHub repos are permissively licensed.

Known quality limits:
- The prototype `TruePeakLimiter` is **not** certified true-peak or BS.1770 compliant.
- The production mastering DSP expects a separate oversampler.
- The production pitch source may have variable source age for voiced synthesis; the delay contract describes the fixed dry/transient host-compensated path, not time-varying source grain age.
- The header-only `SkellyTuneBackend` is currently realtime profile; no experimental cycle-synthesis mode is enabled by the adapter.

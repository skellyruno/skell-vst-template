#!/usr/bin/env python3
"""Generate a deterministic FL Studio SkellCore host-acceptance test kit.

The output contains only original reference audio, a host measurement CSV and
instructions. No FLP is fabricated; FL Studio must save/reopen the actual test
project. Standard library only; no DAW paths are read or modified.
"""
import argparse
import csv
import math
from pathlib import Path
import struct
import wave

SAMPLE_RATE = 48000
DURATION_SECONDS = 12
FRAMES = SAMPLE_RATE * DURATION_SECONDS
CHANNELS = 2

def sample_at(index: int) -> tuple[int, int]:
    time = index / SAMPLE_RATE
    # Different tones in L/R expose channel swaps or mono routing mistakes.
    # Two silence windows let testers see residual noise and frozen meter lines.
    silent = 3 <= time < 4 or 7 <= time < 8
    if silent:
        left = right = 0.0
    else:
        left = .100 * math.sin(2 * math.pi * 220.0 * time)
        right = .085 * math.sin(2 * math.pi * 330.0 * time)
        for pulse_time in (2.0, 6.0, 10.0):
            dt = time - pulse_time
            if 0 <= dt < .004:
                pulse = .06 * (1 - dt / .004)
                left += pulse
                right -= pulse * .5
    # No clipping possible even after Quickstart maximum 2.0x gain.
    left = max(-1.0, min(1.0, left))
    right = max(-1.0, min(1.0, right))
    return (int(round(left * 32767)), int(round(right * 32767)))

def write_wav(path: Path) -> None:
    with wave.open(str(path), "wb") as destination:
        destination.setnchannels(CHANNELS)
        destination.setsampwidth(2)
        destination.setframerate(SAMPLE_RATE)
        # Stream in chunks to avoid large temporary memory allocations.
        for begin in range(0, FRAMES, 4096):
            end = min(begin + 4096, FRAMES)
            audio = bytearray((end - begin) * CHANNELS * 2)
            for index in range(begin, end):
                left, right = sample_at(index)
                struct.pack_into("<hh", audio, (index - begin) * 4, left, right)
            destination.writeframesraw(audio)

def write_matrix(path: Path) -> None:
    with path.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.writer(destination)
        writer.writerow([
            "sample_rate_hz", "host_buffer_samples", "instances",
            "baseline_cpu_percent", "enabled_cpu_percent", "cpu_peak_percent",
            "dropouts", "project_reopen_pass", "automation_pass", "notes"
        ])
        for rate in (44100, 48000, 96000):
            for block in (64, 128, 512):
                for count in (1, 4, 8):
                    writer.writerow([rate, block, count, "", "", "", "", "", "", ""])
            for block in (128, 512):
                writer.writerow([rate, block, 16, "", "", "", "", "", "", ""])

README = """# SkellCore Quickstart – FL Studio acceptance kit

This package is a **manual FL Studio test aid**, not evidence that the DAW
has already passed. The reference.wav is original synthetic stereo audio,
48 kHz / 16-bit / 12 seconds. L uses 220 Hz and R uses 330 Hz; short
transients occur at 2, 6 and 10 seconds; 3–4 and 7–8 seconds are silent.

1. Use the exact Skell Quickstart VST3 build identified by the CI commit
   and SHA256. Install it using your normal Windows VST3 procedure.
2. Start FL Studio with an empty project; add reference.wav to the playlist,
   route to a clean mixer insert with **no other effects or gain changes**.
   Turn off any channel or clip normalization.
3. Measure the dry reference at gain 1.0. With Quickstart Enabled=on, set
   Gain=1.625; after 15 ms smoothing, steady signal level should be
   approximately 1.625x (about +4.22 dB). The silence windows stay silent.
4. Toggle Enabled=off. Signal should return smoothly to unity, not mute.
   Turn on and automate Gain 1.0 -> 0.2 -> 1.8 while the clip plays.
5. Save an FLP while Gain=1.625 / Enabled=off, close FL Studio, reopen it,
   and verify state. Reopen/close the editor repeatedly, resize min/default/
   max, and make sure SkellWorld stays offline until explicitly joined.
6. For each row of results.csv, record **FL Studio's measured CPU**
   baseline, enabled average/peak CPU, dropouts, automation and state
   recovery. If your sound card does not support a combination, mark N/A.
7. Compare 1, 4, 8 and 16 Quickstart instances. At 16 instances use
   128/512-sample buffers as the initial stress matrix.

Do not confuse CI CPU ratios with DAW audio thread load or ASIO input/output
latency. Do not change SkellCore's shared skull/control defaults while testing.
Full expected behavior: docs/FL_STUDIO_HOST_ACCEPTANCE.md in the source repo.
"""

def build(output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    wav_file = output_dir / "reference.wav"
    write_wav(wav_file)
    write_matrix(output_dir / "results.csv")
    (output_dir / "README.md").write_text(README, encoding="utf-8")
    with wave.open(str(wav_file), "rb") as readback:
        assert (readback.getnchannels(), readback.getsampwidth(),
                readback.getframerate(), readback.getnframes()) == (
                    CHANNELS, 2, SAMPLE_RATE, FRAMES)
    print(f"Created 12-second FL Studio reference and test matrix: {output_dir}")

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("host-test-kit"))
    args = parser.parse_args()
    build(args.out)

if __name__ == "__main__":
    main()

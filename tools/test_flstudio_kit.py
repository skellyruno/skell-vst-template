#!/usr/bin/env python3
"""Regression-check the deterministic FL Studio acceptance kit generator."""
import csv
import pathlib
import subprocess
import sys
import tempfile
import unittest
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]

class HostTestKitTests(unittest.TestCase):
    def test_generated_wav_and_matrix(self):
        with tempfile.TemporaryDirectory(prefix="skell-host-kit-") as tmp:
            folder = pathlib.Path(tmp) / "kit"
            subprocess.run([sys.executable,
                            str(ROOT / "tools" / "Make-FLStudio-TestKit.py"),
                            "--out", str(folder)], check=True)
            with wave.open(str(folder / "reference.wav"), "rb") as wav:
                self.assertEqual(wav.getnchannels(), 2)
                self.assertEqual(wav.getsampwidth(), 2)
                self.assertEqual(wav.getframerate(), 48000)
                self.assertEqual(wav.getnframes(), 576000)
                # Stereo is intentionally non-identical in the audible regions.
                stereo = wav.readframes(1000)
                self.assertNotEqual(stereo[2:4], stereo[0:2])
                # Silence at 3s and 7s must be actual digital zero.
                for second in (3.5, 7.5):
                    wav.setpos(int(second * 48000))
                    self.assertEqual(wav.readframes(500), bytes(500 * 4))

            self.assertTrue((folder / "README.md").exists())
            with (folder / "results.csv").open(
                newline="", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                rows = list(reader)
            self.assertEqual(len(rows), 33)
            self.assertEqual({int(row["sample_rate_hz"]) for row in rows},
                             {44100, 48000, 96000})
            self.assertEqual({int(row["instances"]) for row in rows},
                             {1, 4, 8, 16})
            self.assertTrue(all(row["baseline_cpu_percent"] == "" for row in rows))
            self.assertEqual(sum(row["instances"] == "16" for row in rows), 6)

if __name__ == "__main__":
    unittest.main()

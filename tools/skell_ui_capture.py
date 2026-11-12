#!/usr/bin/env python3
"""SKELL-wide local real-JUCE screenshot runner (default four screenshots).
This executes a compiled per-plugin renderer; it cannot render Windows VST3
modules or rebuild C++ sources. No GitHub Actions or network needed.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("ui-screenshots-local"))
    parser.add_argument("--full", action="store_true", help="Full regression scenes, not four quick images")
    parser.add_argument("--expected", type=int, help="Expected scene count; defaults to four in quick mode")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    exe, folder = args.renderer.resolve(), args.output.resolve()
    if not exe.is_file():
        parser.error("Compiled Linux UI renderer executable not found")
    xvfb = shutil.which("xvfb-run")
    if not xvfb:
        parser.error("Install xvfb and xauth for local Linux rendering")
    folder.mkdir(parents=True, exist_ok=True)
    for old in list(folder.glob("*.png")) + [folder / "manifest.json"]:
        if old.exists():
            old.unlink()
    cmd = [xvfb, "-a", "-s", "-screen 0 1920x1200x24",
           str(exe), "--output", str(folder)]
    if args.full:
        cmd.append("--full")
    started = time.perf_counter()
    try:
        result = subprocess.run(cmd, capture_output=True, text=True,
                                timeout=args.timeout)
        elapsed = round(time.perf_counter() - started, 3)
        if result.returncode:
            raise RuntimeError("JUCE renderer failed: " + result.stderr[-2000:])
        manifest = json.loads((folder / "manifest.json").read_text())
        rows = manifest.get("scenes", [])
        expected = args.expected if args.expected is not None else (None if args.full else 4)
        if not rows or manifest.get("count") != len(rows):
            raise RuntimeError("Invalid/empty JUCE screenshot manifest")
        if expected is not None and len(rows) != expected:
            raise RuntimeError(f"Expected {expected} scenes, got {len(rows)}")
        expected_images = {"contact-sheet.png"}
        for scene in rows:
            png = scene["file"]
            expected_images.add(png)
            with Image.open(folder / png) as img:
                if not img.getbbox() or img.size != (scene["width"], scene["height"]):
                    raise RuntimeError("Blank or wrong-size screenshot: " + png)
            if scene.get("sampledColours", 0) < 12:
                raise RuntimeError("Low-contrast screenshot: " + png)
        if {p.name for p in folder.glob("*.png")} != expected_images:
            raise RuntimeError("Missing or stale PNGs")
        (folder / "local-timing.json").write_text(json.dumps({
            "real_juce_editor": True,
            "scenes": len(rows),
            "elapsed_seconds": elapsed,
            "mode": "full" if args.full else "quick",
        }, indent=2) + "\n")
        print(f"PASS: {len(rows)} REAL JUCE screenshots in {elapsed}s -> {folder}")
        return 0
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as e:
        print("FAIL:", e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Offline, plugin-agnostic JUCE source compile + four real UI screenshots.

Requires a pre-populated JUCE/SkellCore checkout and Linux development libs.
Windows VST3 and DSP release validation are deliberately separate.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--project", type=Path, required=True)
    p.add_argument("--juce", type=Path, required=True)
    p.add_argument("--skellcore", type=Path, required=True)
    p.add_argument("--renderer-target", default="SkellyMasterUIRenderer")
    p.add_argument("--renderer-option", default="SKELLY_MASTER_BUILD_UI_RENDERER")
    p.add_argument("--build", type=Path)
    p.add_argument("--output", type=Path, default=Path("ui-screenshots-local"))
    p.add_argument("--jobs", type=int, default=3)
    p.add_argument("--full", action="store_true")
    p.add_argument("--check-only", action="store_true")
    a = p.parse_args()
    project, juce, core = (x.resolve() for x in (a.project, a.juce, a.skellcore))
    build = a.build.resolve() if a.build else project / ".build-ui-local"
    output = a.output.resolve()
    requirements = [("CMake", shutil.which("cmake")), ("Ninja", shutil.which("ninja")),
                    ("C++", shutil.which("c++")), ("Xvfb", shutil.which("xvfb-run")),
                    ("plugin source", (project/"CMakeLists.txt").is_file()),
                    ("JUCE source", (juce/"CMakeLists.txt").is_file()),
                    ("SkellCore source", (core/"SkellCore"/"CMakeLists.txt").is_file())]
    missing = [name for name, found in requirements if not found]
    if missing:
        print("BLOCKED: Missing " + ", ".join(missing), file=sys.stderr)
        return 2
    if a.check_only:
        print("PASS: basic local compile prerequisites")
        return 0
    started = time.perf_counter()
    config = ["cmake","-S",str(project),"-B",str(build),"-G","Ninja",
              "-DCMAKE_BUILD_TYPE=Release",f"-DJUCE_PATH={juce}",
              f"-DFETCHCONTENT_SOURCE_DIR_SKELLCORESHARED={core}",
              f"-D{a.renderer_option}=ON"]
    try:
        subprocess.run(config, check=True)
        subprocess.run(["cmake","--build",str(build),"--target",a.renderer_target,
                        "--parallel",str(max(1,a.jobs))],check=True)
        executables = [x for x in build.rglob(a.renderer_target)
                       if x.is_file() and os.access(x,os.X_OK)]
        if len(executables)!=1:
            raise RuntimeError(f"Expected one rebuilt renderer, found {len(executables)}")
        output.mkdir(parents=True, exist_ok=True)
        for prior in [*output.glob("*.png"),output/"manifest.json"]:
            if prior.exists(): prior.unlink()
        cmd = ["xvfb-run","-a","-s","-screen 0 1920x1200x24",
               str(executables[0]),"--output",str(output)]
        if a.full: cmd.append("--full")
        subprocess.run(cmd,check=True)
        manifest = json.loads((output/"manifest.json").read_text())
        count = manifest["count"]
        if count!=len(manifest["scenes"]) or (not a.full and count!=4):
            raise RuntimeError(f"Expected four quick scenes, received {count}")
        pngs = {x["file"] for x in manifest["scenes"]}|{"contact-sheet.png"}
        if pngs != {x.name for x in output.glob("*.png")}:
            raise RuntimeError("Missing/stale screenshot; refusing to report pass")
        result = {"passed":True,"scenes":count,
                  "elapsed_seconds":round(time.perf_counter()-started,3),
                  "renderer":str(executables[0]),"source_project":str(project)}
        (output/"local-build-report.json").write_text(json.dumps(result,indent=2)+"\n")
        print(f"PASS: locally compiled real JUCE UI and captured {count} scenes")
        return 0
    except (subprocess.CalledProcessError,OSError,ValueError,RuntimeError) as exc:
        print("FAILED:",exc,file=sys.stderr)
        return 1

if __name__=="__main__":
    sys.exit(main())

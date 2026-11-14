#!/usr/bin/env python3
"""Skelly local UI router. Builds REAL plugin editors; never fabricates previews.

No GitHub Actions are used for screenshots. For a fresh source change a locally
compiled JUCE renderer is required. Windows VST3/DSP validation is independent.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

# Each entry is a real, opt-in CMake target in that repository.
# "wired" means code exists; it does NOT imply the local C++ build was tested.
PROJECTS = {
    "skell-master-vst": {
        "target": "SkellyMasterUIRenderer",
        "option": "SKELLY_MASTER_BUILD_UI_RENDERER",
        "core_dependency": "SKELLCORESHARED",
        "status": "locally-verified",
        "extra": [],
    },
    "skelltune-mini": {
        "target": "SkellyTuneQuick4",
        "option": "SKELLY_BUILD_UI_QUICK4",
        "core_dependency": "SKELLCORESHARED",
        "status": "locally-verified",
        "extra": ["SKELLY_BUILD_PLUGIN=ON", "SKELLY_BUILD_DSP_TESTS=OFF"],
    },
    "skell-silence-vst": {
        "target": "SkellSilenceUIRenderer",
        "option": "SKELL_SILENCE_BUILD_UI_RENDERER",
        "core_dependency": "SKELLCOREPINNED",
        "status": "wired-needs-native-check",
        "extra": [],
    },
    "skell-deesser-vst": {
        "target": "SkellyDeEsserUIRenderer",
        "option": "SKELLY_DEESSER_BUILD_UI_RENDERER",
        "core_dependency": "SKELLCORE_PATH",
        "status": "wired-needs-native-check",
        "extra": [],
    },
    "skell-preset-vst": {
        "target": "SkellPresetUIRenderer",
        "option": "SKELL_PRESET_BUILD_UI_RENDERER",
        "core_dependency": "",
        "status": "wired-needs-native-check",
        "extra": [
            "SKELL_PRESET_PACKAGE_TESTS=OFF",
            # Use local shared SkellCore headers instead of downloading a
            # second copy through the Guided Tour FetchContent dependency.
        ],
        "quick_args": ["--quick4"],
    },
    "skell-fan-vst": {
        "target": "SkellFanQuick4",
        "option": "SKELLFAN_BUILD_UI_QUICK4",
        "core_dependency": "SKELLCORE",
        "status": "wired-needs-native-check",
        "extra": ["SKELLFAN_BUILD_TESTS=OFF"],
    },
    "skell-flang-vst": {
        "target": "SkellFlangUiSmoke",
        "option": "SKELLFLANG_BUILD_UI_SMOKE",
        "core_dependency": "",
        "status": "locally-verified",
        "extra": [],
    },
}

def absolute(path: Path) -> Path:
    return path.expanduser().resolve()

def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--list",action="store_true",help="List registered UI renderer targets")
    p.add_argument("--plugin",choices=sorted(PROJECTS))
    p.add_argument("--project",type=Path,help="Local plugin source checkout")
    p.add_argument("--juce",type=Path,help="Local JUCE 8.0.12 source checkout")
    p.add_argument("--skellcore",type=Path,help="Local pinned SkellCore source checkout")
    p.add_argument("--output",type=Path,default=Path("quick4-local"))
    p.add_argument("--build",type=Path,help="Separate native build directory")
    p.add_argument("--jobs",type=int,default=1)
    p.add_argument("--full",action="store_true",help="Explicit deep audit, not Quick4")
    p.add_argument("--define",action="append",default=[],
                   help="Additional CMake NAME=VALUE (repeatable)")
    args=p.parse_args()
    if args.list:
        for key,item in sorted(PROJECTS.items()):
            print(f"{key}: {item['target']} [{item['status']}]")
        return 0
    if not args.plugin or not args.project or not args.juce:
        p.error("--plugin, --project and --juce are required unless --list")
    cfg=PROJECTS[args.plugin]
    project,juce,output=map(absolute,[args.project,args.juce,args.output])
    build=absolute(args.build) if args.build else project/".skell-ui-local"
    core=absolute(args.skellcore) if args.skellcore else None
    for label,path in [("plugin",project/"CMakeLists.txt"),
                       ("JUCE",juce/"CMakeLists.txt")]:
        if not path.is_file():
            p.error(f"Missing {label} source: {path}")
    coredep=cfg["core_dependency"]
    if coredep and (core is None or not (core/"SkellCore"/"CMakeLists.txt").is_file()):
        p.error("This plugin needs --skellcore pointing to a local repo with SkellCore/CMakeLists.txt")
    for binary in (("cmake", "ninja", "xvfb-run") if os.name != "nt"
                   else ("cmake", "ninja")):
        if not shutil.which(binary):
            p.error(f"{binary} is required for a local real-JUCE screenshot")
    if not shutil.which("c++") and os.name != "nt":
        p.error("A local C++ compiler is required (no screenshots from stale binaries)")
    extra=cfg["extra"]+args.define
    for value in extra:
        if "=" not in value or not value.split("=",1)[0].replace("_","").isalnum():
            p.error("Invalid --define: "+value)
    cmake=["cmake","-S",str(project),"-B",str(build),"-G","Ninja",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DCMAKE_CXX_FLAGS_RELEASE=-O0 -DNDEBUG",
           "-DCMAKE_C_FLAGS_RELEASE=-O0 -DNDEBUG",
           f"-DJUCE_PATH={juce}",f"-D{cfg['option']}=ON"]
    if coredep=="SKELLCORE_PATH":
        cmake.append(f"-DSKELLCORE_PATH={core/'SkellCore'}")
    elif coredep:
        cmake.append(f"-DFETCHCONTENT_SOURCE_DIR_{coredep}={core}")
    # Preset bundles SkellCore's DSP but separately FetchContents guided-tour
    # headers. Reuse the already checked-out Core source, never refetch it.
    if args.plugin=="skell-preset-vst" and core:
        cmake.append(f"-DFETCHCONTENT_SOURCE_DIR_SKELLGUIDEDTOURHEADERS={core}")
    cmake += ["-D"+value for value in extra]
    started=time.perf_counter()
    try:
        subprocess.run(cmake,check=True)
        subprocess.run(["cmake","--build",str(build),"--target",cfg["target"],
                        "--parallel",str(max(1,args.jobs))],check=True)
        runners=[x for x in build.rglob(cfg["target"] + (".exe" if os.name=="nt" else ""))
                 if x.is_file() and (os.name=="nt" or os.access(x,os.X_OK))]
        if len(runners)!=1:
            raise RuntimeError(f"Expected one genuine UI executable; found {len(runners)}")
        output.mkdir(parents=True,exist_ok=True)
        for old in list(output.glob("*.png")) + [output/"manifest.json"]:
            if old.exists(): old.unlink()
        cmd=[str(runners[0]),"--output",str(output)]
        if args.full:
            cmd.append("--full")
        else:
            cmd.extend(cfg.get("quick_args",[]))
        if os.name!="nt":
            cmd=["xvfb-run","-a","-s","-screen 0 1920x1200x24",*cmd]
        subprocess.run(cmd,check=True)
        manifest=json.loads((output/"manifest.json").read_text())
        entries=manifest.get("scenes",[])
        if len(entries)!=(manifest.get("count")):
            raise RuntimeError("Invalid scene count in actual renderer manifest")
        if not args.full and len(entries)!=4:
            raise RuntimeError("Quick4 expected precisely four real GUI screenshots")
        expected={"contact-sheet.png"}|{entry["file"] for entry in entries}
        found={x.name for x in output.glob("*.png")}
        if expected!=found:
            raise RuntimeError("Missing or stale UI screenshots: "+repr(expected^found))
        for item in entries:
            frame=output/item["file"]
            if frame.stat().st_size<1024 or item["width"]<64 or item["height"]<64:
                raise RuntimeError("Blank/invalid screenshot: "+item["file"])
        source_hash=hashlib.sha256((project/"CMakeLists.txt").read_bytes()).hexdigest()
        def git_info(*args):
            try:
                return subprocess.run(["git","-C",str(project),*args],
                                      check=True,capture_output=True,text=True).stdout.strip()
            except (OSError,subprocess.CalledProcessError):
                return None
        source_commit=git_info("rev-parse","HEAD")
        dirty=bool(git_info("status","--porcelain")) if source_commit else None
        if source_commit is None:
            raise RuntimeError("Cannot prove the source revision (git unavailable or project not a checkout)")
        report={
            "passed":True, "plugin":args.plugin,"source":str(project),
            "renderer":str(runners[0]),"quick4":not args.full,
            "screenshots":len(entries),"elapsed_seconds":round(time.perf_counter()-started,3),
            "cmake_sha256":source_hash,
            "source_commit":source_commit,
            "source_dirty":dirty,
            "renderer_status":cfg["status"],
            "verification":"locally-rebuilt-real-JUCE" if not dirty else "dirty-source-local-build",
            "caveat":"Not Windows VST3 or FL Studio validation",
        }
        (output/"local-build-report.json").write_text(json.dumps(report,indent=2)+"\n")
        # Self-contained handoff across ChatGPT environments: verified UI
        # PNGs, contact sheet, manifest and exact source commit evidence.
        package=output/(args.plugin+"-quick4-"+source_commit[:12]+".zip")
        with zipfile.ZipFile(package,"w",zipfile.ZIP_DEFLATED) as archive:
            for name in sorted(expected | {"manifest.json","local-build-report.json"}):
                archive.write(output/name,arcname=name)
        print(f"CAPTURE ZIP: {package}")
        print(f"PASS: {args.plugin} compiled locally from {source_commit[:12]}"
              f"{' (dirty)' if dirty else ''}, {len(entries)} actual JUCE screenshots -> {output}")
        return 0
    except (subprocess.CalledProcessError,RuntimeError,OSError,ValueError,KeyError) as error:
        print(f"FAILED {args.plugin}: {error}",file=sys.stderr)
        return 1

if __name__=="__main__":
    sys.exit(main())

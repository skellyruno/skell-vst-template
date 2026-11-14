#!/usr/bin/env python3
"""Persistent Quick4 workspace for genuine JUCE plugin captures.

The workspace stays on the user's disk between chats. Clones JUCE 8.0.12 and
the selected plugin once, then reuses source/dependency directories and Ninja
build cache. A newly supplied --ref is fetched ONLY with explicit --update.
This is an interactive command; nothing runs in the background or in CI.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

from quick4_projects import PROJECTS, main as run_quick4

CORE = Path(__file__).resolve().parents[1]
OWNER = "skellyruno"
JUCE_TAG = "8.0.12"


def run(*args: str) -> None:
    subprocess.run(list(args), check=True)


def output(*args: str) -> str:
    return subprocess.check_output(list(args), text=True).strip()


def require_clean(path: Path) -> None:
    if output("git", "-C", str(path), "status", "--porcelain"):
        raise RuntimeError(
            f"{path} has uncommitted changes; refusing to switch source revisions"
        )


def ensure_juce(location: Path, *, offline: bool) -> None:
    if (location / "CMakeLists.txt").is_file():
        return  # Never update or delete the pinned local JUCE cache implicitly.
    if location.exists():
        raise RuntimeError(f"JUCE directory exists but has no CMakeLists.txt: {location}")
    if offline:
        raise RuntimeError(f"Offline: no JUCE {JUCE_TAG} source at {location}")
    location.parent.mkdir(parents=True, exist_ok=True)
    run("git", "clone", "--depth", "1", "--branch", JUCE_TAG,
        "https://github.com/juce-framework/JUCE.git", str(location))


def ensure_plugin(name: str, location: Path, *, ref: str | None,
                  update: bool, offline: bool) -> None:
    if not (location / ".git").exists():
        if location.exists():
            raise RuntimeError(f"Refusing to overwrite non-git project: {location}")
        if offline:
            raise RuntimeError(f"Offline: no plugin source checkout at {location}")
        location.parent.mkdir(parents=True, exist_ok=True)
        run("git", "clone", f"https://github.com/{OWNER}/{name}.git", str(location))
    if not (location / "CMakeLists.txt").is_file():
        raise RuntimeError(f"Plugin checkout has no CMakeLists.txt: {location}")
    if update:
        if offline:
            raise RuntimeError("--offline and --update are mutually exclusive")
        if not ref:
            raise RuntimeError("--update requires an explicit --ref (no implicit branch switching)")
        require_clean(location)
        # Never merge, force reset, or overwrite someone else's work.
        run("git", "-C", str(location), "fetch", "--depth", "1", "origin", ref)
        run("git", "-C", str(location), "checkout", "--detach", "FETCH_HEAD")
    elif ref:
        require_clean(location)
        candidates = [ref, f"origin/{ref}"]
        commit = None
        for candidate in candidates:
            try:
                commit = output("git", "-C", str(location),
                                "rev-parse", "--verify", f"{candidate}^{{commit}}")
                break
            except subprocess.CalledProcessError:
                continue
        if commit is None:
            raise RuntimeError(
                f"Ref {ref!r} is not cached: rerun with --update to fetch it"
            )
        run("git", "-C", str(location), "checkout", "--detach", commit)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin", required=True, choices=sorted(PROJECTS))
    parser.add_argument("--workspace", type=Path,
                        default=Path(os.environ.get("SKELL_QUICK4_HOME",
                           str(Path.home() / "Skelly" / "Quick4"))))
    parser.add_argument("--ref", help="Exact plugin branch, tag or SHA to capture")
    parser.add_argument("--update", action="store_true",
                        help="Explicitly fetch selected upstream source revision")
    parser.add_argument("--offline", action="store_true",
                        help="Never clone/fetch; only use existing workspace")
    parser.add_argument("--full", action="store_true",
                        help="Deep UI audit instead of four Quick4 scenes")
    parser.add_argument("--jobs", type=int, default=1,
                        help="C++ compile parallelism (default one for 13 GB RAM)")
    parser.add_argument("--define", action="append", default=[],
                        help="Optional extra CMake NAME=VALUE")
    args = parser.parse_args()

    root = args.workspace.expanduser().resolve()
    project = root / "projects" / args.plugin
    juce = root / "vendor" / f"JUCE-{JUCE_TAG}"
    build = root / "build" / args.plugin
    captures = root / "captures" / args.plugin
    if args.update and args.offline:
        parser.error("--offline and --update cannot both be set")
    if not shutil.which("git"):
        parser.error("Git is required for exact-version source provenance")
    if not (CORE / "SkellCore" / "CMakeLists.txt").is_file():
        parser.error("Run this tool from a full SkellCore repository checkout")

    try:
        ensure_juce(juce, offline=args.offline)
        ensure_plugin(args.plugin, project, ref=args.ref,
                      update=args.update, offline=args.offline)
        sha = output("git", "-C", str(project), "rev-parse", "HEAD")
        print(f"Quick4 source: {project} @ {sha}", flush=True)
        print(f"Persistent build cache: {build}", flush=True)
        command = [
            "--plugin", args.plugin,
            "--project", str(project), "--juce", str(juce),
            "--skellcore", str(CORE), "--build", str(build),
            "--output", str(captures), "--jobs", str(max(1,args.jobs)),
        ]
        if args.full:
            command.append("--full")
        for value in args.define:
            command.extend(["--define", value])
        saved_argv = sys.argv
        try:
            sys.argv = [str(CORE / "tools" / "quick4_projects.py"), *command]
            return run_quick4()
        finally:
            sys.argv = saved_argv
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"BLOCKED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())

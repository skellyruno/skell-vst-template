# Skell Quick4 — persistent real-editor capture

Quick4 builds and captures the actual JUCE AudioProcessorEditor locally. It
never creates mockups or starts GitHub screenshot workflows. Its source,
dependencies, objects, and PNGs live in a stable folder on your own computer,
so a new ChatGPT conversation does not require a fresh download or full build.
Ninja compiles changed files when the source revision advances.

## Windows: Visual Studio Developer PowerShell

Install Python 3, Git, CMake, Ninja, and Visual Studio/MSVC once. Clone
skellyruno/skellcore-vst-template on your PC. In that repository, run:

    .\quick4.ps1 -Plugin skell-preset-vst -Ref feat/preset-quick4-local-persistent-20261114 -Update

After the initial clone and build, reuse cached source and build objects:

    .\quick4.ps1 -Plugin skell-preset-vst -Offline

Use -Full for the deep 110-scene UI audit instead of the standard four
captures. Windows needs an interactive GUI session but not Xvfb.

## Linux

Install JUCE Linux development headers, Git, CMake, Ninja, C++17 compiler,
Xvfb and Xauth once. From the SkellCore checkout run:

    bash quick4.sh --ref feat/preset-quick4-local-persistent-20261114 --update
    bash quick4.sh --offline

## Persistent directories

Default: ~/Skelly/Quick4 (Windows user home or Linux home).
Override with -Workspace (PowerShell), --workspace (Linux), or
SKELL_QUICK4_HOME environment variable.

- projects/skell-preset-vst — actual Git source at the selected SHA
- vendor/JUCE-8.0.12 — cached pinned JUCE, cloned once
- build/skell-preset-vst — persistent CMake/Ninja incremental build
- captures/skell-preset-vst — four genuine PNGs, manifest, contact sheet,
  local-build-report.json, and a source-SHA-named portable capture ZIP

Example four real scenes: Simple Tone/Punch/Space/Grit (100%), Advanced
effect editor (100%), movable effect library (70%), MIC widget (70%).
The deep full audit remains intact.

Each report records the exact Git SHA and dirty source state. The capture
ZIP contains the four PNGs, contact sheet, manifest and provenance. Share
that ZIP in a new ChatGPT conversation if its isolated working environment
cannot access your local Windows disk. Older ZIPs are never evidence of
newer UI changes.

For updated source use -Ref BRANCH -Update / --ref BRANCH --update.
Remote fetch and checkout require explicit flags and a clean Git worktree.
Quick4 refuses dirty worktree switching; it never force-resets your source
or merges unverified work. --offline uses only existing caches.

## Limitations and safety

A new chat's isolated /mnt/data does not automatically mount prior compiled
objects, or your Windows machine. The permanent cache exists on YOUR disk,
not across separate ChatGPT containers. Native source and GUI dependencies
must be available to rebuild after a change. The capture ZIP is the
cross-chat file handoff when no shared file mount exists.

A Quick4 pass verifies a real UI screenshot, not Windows VST3 packaging,
latency, CPU, FL Studio audio compatibility, or sound quality. Preserve those
separate gates. GitHub ui-screenshot workflows stay manual-only.
Quick4's Python unit-test CI is separate and NEVER captures screenshots.

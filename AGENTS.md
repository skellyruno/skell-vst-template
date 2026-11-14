# SKELLY ECOSYSTEM CODING AGENT RULES 🩻

**Never make up the current appearance of a Skelly JUCE plugin.** User requests like "show the UI" require the genuine compiled current-source JUCE AudioProcessorEditor and exact commit. Do not use concept-art/image generation, a WAV audio-render utility, or a previous revision's CI screenshot. On any blocker say which dependency, source or target is missing. The standard is a *local* Quick4 actual-editor capture, not GitHub Actions screenshots.

- Primary policy: `docs/PLUGIN_QUICK4_ROLLOUT.md`.
- Shared local CMake/C++/Xvfb router: `tools/quick4_projects.py`; inspect `--list` for projects and honest verification status.
- Each wired plugin has `AGENTS.md` and `docs/LOCAL_UI_SCREENSHOTS.md` with its actual CMake target.
- Default **four** real scenes: default, control/menu, active, supported alternative theme/state; `--full` for explicit deep regression.
- Always **compile the actual C++ editor** after UI source changes and inspect PNGs; never use an old binary as evidence of a new edit.
- Local screenshot validation is **NOT** Windows VST3 build, DSP, latency, CPU, FL Studio or release validation. Keep those independent.
- Do not trigger automatic GitHub UI screenshot jobs for routine visual changes; GitHub is for version control and independent release workflows.
- Migration is incremental: Master, Mini, and Flang have been locally verified; Silence, Preset, DeEsser, Fan are wired but await proof; other plugins are backlog.

## Persistent Quick4 handoff
- Run the single-command Windows/Linux wrapper from this repository (quick4.ps1 / quick4.sh) via tools/quick4_workspace.py. Details: docs/QUICK4_PERSISTENT_WORKSPACE.md.
- Cached source, JUCE, and incremental builds live under the user's stable ~/Skelly/Quick4 workspace, not a chat's transient /mnt/data mount.
- Each real Quick4 capture must include the exact Git SHA, screenshots, contact sheet and portable ZIP. Share that ZIP into an isolated new chat if the user's disk is not connected. Never claim that a prior ZIP depicts a newer revision.
- Plugin renderer Quick4 flags are opt-in; preserve original full audits and manual-only screenshot workflows. Pure-Python CI tooling tests do not capture screenshots.

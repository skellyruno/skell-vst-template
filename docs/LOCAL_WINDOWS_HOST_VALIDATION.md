# Local Windows SkellCore host and CPU verification

The SkellCore Quickstart host-readiness harness can run directly on your Windows
PC, using the same **real JUCE processor** as the GitHub Actions tests. It is
not a substitute for tests inside FL Studio.

## Run

Requirements: Windows, CMake, Git, Visual Studio Desktop C++ build tools.

PowerShell in a checkout of SkellCore PR #26:

    powershell -ExecutionPolicy Bypass -File .\tools\Run-SkellHostValidation.ps1

To rerun an existing binary without rebuilding:

    powershell -ExecutionPolicy Bypass -File .\tools\Run-SkellHostValidation.ps1 -SkipBuild

To additionally compile and run the portable DSP suite:

    powershell -ExecutionPolicy Bypass -File .\tools\Run-SkellHostValidation.ps1 -WithDSPTests

The script does not install the VST3, change FL Studio settings, or modify
audio devices. It builds to build-host-local and saves these files locally:

- host-reports/host-readiness.log — full test output
- host-reports/host-matrix.csv — 36 separate measured cases
- host-reports/host-summary.json — CPU/OS, source commit and VST3 SHA256

These folders are ignored by Git. For release safety, -SkipBuild now
requires a **clean Git checkout** and a matching build revision stamp. If
source changes, the command refuses to associate an older executable with
a newer commit; rerun without -SkipBuild instead. A source tree modified
outside Git is not a releasable binary provenance claim.

## Test coverage

Rates 44100/48000/96000 Hz, 64/128/512 sample blocks, and 1/4/8/16
simultaneous processor instances, plus mono-bus support. Checks include
finite outputs, automation/bypass correctness, per-instance independence,
serialized parameter restore, invalid state handling and editor open/close.

Matrix timing is serial processBlock CPU only; it excludes sample generation,
ASIO driver, FL Studio mixer, VST3 hosting, GUI compositing and other plugins.
The serial time ratio is not a real DAW CPU percentage or RTL latency.
Compare repeated runs only on comparable hardware/power/build settings.

## Actual FL Studio acceptance

Generate original reference audio and a data sheet:

    python tools/Make-FLStudio-TestKit.py --out host-test-kit

Use the reference.wav and results.csv to follow
docs/FL_STUDIO_HOST_ACCEPTANCE.md. Run on the exact installed VST3 binary
identified by a build SHA and SHA256 hash. Save/reload the FLP, automate Gain,
toggle Enabled, and record CPU/dropouts for 1/4/8/16 mixer instances.

The user-machine FL Studio gate remains unverified until the DAW is actually
run. Host-readiness and pluginval test success do not certify FL Studio.

## Troubleshooting

- CMake missing: install CMake and add it to PATH.
- Visual Studio generator missing: install Desktop development with C++,
  matching MSVC C++ toolset and Windows SDK.
- Missing executable under -SkipBuild: rerun without that switch.
- Failed host-check: read host-reports/host-readiness.log for the case and
  keep the failing build SHA for reproduction.
- Inconsistent CPU readings: rerun with the same sample rates, block sizes,
  power plan and build configuration; compare a distribution of measurements.

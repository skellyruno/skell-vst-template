#!/usr/bin/env python3
"""Validate self-reported FL Studio host measurements without inventing passes.

Input: results.csv created by Make-FLStudio-TestKit.py and filled manually.
Output: a Markdown review summary; never changes the original results file.
"""
import argparse
import csv
import math
from pathlib import Path

FIELDS = (
    "baseline_cpu_percent", "enabled_cpu_percent", "cpu_peak_percent",
    "dropouts", "project_reopen_pass", "automation_pass"
)
RATES = (44100, 48000, 96000)
BLOCKS = (64, 128, 512)
COUNTS = (1, 4, 8)
EXPECTED = {(r, b, n) for r in RATES for b in BLOCKS for n in COUNTS}
EXPECTED |= {(r, b, 16) for r in RATES for b in (128, 512)}
CRITICAL = {(48000, 128, n) for n in (1, 4, 8, 16)}
HEADERS = ("sample_rate_hz", "host_buffer_samples", "instances",
           *FIELDS, "notes")

def parsed_boolean(value, label):
    value = value.strip().lower()
    if value in ("yes", "true", "pass", "1"):
        return True
    if value in ("no", "false", "fail", "0"):
        return False
    raise ValueError(f"{label}: use pass/fail, yes/no or true/false")

def analyse(csv_file):
    rows = []
    with Path(csv_file).open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None or tuple(reader.fieldnames) != HEADERS:
            raise ValueError("Results CSV columns do not match the supplied acceptance kit")
        rows = list(reader)
    keys = []
    summary = {"passed": [], "failed": [], "pending": [], "skipped": []}
    for line_num, row in enumerate(rows, start=2):
        try:
            key = tuple(int(row[col]) for col in HEADERS[:3])
        except (TypeError, ValueError) as exc:
            raise ValueError(f"Line {line_num}: invalid rate/buffer/instance number") from exc
        if key not in EXPECTED:
            raise ValueError(f"Line {line_num}: unexpected scenario {key}")
        if key in keys:
            raise ValueError(f"Line {line_num}: duplicate scenario {key}")
        keys.append(key)
        values = [row[field].strip() for field in FIELDS]
        notes = (row["notes"] or "").strip()
        if all(not value for value in values):
            summary["pending"].append(key)
            continue
        if all(value.casefold() in ("n/a", "na", "unsupported") for value in values):
            if not notes:
                raise ValueError(f"Line {line_num}: unsupported scenario needs a reason in notes")
            summary["skipped"].append(key)
            continue
        if any(not value for value in values):
            summary["pending"].append(key)
            continue
        if any(value.casefold() in ("n/a", "na", "unsupported") for value in values):
            raise ValueError(f"Line {line_num}: partial N/A; either measure all fields or mark whole row unsupported")

        try:
            cpu = [float(row[field]) for field in FIELDS[:3]]
            if not all(math.isfinite(v) and v >= 0 for v in cpu):
                raise ValueError("non-finite or negative CPU value")
            dropouts = int(row["dropouts"])
            if dropouts < 0:
                raise ValueError("negative dropout count")
            recovered = parsed_boolean(row["project_reopen_pass"], "project_reopen_pass")
            automated = parsed_boolean(row["automation_pass"], "automation_pass")
        except (ValueError, TypeError) as exc:
            raise ValueError(f"Line {line_num}, case {key}: {exc}") from exc
        observation = (key, cpu, dropouts, notes)
        if dropouts or not recovered or not automated:
            summary["failed"].append(observation)
        else:
            summary["passed"].append(observation)
    missing = EXPECTED - set(keys)
    if missing:
        raise ValueError(f"Results sheet is missing {len(missing)} required cases")
    if len(rows) != len(EXPECTED):
        raise ValueError("Result sheet row count differs from acceptance matrix")
    measured = {row[0] for row in summary["passed"] + summary["failed"]}
    summary["critical_measured"] = CRITICAL.issubset(measured)
    return summary

def markdown_summary(summary, source_path):
    done = len(summary["passed"]) + len(summary["failed"])
    failed = len(summary["failed"])
    pending = len(summary["pending"])
    skipped = len(summary["skipped"])
    if failed:
        status = "REPORTED FAILURES — investigate before approval"
    elif pending or skipped or not summary["critical_measured"]:
        status = "INCOMPLETE — host acceptance not certified"
    else:
        status = "READY FOR MANUAL REVIEW — not independent FL Studio certification"
    lines = [
        "# SkellCore FL Studio host measurement review", "",
        f"Source sheet: {Path(source_path).name}", "",
        f"**Status: {status}**", "",
        f"Recorded pass rows: {len(summary['passed'])} / {len(EXPECTED)}",
        f"Recorded failure rows: {failed}",
        f"Pending rows: {pending}",
        f"Unsupported rows: {skipped}",
        f"Critical 48 kHz / 128-sample 1/4/8/16 cases measured: {'yes' if summary['critical_measured'] else 'no'}",
        "", "These are manually supplied FL Studio observations, not independent test results.",
        "Even a fully completed passing sheet requires review of the actual FLP, installed binary hash and GUI behavior.",
        ""
    ]
    if summary["failed"]:
        lines += ["## Reported failures", "", "| Rate | Buffer | Instances | Dropouts | CPU peak (%) | Notes |",
                  "| --- | --- | --- | --- | --- | --- |"]
        for (rate, block, n), cpu, drops, notes in summary["failed"]:
            notes = notes.replace("|", "/").replace("\n", " ")
            lines.append(f"| {rate} | {block} | {n} | {drops} | {cpu[2]:.2f} | {notes} |")
        lines.append("")
    if done:
        peak = max(summary["passed"] + summary["failed"], key=lambda item: item[1][2])
        lines += ["## Largest recorded FL Studio CPU peak", "",
                  f"{peak[1][2]:.2f}% reported in {peak[0][0]} Hz, "
                  f"{peak[0][1]}-sample, {peak[0][2]}-instance scenario.", "",
                  "Do not compare these DAW values directly to serial C++ CI microbenchmarks.", ""]
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, help="Manually filled FL Studio results.csv")
    parser.add_argument("--out", type=Path, default=None)
    opts = parser.parse_args()
    try:
        summary = analyse(opts.csv)
    except (ValueError, OSError) as exc:
        parser.error(str(exc))
    path = opts.out or opts.csv.with_name("flstudio-review.md")
    path.write_text(markdown_summary(summary, opts.csv), encoding="utf-8")
    print(f"FL Studio review written to {path}")
    print(f"Measured: {len(summary['passed'])+len(summary['failed'])}, "
          f"failed: {len(summary['failed'])}, "
          f"pending: {len(summary['pending'])}, skipped: {len(summary['skipped'])}")
    print("This is a self-reported acceptance review, not an automated FL Studio host pass.")

if __name__ == "__main__":
    main()

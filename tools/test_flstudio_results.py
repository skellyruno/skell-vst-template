#!/usr/bin/env python3
"""Regression tests for self-reported FL Studio results review."""
import csv
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "skell_fl_results", ROOT / "tools" / "Review-FLStudio-Results.py")
results = importlib.util.module_from_spec(spec)
spec.loader.exec_module(results)

class ReviewHostResultsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="skell-fl-results-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        subprocess.run([sys.executable,
                        str(ROOT / "tools" / "Make-FLStudio-TestKit.py"),
                        "--out", str(self.directory)],
                       check=True, capture_output=True)
        self.path = self.directory / "results.csv"
        with self.path.open(newline="", encoding="utf-8") as stream:
            reader = csv.DictReader(stream)
            self.rows = list(reader)
            self.headers = reader.fieldnames

    def rewrite(self):
        with self.path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=self.headers)
            writer.writeheader()
            writer.writerows(self.rows)

    @staticmethod
    def set_pass(row):
        row.update(baseline_cpu_percent="2.0", enabled_cpu_percent="2.4",
                   cpu_peak_percent="3.7", dropouts="0",
                   project_reopen_pass="pass", automation_pass="pass")

    def test_blank_sheet_is_incomplete_not_a_pass(self):
        review = results.analyse(self.path)
        self.assertEqual(len(review["pending"]), 33)
        self.assertEqual(len(review["passed"]), 0)
        self.assertFalse(review["critical_measured"])
        self.assertIn("INCOMPLETE", results.markdown_summary(review,self.path))

    def test_critical_cases_measured_but_other_rows_skipped(self):
        for row in self.rows:
            key = (int(row["sample_rate_hz"]), int(row["host_buffer_samples"]),
                   int(row["instances"]))
            if key in results.CRITICAL:
                self.set_pass(row)
            else:
                row.update({field: "N/A" for field in results.FIELDS})
                row["notes"] = "Driver configuration not available"
        self.rewrite()
        review = results.analyse(self.path)
        self.assertTrue(review["critical_measured"])
        self.assertEqual(len(review["passed"]), 4)
        self.assertEqual(len(review["skipped"]), 29)
        self.assertIn("INCOMPLETE", results.markdown_summary(review,self.path))

    def test_detects_automation_failure_and_dropouts(self):
        self.set_pass(self.rows[0])
        self.rows[0]["dropouts"] = "3"
        self.rows[0]["automation_pass"] = "fail"
        self.rewrite()
        review = results.analyse(self.path)
        self.assertEqual(len(review["failed"]), 1)
        self.assertIn("REPORTED FAILURES", results.markdown_summary(review,self.path))

    def test_rejects_bad_data_duplicates_and_missing_cases(self):
        self.set_pass(self.rows[0])
        self.rows[0]["cpu_peak_percent"] = "nan"
        self.rewrite()
        with self.assertRaisesRegex(ValueError, "non-finite"):
            results.analyse(self.path)
        self.rows[0]["cpu_peak_percent"] = "3"
        original_second = dict(self.rows[1])
        self.rows[1]["instances"] = self.rows[0]["instances"]
        self.rows[1]["host_buffer_samples"] = self.rows[0]["host_buffer_samples"]
        self.rows[1]["sample_rate_hz"] = self.rows[0]["sample_rate_hz"]
        self.rewrite()
        with self.assertRaisesRegex(ValueError, "duplicate"):
            results.analyse(self.path)
        self.rows[1] = original_second
        del self.rows[-1]
        self.rewrite()
        with self.assertRaisesRegex(ValueError, "missing"):
            results.analyse(self.path)

    def test_rejects_partial_and_unsupported_without_reason(self):
        self.rows[0]["baseline_cpu_percent"] = "5.0"
        self.rewrite()
        review = results.analyse(self.path)
        self.assertEqual(len(review["pending"]), 33)
        self.rows[0].update({field: "N/A" for field in results.FIELDS})
        self.rewrite()
        with self.assertRaisesRegex(ValueError, "needs a reason"):
            results.analyse(self.path)

if __name__ == "__main__":
    unittest.main()

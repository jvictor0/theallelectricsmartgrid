import copy
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ipad_perf


class ResultsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.path = Path(self.directory.name) / "perf.sqlite3"
        self.db = ipad_perf.open_database(self.path)
        self.request = {"run_id": "run", "case": "custom", "arguments": ["one"],
                        "notes": "Measure a new algorithm", "timeout": 10}
        ipad_perf.start_run(self.db, self.request)
        self.report = {**self.request, "git_commit": "a" * 40, "git_dirty": True,
                       "case_notes": "Find the cost per operation", "status": "completed",
                       "results": [{"ns_per_item": 2.5, "items": 9, "variant": "fast"}],
                       "telemetry": [{"thermal_state": 0}, {"thermal_state": 1}],
                       "device": {"model": "test"}}

    def tearDown(self):
        self.db.close()
        self.directory.cleanup()

    def test_arbitrary_metrics_and_provenance_survive_reopening_database(self):
        ipad_perf.save_report(self.db, self.request, self.report)
        with sqlite3.connect(self.path) as reader:
            row = reader.execute("SELECT case_name, git_commit, git_dirty, notes, case_notes, "
                                 "results_json, telemetry_json, app_json FROM runs").fetchone()
        self.assertEqual(row[:5], ("custom", "a" * 40, 1, "Measure a new algorithm", "Find the cost per operation"))
        self.assertEqual(json.loads(row[5]), self.report["results"])
        self.assertEqual(json.loads(row[6]), self.report["telemetry"])
        self.assertEqual(json.loads(row[7])["device"], {"model": "test"})

    def test_mismatched_run_and_nonfinite_metrics_cannot_be_saved_as_success(self):
        for change in ({"run_id": "stale"}, {"arguments": []}, {"results": [{"value": float("nan")}]},
                       {"results": [42]}, {"status": "running"}, {"git_commit": ""}):
            report = {**copy.deepcopy(self.report), **change}
            with self.assertRaises(ValueError):
                ipad_perf.save_report(self.db, self.request, report)
            self.assertEqual(self.db.execute("SELECT status FROM runs").fetchone()[0], "running")

    def test_device_failure_retains_results_and_telemetry(self):
        self.report.update(status="failed", error="bad parameter")
        ipad_perf.save_report(self.db, self.request, self.report)
        row = self.db.execute("SELECT status, error, telemetry_json FROM runs").fetchone()
        self.assertEqual(row[:2], ("failed", "bad parameter"))
        self.assertEqual(json.loads(row[2]), self.report["telemetry"])

    def test_host_failure_retains_request(self):
        ipad_perf.fail_run(self.db, "run", "connection lost")
        row = self.db.execute("SELECT status, error, notes, arguments_json FROM runs").fetchone()
        self.assertEqual(row, ("failed", "connection lost", self.request["notes"], '["one"]'))


if __name__ == "__main__":
    unittest.main()

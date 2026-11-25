from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import copy
from io import StringIO
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
from threading import Barrier
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ipad_perf
import ipad_perf_lock


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


class LockTests(unittest.TestCase):
    def setUp(self):
        contexts = ExitStack()
        self.addCleanup(contexts.close)
        directory = contexts.enter_context(tempfile.TemporaryDirectory())
        self.results = Path(directory) / "results.sqlite3"
        contexts.enter_context(patch.object(ipad_perf_lock, "LOCK_FILE", Path(directory) / "ipad-perf.lock"))

    def run_cli(self, owner, *args, event=None):
        output, errors = StringIO(), StringIO()
        with ExitStack() as contexts:
            contexts.enter_context(patch.dict("os.environ", CODEX_THREAD_ID=owner))
            contexts.enter_context(patch.object(sys, "argv", ["ipad_perf.py", *args]))
            contexts.enter_context(patch.object(sys, "stdin", StringIO(json.dumps(event))))
            contexts.enter_context(redirect_stdout(output))
            contexts.enter_context(redirect_stderr(errors))
            status = ipad_perf.main()
        return status, output.getvalue(), errors.getvalue()

    def test_only_the_owner_can_run_or_release(self):
        args = ("--skip-install", "--db", str(self.results))
        status, _, error = self.run_cli("thread-a", *args)
        self.assertEqual(status, 1)
        self.assertIn("ipad_perf.py --lock", error)
        self.assertIn("Keep it for the entire battery", error)
        self.assertEqual(self.run_cli("thread-a", "--lock")[0], 0)
        self.assertEqual(self.run_cli("thread-a", "--lock")[0], 0)
        self.assertEqual(self.run_cli("thread-a", "--check-lock")[0], 0)
        status, _, error = self.run_cli("thread-b", *args)
        self.assertEqual(status, 1)
        self.assertIn("thread-a", error)
        self.assertFalse(self.results.exists())
        self.assertEqual(self.run_cli("thread-b", "--unlock")[0], 1)
        self.assertIn("thread-a", self.run_cli("", "--lock-status")[1])
        self.assertEqual(self.run_cli("thread-a", "--unlock")[0], 0)
        self.assertEqual(ipad_perf_lock.lock_owner(), "")

    def test_force_requires_unlock_and_can_clear_another_owner_without_a_thread_id(self):
        ipad_perf_lock.acquire_lock("stale-thread", 0)
        with self.assertRaises(SystemExit) as failure:
            self.run_cli("", "--force")
        self.assertEqual(failure.exception.code, 2)
        self.assertEqual(ipad_perf_lock.lock_owner(), "stale-thread")
        status, output, _ = self.run_cli("", "--unlock", "--force")
        self.assertEqual(status, 0)
        self.assertIn("stale-thread", output)
        self.assertEqual(ipad_perf_lock.lock_owner(), "")

    def test_concurrent_acquisition_has_one_winner(self):
        starting = Barrier(2)

        def claim(owner):
            starting.wait()
            try:
                ipad_perf_lock.acquire_lock(owner, 0)
                return owner
            except ValueError:
                return None

        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(claim, ("thread-a", "thread-b")))
        self.assertEqual([owner for owner in results if owner], [ipad_perf_lock.lock_owner()])

    def test_waiter_can_claim_after_release_without_changing_the_previous_owner(self):
        ipad_perf_lock.acquire_lock("thread-a", 0)

        def release_during_wait(delay):
            self.assertGreater(delay, 0)
            self.assertEqual(ipad_perf_lock.lock_owner(), "thread-a")
            ipad_perf_lock.release_lock("thread-a")

        with patch.object(ipad_perf_lock.time, "sleep", side_effect=release_during_wait):
            status, _, errors = self.run_cli("thread-b", "--lock", "--wait", "5")
        self.assertEqual(status, 0)
        self.assertIn("Waiting", errors)
        self.assertEqual(ipad_perf_lock.lock_owner(), "thread-b")

    def test_stop_hook_reminds_only_the_owner_without_releasing(self):
        ipad_perf_lock.acquire_lock("thread-a", 0)
        status, output, _ = self.run_cli("", "--stop-hook", event={"session_id": "thread-b"})
        self.assertEqual((status, output), (0, ""))
        event = {"session_id": "thread-a", "stop_hook_active": False}
        status, output, _ = self.run_cli("", "--stop-hook", event=event)
        self.assertEqual(status, 0)
        self.assertEqual(json.loads(output)["decision"], "block")
        self.assertIn("ipad_perf.py --unlock", json.loads(output)["reason"])
        event["stop_hook_active"] = True
        status, output, _ = self.run_cli("", "--stop-hook", event=event)
        self.assertEqual(status, 0)
        self.assertIn("systemMessage", json.loads(output))
        self.assertEqual(ipad_perf_lock.lock_owner(), "thread-a")


if __name__ == "__main__":
    unittest.main()

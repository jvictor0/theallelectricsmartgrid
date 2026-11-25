from io import BytesIO
import fcntl
import json
import os
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import lean_studio
import lean_studio_worker as worker
import resource_lock


class StudioTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.queue = worker.Queue(self.root)
        self.job = "a" * 32
        self.queue.job_dir(self.job).mkdir(parents=True)
        self.request = {"kind": "make", "args": ["lean-test"], "memory_mib": 64, "thread": "test-thread"}
        with self.queue.db() as db:
            db.execute("INSERT INTO jobs (id, request) VALUES (?, ?)", (self.job, json.dumps(self.request)))

    def test_snapshot_rejects_paths_outside_workspace(self):
        data = BytesIO()
        with tarfile.open(fileobj=data, mode="w") as archive:
            archive.addfile(tarfile.TarInfo("../escape"))
        data.seek(0)
        with tarfile.open(fileobj=data) as archive:
            with self.assertRaisesRegex(ValueError, "Invalid snapshot path"):
                worker.extract_files(archive, self.root / "source", worker.source_path)

    def test_scratch_check_and_output_are_explicit(self):
        scratch = worker.SCRATCH / "Test.lean"
        output = worker.SCRATCH / "Test.olean"
        (self.root / scratch).parent.mkdir(parents=True)
        (self.root / scratch).touch()
        with patch.object(lean_studio, "REPO", self.root), patch.dict(os.environ, SMARTGRID_LEAN_INPUTS=""):
            inputs, outputs = lean_studio.inputs_and_outputs(["env", "lean", str(scratch), "-o", str(output)])
        self.assertEqual(inputs, {scratch})
        self.assertEqual(outputs, {output})
        with patch.object(worker.os, "access", return_value=True), patch.object(worker.subprocess, "call") as call:
            worker.run_lake(["env", "lean", str(scratch)])
        self.assertEqual(call.call_args.args[0][1:], ["lean", str(scratch), "--", "--threads=1"])

    def test_run_prepares_imports_and_preserves_program_args(self):
        script = str(worker.SCRATCH / "Test.lean")
        args = ["--dir", ".", "-v", "env", "lean", "--run", script, "-o", "program-arg"]
        with patch.object(worker.os, "access", return_value=True), patch.object(worker.subprocess, "call", return_value=0) as call:
            self.assertEqual(worker.run_lake(args), 0)
        prepare, run = call.call_args_list
        self.assertEqual(prepare.args[0][1:], ["--dir", ".", "-v", "setup-file", script])
        self.assertEqual(run.args[0][1:7], ["--dir", ".", "-v", "env", "lean", "--threads=1"])
        self.assertEqual(run.args[0][-4:], ["--run", script, "-o", "program-arg"])
        self.assertEqual(lean_studio.inputs_and_outputs(args)[1], set())

    def test_lake_global_options_and_query_are_forwarded(self):
        args = ["--dir", ".", "-v", "query", "DSP.Processor"]
        with patch.object(worker.os, "access", return_value=True), patch.object(worker.subprocess, "call") as call:
            worker.run_lake(args)
        self.assertEqual(call.call_args.args[0][1:], args)

    def test_snapshot_includes_pending_source_and_omits_deleted_files(self):
        (self.root / "edited.lean").write_text("-- edited\n")
        (self.root / "new.lean").write_text("-- new\n")
        archive = self.root / "snapshot.tar.gz"
        request = {}
        with patch.object(lean_studio, "REPO", self.root), patch.object(
            lean_studio, "git", side_effect=[b"edited.lean\0deleted.lean\0new.lean\0", b"base\n"]
        ):
            lean_studio.snapshot(request, set(), archive)
        with tarfile.open(archive) as stream:
            self.assertEqual(stream.getnames(), ["edited.lean", "new.lean"])
            self.assertEqual(stream.extractfile("edited.lean").read(), b"-- edited\n")
        self.assertEqual(request["base_commit"], "base")
        self.assertEqual(request["source_sha256"], worker.file_digest(archive))

    def test_changed_local_header_is_preserved_and_returned_as_patch(self):
        relative = worker.GENERATED / "Test.hpp"
        local = self.root / relative
        local.parent.mkdir(parents=True)
        local.write_text("submitted\n")
        job = self.queue.job_dir(self.job)
        (job / "request.json").write_text(json.dumps({"outputs": {str(relative): worker.file_digest(local)}}))
        output = job / "outputs" / relative
        output.parent.mkdir(parents=True)
        output.write_text("generated\n")
        local.write_text("edited while queued\n")
        with patch.object(lean_studio, "REPO", self.root):
            with self.assertRaisesRegex(ValueError, "Local outputs changed"):
                lean_studio.receive_outputs(self.job, job)
        self.assertEqual(local.read_text(), "edited while queued\n")
        self.assertIn("+generated", (job / "generated.patch").read_text())

    def test_new_formal_headers_are_returned(self):
        directory = self.queue.job_dir(self.job)
        for path in (worker.GENERATED / "New.hpp", worker.FORMAL_OUTPUTS["lean-dsp-generate"] / "New.hpp"):
            file = self.queue.workspace / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("new header")
        self.queue.save_outputs({"kind": "make", "args": ["lean-dsp-generate"], "outputs": {}}, directory)
        with tarfile.open(directory / "outputs.tar.gz") as stream:
            self.assertEqual(stream.getnames(), ["build/lean/generated/New.hpp", "formal/DSP/generated/New.hpp"])

    def test_output_baseline_check_holds_worktree_lock(self):
        job = self.queue.job_dir(self.job)
        relative = worker.GENERATED / "Test.hpp"
        local = self.root / relative
        (job / "request.json").write_text(json.dumps({"outputs": {}}))
        generated = job / "outputs" / relative
        generated.parent.mkdir(parents=True)
        generated.write_text("generated\n")

        def digest(path):
            with (self.root / lean_studio.RESULTS / "outputs.lock").open() as slot:
                with self.assertRaises(BlockingIOError):
                    fcntl.flock(slot, fcntl.LOCK_EX | fcntl.LOCK_NB)
            return worker.file_digest(path)

        with patch.object(lean_studio, "REPO", self.root), patch.object(lean_studio, "file_digest", side_effect=digest):
            lean_studio.receive_outputs(self.job, job)
        self.assertEqual(local.read_text(), "generated\n")

    def test_cancel_queued_job(self):
        self.queue.cancel(self.job)
        row = self.queue.row(self.job)
        self.assertEqual((row["state"], row["code"]), ("canceled", 130))

    def test_retry_keeps_one_persistent_lock(self):
        budgets = []

        def attempt(job, env, stdout, stderr, memory, budget):
            lock = json.loads(resource_lock.owner(self.queue.build_lock))
            self.assertEqual(lock, {"job": self.job, "thread": "test-thread"})
            self.assertEqual(env["LEAN_NUM_THREADS"], "16")
            budgets.append(budget)
            return (1, "job memory budget") if len(budgets) == 1 else (0, "exit")

        memory = Mock()
        memory.pressure.return_value = 1
        with patch.object(self.queue, "attempt", side_effect=attempt):
            self.queue.run(self.queue.row(self.job), memory)
        self.assertEqual(budgets, [64, worker.MAX_MEMORY_MIB])
        self.assertEqual(self.queue.row(self.job)["state"], "completed")
        self.assertEqual(resource_lock.owner(self.queue.build_lock), "")

    def test_memory_stop_kills_the_process_group(self):
        memory = Mock()
        memory.pressure.return_value = 1
        memory.footprint.return_value = 4 * worker.MIB
        with (self.root / "attempt.log").open("wb") as log:
            code, reason = self.queue.attempt(self.job, os.environ, log, log, memory, 1)
        self.assertEqual((code, reason), (1, "job memory budget"))
        group = memory.footprint.call_args.args[0]
        self.assertFalse(worker.signal_group(group, 0))
        self.assertIsNone(self.queue.row(self.job)["pgid"])

    def test_closed_start_gate_leaves_workspace_untouched(self):
        with patch.object(self.queue, "prepare") as prepare:
            self.assertEqual(self.queue.execute(self.job, BytesIO()), 75)
        prepare.assert_not_called()

    def test_command_streams_stay_separate(self):
        directory = self.queue.job_dir(self.job)
        files = {
            "lean-toolchain": b"stub\n", "lakefile.lean": b"", "scripts/lean.sh": b"",
            "Makefile": b"lean-test:\n\t@echo result\n\t@echo diagnostic >&2\n",
        }
        with tarfile.open(directory / "source.tar.gz", "w:gz") as archive:
            for name, data in files.items():
                info = tarfile.TarInfo(name)
                info.size = len(data)
                archive.addfile(info, BytesIO(data))
        memory = Mock()
        memory.footprint.return_value = 0
        memory.pressure.return_value = 1
        self.queue.run(self.queue.row(self.job), memory)
        self.assertEqual(self.queue.row(self.job)["state"], "completed")
        self.assertEqual((directory / "stdout.log").read_text(), "result\n")
        self.assertIn("diagnostic\n", (directory / "stderr.log").read_text())


if __name__ == "__main__":
    unittest.main()

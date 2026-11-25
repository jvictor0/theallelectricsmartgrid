#!/usr/bin/env python3
"""Native macOS worker and SSH endpoint for the Studio Lean queue."""

import argparse
from contextlib import ExitStack, closing, contextmanager
import ctypes
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import signal
import sqlite3
import subprocess
import sys
import tarfile
import tempfile
import time
import uuid

import resource_lock


STUDIO_PATH = Path(".cache/theallelectricsmartgrid/lean-studio")
ROOT = Path.home() / STUDIO_PATH
MAX_MEMORY_MIB = 160 * 1024
DEFAULT_MEMORY_MIB = 128 * 1024
MIB = 1024 * 1024
SCRATCH = Path("build/lean/scratch")
GENERATED = Path("build/lean/generated")
FORMAL_OUTPUTS = {
    "lean-dsp-generate": Path("formal/DSP/generated"),
    "lean-inverse-generate": Path("formal/PhaseTime/generated"),
}
MAKE_TARGETS = {
    "lean-setup", "lean-cache", "lean-test", "lean-dsp-test",
    "lean-dsp-codegen", "lean-dsp-generate", "lean-dsp-codegen-test",
    "lean-inverse-codegen", "lean-inverse-generate", "lean-inverse-test",
}
ACTIVE = ("queued", "running")


def relpath(name):
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in ("..", ".git") for part in path.parts) or not path.parts:
        raise ValueError(f"Invalid snapshot path: {name}")
    return Path(path)


def file_digest(path):
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(MIB), b""):
            digest.update(block)
    return digest.hexdigest()


def extract_files(archive, destination, accept):
    paths = []
    for member in archive:
        path = relpath(member.name)
        if not member.isfile() or not accept(path):
            raise ValueError(f"Unexpected archive entry: {member.name}")
        target = destination / path
        target.parent.mkdir(parents=True, exist_ok=True)
        with archive.extractfile(member) as source, target.open("xb") as output:
            shutil.copyfileobj(source, output)
        target.chmod(member.mode & 0o777)
        paths.append(path)
    return paths


def source_path(path):
    return path.is_relative_to(SCRATCH) or path.parts[0] not in ("build", ".lake")


class ProcessUsage(ctypes.Structure):
    _fields_ = [("uuid", ctypes.c_ubyte * 16)] + [
        (name, ctypes.c_uint64) for name in (
            "user_time", "system_time", "package_wakeups", "interrupt_wakeups", "pageins",
            "wired", "resident", "physical_footprint", "start_time", "exit_time",
        )
    ]


class MacMemory:
    def __init__(self):
        self.proc = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
        self.proc.proc_listpids.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p, ctypes.c_int]
        self.proc.proc_listpids.restype = ctypes.c_int
        self.proc.proc_pid_rusage.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
        self.proc.proc_pid_rusage.restype = ctypes.c_int
        self.system = ctypes.CDLL("/usr/lib/libSystem.B.dylib", use_errno=True)
        self.system.sysctlbyname.argtypes = [
            ctypes.c_char_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p, ctypes.c_size_t,
        ]
        self.system.sysctlbyname.restype = ctypes.c_int

    def pressure(self):
        value = ctypes.c_int()
        size = ctypes.c_size_t(ctypes.sizeof(value))
        if self.system.sysctlbyname(b"kern.memorystatus_vm_pressure_level", ctypes.byref(value),
                                   ctypes.byref(size), None, 0):
            error = ctypes.get_errno()
            raise OSError(error, os.strerror(error))
        if value.value not in (1, 2, 4):
            raise RuntimeError(f"Unrecognized macOS memory-pressure level {value.value}")
        return value.value

    def footprint(self, group):
        # PROC_PGRP_ONLY = 2 and RUSAGE_INFO_V0 = 0 in the macOS SDK.
        size = 128
        while True:
            pids = (ctypes.c_int * size)()
            ctypes.set_errno(0)
            length = self.proc.proc_listpids(2, group, pids, ctypes.sizeof(pids))
            if length <= 0 and ctypes.get_errno() not in (0, errno.ESRCH):
                error = ctypes.get_errno()
                raise OSError(error, os.strerror(error))
            if length < ctypes.sizeof(pids):
                break
            size *= 2
        total = 0
        for pid in pids[:max(0, length) // ctypes.sizeof(ctypes.c_int)]:
            if not pid:
                continue
            usage = ProcessUsage()
            if self.proc.proc_pid_rusage(pid, 0, ctypes.byref(usage)):
                error = ctypes.get_errno()
                if error in (errno.ESRCH, errno.ENOENT):
                    continue
                raise OSError(error, os.strerror(error))
            total += usage.physical_footprint
        return total


def signal_group(pgid, sig):
    try:
        os.killpg(pgid, sig)
    except ProcessLookupError:
        return False
    return True


def group_alive(pgid):
    try:
        return signal_group(pgid, 0)
    except PermissionError:
        # Darwin can report EPERM while the only remaining group members are
        # zombies. They have stopped executing and cannot keep the job alive.
        listing = subprocess.check_output(["/bin/ps", "-axo", "pgid=,stat="], text=True)
        return any(group == str(pgid) and not state.startswith("Z")
                   for group, state in (line.split()[:2] for line in listing.splitlines() if line.strip()))


def stop_group(process):
    try:
        signal_group(process.pid, signal.SIGKILL)
    except PermissionError:
        if group_alive(process.pid):
            raise
    process.wait(timeout=10)
    deadline = time.monotonic() + 10
    while group_alive(process.pid):
        if time.monotonic() >= deadline:
            raise RuntimeError(f"Process group {process.pid} is still alive")
        time.sleep(0.1)


class Queue:
    def __init__(self, root):
        self.root = root
        self.workspace = root / "workspace"
        self.build_lock = root / "lean-build.lock"
        self.root.mkdir(parents=True, exist_ok=True)

    @contextmanager
    def db(self):
        with closing(sqlite3.connect(self.root / "queue.sqlite3", timeout=15)) as connection:
            connection.row_factory = sqlite3.Row
            with connection:
                connection.execute("""CREATE TABLE IF NOT EXISTS jobs (
                    seq INTEGER PRIMARY KEY, id TEXT UNIQUE, request TEXT NOT NULL,
                    state TEXT NOT NULL DEFAULT 'queued', canceled INTEGER NOT NULL DEFAULT 0,
                    pgid INTEGER, code INTEGER, reason TEXT
                )""")
                yield connection

    def job_dir(self, job_id):
        return self.root / "jobs" / uuid.UUID(job_id).hex

    def row(self, job_id):
        with self.db() as db:
            row = db.execute("SELECT * FROM jobs WHERE id=?", (uuid.UUID(job_id).hex,)).fetchone()
            if row is None:
                raise ValueError(f"Unknown job {job_id}")
            return row

    def start_worker(self):
        with (self.root / "worker.log").open("ab") as log:
            subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--root", str(self.root), "work"],
                             stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)

    def submit(self, source):
        request = json.loads(source.readline(128 * 1024))
        kind, args = request["kind"], request["args"]
        budget = request["memory_mib"]
        if (not isinstance(args, list) or not all(isinstance(arg, str) for arg in args)
                or type(budget) is not int or not 1 <= budget <= MAX_MEMORY_MIB):
            raise ValueError("Invalid Lean job arguments or memory budget")
        if kind == "make":
            if len(args) != 1 or args[0] not in MAKE_TARGETS:
                raise ValueError("Only repository Lean Make targets can be submitted")
        elif kind != "lake":
            raise ValueError("Expected a Lake command or Lean Make target")
        for name in request.get("outputs", {}):
            if not returnable(relpath(name)):
                raise ValueError(f"Unsupported output: {name}")

        job_id = uuid.uuid4().hex
        directory = self.job_dir(job_id)
        directory.mkdir(parents=True)
        try:
            with (directory / "source.tar.gz").open("wb") as archive:
                shutil.copyfileobj(source, archive)
            digest = request.get("source_sha256")
            if digest and file_digest(directory / "source.tar.gz") != digest:
                raise ValueError("Studio snapshot did not match the submitted digest")
            (directory / "stdout.log").touch()
            (directory / "stderr.log").touch()
            with self.db() as db:
                db.execute("INSERT INTO jobs (id, request) VALUES (?, ?)", (job_id, json.dumps(request)))
        except BaseException:
            shutil.rmtree(directory)
            raise
        self.start_worker()
        print(json.dumps({"id": job_id}))

    def status(self, job_id):
        with self.db() as db:
            if job_id:
                rows = [dict(self.row(job_id))]
            else:
                rows = [dict(row) for row in db.execute(
                    "SELECT * FROM jobs WHERE state IN ('queued', 'running') ORDER BY seq"
                )]
            for row in rows:
                req = json.loads(row.pop("request"))
                row["thread"] = req.get("thread")
                row["base_commit"] = req.get("base_commit")
                row["memory_mib"] = req["memory_mib"]
                row["command"] = req["args"]
                if row["state"] == "queued":
                    row["ahead"] = db.execute(
                        "SELECT count(*) FROM jobs WHERE seq < ? AND state IN ('queued', 'running')", (row["seq"],)
                    ).fetchone()[0]
        print(json.dumps({"jobs": rows, "lock": resource_lock.owner(self.build_lock)}, indent=2))

    def follow(self, job_id):
        no_worker_since = None
        row = self.row(job_id)
        if row["state"] == "queued":
            print(f"Studio Lean job {row['id']} queued", file=sys.stderr, flush=True)
        directory = self.job_dir(job_id)
        stdout = directory / "stdout.log"
        stderr = directory / "stderr.log"
        with ExitStack() as streams:
            logs = [(streams.enter_context(file.open("rb")), output) for file, output in (
                (stdout, sys.stdout.buffer), (stderr, sys.stderr.buffer),
            ) if file.is_file()]
            if not logs:
                # Jobs completed by the previous worker kept one combined log.
                logs = [(streams.enter_context((directory / "build.log").open("rb")), sys.stdout.buffer)]
            while True:
                row = self.row(job_id)
                for log, output in logs:
                    shutil.copyfileobj(log, output)
                    output.flush()
                if row["state"] not in ACTIVE:
                    return row["code"]
                if self.is_running():
                    no_worker_since = None
                else:
                    no_worker_since = no_worker_since or time.monotonic()
                    if time.monotonic() - no_worker_since > 3:
                        if self.row(job_id)["state"] not in ACTIVE:
                            continue
                        holder = resource_lock.owner(self.build_lock)
                        detail = f"blocked by {holder}" if holder else "has no running worker"
                        print(f"Studio Lean {detail}. See {self.root / 'worker.log'}.", file=sys.stderr)
                        return 75
                time.sleep(0.5)

    def is_running(self):
        with (self.root / "worker.lock").open("a") as slot:
            try:
                fcntl.flock(slot, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return True
            return False

    def cancel(self, job_id):
        row = self.row(job_id)
        with self.db() as db:
            db.execute("""UPDATE jobs SET canceled=1,
                state=CASE WHEN state='queued' THEN 'canceled' ELSE state END,
                code=CASE WHEN state='queued' THEN 130 ELSE code END,
                reason=CASE WHEN state='queued' THEN 'canceled' ELSE reason END
                WHERE id=? AND state IN ('queued','running')""", (row["id"],))
        print(f"Cancellation requested for {row['id']}")

    def unlock(self):
        with (self.root / "worker.lock").open("a") as slot:
            fcntl.flock(slot, fcntl.LOCK_EX | fcntl.LOCK_NB)
            holder = resource_lock.owner(self.build_lock)
            if holder:
                row = self.row(json.loads(holder)["job"])
                if row["pgid"] and group_alive(row["pgid"]):
                    raise RuntimeError(f"Process group {row['pgid']} is alive. Stop the old build before clearing the lock.")
                if row["state"] in ACTIVE:
                    self.finish(row["id"], 1, "worker interrupted")
                resource_lock.release(self.build_lock, holder, force=True)
        print(f"Cleared {holder}" if holder else "Studio Lean lock is free")
        self.start_worker()

    def finish(self, job_id, code, reason):
        state = "canceled" if code == 130 else "failed" if code else "completed"
        with self.db() as db:
            db.execute("UPDATE jobs SET state=?, code=?, reason=?, pgid=NULL WHERE id=?",
                       (state, code, reason, job_id))

    def prepare(self, directory):
        with tempfile.TemporaryDirectory(dir=directory) as temp:
            staging = Path(temp)
            with tarfile.open(directory / "source.tar.gz", "r:gz") as archive:
                extract_files(archive, staging, source_path)
            for relative in ("lean-toolchain", "lakefile.lean", "scripts/lean.sh"):
                if not (staging / relative).is_file():
                    raise ValueError(f"Missing snapshot file: {relative}")
            self.workspace.mkdir(exist_ok=True)
            subprocess.run(["/usr/bin/rsync", "-ac", "--delete", "--exclude=/build/", "--exclude=/.lake/",
                            f"{staging}/", f"{self.workspace}/"], check=True)
            scratch = self.workspace / SCRATCH
            scratch.mkdir(parents=True, exist_ok=True)
            (staging / SCRATCH).mkdir(parents=True, exist_ok=True)
            subprocess.run(["/usr/bin/rsync", "-ac", "--delete", f"{staging / SCRATCH}/", f"{scratch}/"], check=True)
        for relative in (GENERATED, Path("build/lean/codegen-test")):
            if (self.workspace / relative).exists():
                shutil.rmtree(self.workspace / relative)

    def execute(self, job_id, gate):
        if gate.read(1) != b"1":
            return 75
        request = json.loads(self.row(job_id)["request"])
        self.prepare(self.job_dir(job_id))
        for name in request.get("return_paths", []):
            path = relpath(name)
            if not returnable(path):
                raise ValueError(f"Unsupported output: {path}")
            (self.workspace / path).parent.mkdir(parents=True, exist_ok=True)
        os.chdir(self.workspace)
        if request["kind"] == "make":
            return subprocess.call(["/usr/bin/make", *request["args"]])
        return run_lake(request["args"])

    def attempt(self, job_id, env, stdout, stderr, memory, budget):
        start = time.monotonic()
        peak = 0
        reason = "exit"
        process = subprocess.Popen([sys.executable, "-B", str(Path(__file__).resolve()),
                                    "--root", str(self.root), "execute", job_id],
                                   env=env, stdin=subprocess.PIPE, stdout=stdout, stderr=stderr,
                                   start_new_session=True)
        try:
            with self.db() as db:
                db.execute("UPDATE jobs SET pgid=? WHERE id=?", (process.pid, job_id))
            # A crash before the commit closes the pipe without granting permission
            # to touch the workspace. A crash after it leaves the PGID for recovery.
            with process.stdin as gate:
                gate.write(b"1")
            while process.poll() is None:
                peak = max(peak, memory.footprint(process.pid))
                if self.row(job_id)["canceled"]:
                    reason = "canceled"
                elif memory.pressure() >= 4:
                    reason = "system memory pressure"
                elif peak >= budget * MIB:
                    reason = "job memory budget"
                if reason != "exit":
                    break
                time.sleep(0.25)
        finally:
            process.stdin.close()
            if process.poll() is None or group_alive(process.pid):
                stop_group(process)
            with self.db() as db:
                db.execute("UPDATE jobs SET pgid=NULL WHERE id=?", (job_id,))
        code = process.returncode
        code = code if code >= 0 else 128 - code
        if reason == "canceled":
            code = 130
        elif reason != "exit":
            code = 1
        stderr.write(f"\nStudio Lean: {reason}, exit {code}, peak {peak / MIB:.1f} MiB, "
                     f"{time.monotonic() - start:.1f} seconds\n".encode())
        return code, reason

    def run(self, row, memory):
        job_id = row["id"]
        request = json.loads(row["request"])
        directory = self.job_dir(job_id)
        claimant = json.dumps({"job": job_id, "thread": request.get("thread")}, sort_keys=True)
        resource_lock.claim(self.build_lock, claimant)
        try:
            with self.db() as db:
                changed = db.execute("UPDATE jobs SET state='running' WHERE id=? AND state='queued'", (job_id,))
                if not changed.rowcount:
                    return
            with (directory / "stdout.log").open("ab", buffering=0) as stdout, \
                 (directory / "stderr.log").open("ab", buffering=0) as stderr:
                try:
                    env = {**os.environ, "SMARTGRID_LEAN_STUDIO_WORKER": "1", "LEAN_NUM_THREADS": "16",
                           "SMARTGRID_LEAN_STUDIO_SERVICE": str(Path(__file__).resolve()),
                           "GIT_TERMINAL_PROMPT": "0",
                           "ELAN_HOME": str(self.root.parent / "elan"),
                           "MATHLIB_CACHE_DIR": str(self.root.parent / "mathlib"),
                           "LAKE_CACHE_DIR": str(self.root.parent / "lake")}
                    budget = request["memory_mib"]
                    stderr.write(f"Studio Lean: starting job {job_id}, budget {budget} MiB\n".encode())
                    code, reason = self.attempt(job_id, env, stdout, stderr, memory, budget)
                    if reason == "job memory budget" and budget < MAX_MEMORY_MIB and memory.pressure() == 1:
                        stderr.write(f"Studio Lean: retrying once with {MAX_MEMORY_MIB} MiB\n".encode())
                        code, reason = self.attempt(job_id, env, stdout, stderr, memory, MAX_MEMORY_MIB)
                    if code == 0:
                        self.save_outputs(request, directory)
                except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, tarfile.TarError) as error:
                    stderr.write(f"Studio Lean: {error}\n".encode())
                    code, reason = 1, str(error)
            if self.row(job_id)["pgid"] is not None:
                raise RuntimeError(f"Job {job_id} may have a live process group. Leaving its lock for review.")
            self.finish(job_id, code, reason)
            (directory / "source.tar.gz").unlink(missing_ok=True)
        finally:
            if self.row(job_id)["pgid"] is None:
                resource_lock.release(self.build_lock, claimant)

    def save_outputs(self, request, directory):
        target = request["args"][0] if request["kind"] == "make" else ""
        paths = [relpath(name) for name in request.get("return_paths", [])]
        paths.extend(path.relative_to(self.workspace) for path in (self.workspace / GENERATED).glob("*.hpp"))
        if target in FORMAL_OUTPUTS:
            baseline = request.get("outputs", {})
            for file in (self.workspace / FORMAL_OUTPUTS[target]).glob("*.hpp"):
                path = file.relative_to(self.workspace)
                if file_digest(file) != baseline.get(path.as_posix()):
                    paths.append(path)
        with tarfile.open(directory / "outputs.tar.gz", "w:gz") as archive:
            for path in sorted(set(paths)):
                if not returnable(path):
                    raise ValueError(f"Unsupported output: {path}")
                file = self.workspace / path
                if file.is_file() and not file.is_symlink():
                    archive.add(file, arcname=path.as_posix(), recursive=False)

    def work(self):
        with (self.root / "worker.lock").open("a") as slot:
            fcntl.flock(slot, fcntl.LOCK_EX)
            memory = MacMemory()
            while not resource_lock.owner(self.build_lock):
                with self.db() as db:
                    row = db.execute("SELECT * FROM jobs WHERE state='queued' ORDER BY seq LIMIT 1").fetchone()
                if row is None:
                    return
                if memory.pressure() != 1:
                    time.sleep(1)
                    continue
                self.run(row, memory)


def returnable(path):
    return (path.is_relative_to(SCRATCH) or path.is_relative_to(GENERATED)
            or path.suffix == ".hpp" and path.parent in FORMAL_OUTPUTS.values())


def lake_command_index(args):
    # Lake owns validation. Find the command only to recognize the Lean wrapper
    # conveniences, leaving global options and their values in the original order.
    value_options = {"-d", "-f", "-K", "-o", "--dir", "--file", "--packages", "--service", "--scope",
                     "--repo", "--platform", "--toolchain", "--rev", "--max-revs", "--log-level",
                     "--fail-level", "--linters", "--lint-only", "--only"}
    skip_value = False
    for index, arg in enumerate(args):
        if skip_value:
            skip_value = False
        elif arg == "--":
            break
        elif arg.startswith("-"):
            skip_value = arg in value_options
        elif not (index == 0 and arg.startswith("+")):
            return index
    return len(args)


def run_lake(args):
    if args == ["setup"]:
        return subprocess.call([sys.executable, "-B", "scripts/build_cache.py", "lean-setup"])
    elan_home = Path(os.environ.get("ELAN_HOME") or ROOT.parent / "elan")
    tools = elan_home / "bin"
    if not os.access(tools / "lake", os.X_OK):
        raise RuntimeError("Run make lean-setup to install the shared Lean toolchain.")
    env = {**os.environ, "ELAN_HOME": str(elan_home), "PATH": f"{tools}{os.pathsep}{os.environ['PATH']}"}
    env.setdefault("LAKE_ARTIFACT_CACHE", "true")
    env.setdefault("LAKE_RESTORE_ARTIFACTS", "true")
    index = lake_command_index(args)
    lake = [str(tools / "lake"), *args[:index]]
    command = args[index:]
    if command[:1] == ["check"]:
        if len(command) != 2:
            raise ValueError("Usage: scripts/lean.sh check <file.lean>")
        command = ["lean", command[1], "--", "--threads=1", "-DwarningAsError=true", "-DautoImplicit=false"]
    elif command[:1] == ["lean"]:
        if "--" not in command:
            command = [*command, "--"]
        compiler_args = command.index("--") + 1
        command = [*command[:compiler_args], "--threads=1", *command[compiler_args:]]
    elif command[:2] == ["env", "lean"]:
        lean = command[2:]
        if "--run" in lean:
            run_index = lean.index("--run")
            if run_index + 1 < len(lean):
                # Lake prepares the imports from the current snapshot. Invoke Lean
                # separately because --run consumes the file and all following args.
                with tempfile.NamedTemporaryFile() as setup:
                    code = subprocess.call([*lake, "setup-file", lean[run_index + 1]], env=env, stdout=setup)
                    if code:
                        return code
                    return subprocess.call([*lake, "env", "lean", "--threads=1", "--setup", setup.name, *lean], env=env)
        files = [arg for arg in lean if arg.endswith(".lean") and not arg.startswith("-")]
        if len(files) == 1 and "--run" not in lean:
            lean.remove(files[0])
            command = ["lean", files[0], "--", "--threads=1", *lean]
        else:
            command = ["env", "lean", "--threads=1", *lean]
    return subprocess.call([*lake, *command], env=env)


def main():
    os.umask(0o077)
    if sys.argv[1:2] == ["lake-command"]:
        return run_lake(sys.argv[2:])
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("command", choices=("submit", "work", "execute", "follow", "status", "cancel", "fetch", "unlock"))
    parser.add_argument("job", nargs="?")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    queue = Queue(args.root)
    if args.command == "submit":
        queue.submit(sys.stdin.buffer)
    elif args.command == "work":
        queue.work()
    elif args.command == "execute":
        return queue.execute(args.job, sys.stdin.buffer)
    elif args.command == "follow":
        return queue.follow(args.job)
    elif args.command == "status":
        queue.status(args.job)
    elif args.command == "cancel":
        queue.cancel(args.job)
    elif args.command == "fetch":
        with (queue.job_dir(args.job) / "outputs.tar.gz").open("rb") as archive:
            shutil.copyfileobj(archive, sys.stdout.buffer)
    elif args.force:
        queue.unlock()
    else:
        parser.error("unlock requires --force after verifying the previous build is idle")
    return 0


if __name__ == "__main__":
    try:
        code = main()
        sys.exit(128 - code if code < 0 else code)
    except (OSError, ValueError, KeyError, RuntimeError, sqlite3.Error, subprocess.SubprocessError, tarfile.TarError) as error:
        sys.exit(f"Studio Lean: {error}")

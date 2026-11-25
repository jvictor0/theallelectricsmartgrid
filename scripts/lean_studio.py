#!/usr/bin/env python3
"""Submit the current worktree to the Studio's native Lean queue."""

import argparse
from contextlib import contextmanager
import difflib
import fcntl
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile

from lean_studio_worker import (DEFAULT_MEMORY_MIB, FORMAL_OUTPUTS, GENERATED, SCRATCH, STUDIO_PATH,
                                extract_files, file_digest, lake_command_index, relpath, returnable, source_path)


REPO = Path(__file__).resolve().parents[1]
RESULTS = Path("build/results/lean-studio")


def ssh(command):
    host = os.environ.get("SMARTGRID_LEAN_HOST", "studio")
    return ["ssh", "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10",
            "-o", "ServerAliveInterval=15", "-o", "ServerAliveCountMax=3", "--", host, command]


def remote(*args):
    return ssh(shlex.join(["/usr/bin/python3", str(STUDIO_PATH / "service/lean_studio_worker.py"), *args]))


def git(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args])


def normalize(arg):
    prefix = str(REPO) + "/"
    if arg.startswith(prefix):
        return arg[len(prefix):]
    name, sep, value = arg.partition("=")
    if sep and value.startswith(prefix):
        return name + "=" + value[len(prefix):]
    return arg


def inputs_and_outputs(args):
    inputs = set()
    outputs = set()
    command = args[lake_command_index(args):]
    if command[:2] == ["env", "lean"] and "--run" in command:
        # Everything after the script file belongs to its main, including -o.
        output_end = len(args) - len(command) + command.index("--run")
    else:
        output_end = len(args)
    value_for_output = False
    for index, arg in enumerate(args):
        if value_for_output:
            outputs.add(relpath(arg))
            value_for_output = False
        elif index < output_end and arg in ("-o", "-i", "-c", "-b", "--o", "--i", "--c", "--bc"):
            value_for_output = True
        elif index < output_end and arg.startswith(("--o=", "--i=", "--c=", "--bc=")):
            outputs.add(relpath(arg.split("=", 1)[1]))
        elif arg.startswith(str(SCRATCH) + "/") and (REPO / arg).is_file():
            inputs.add(relpath(arg))
    if value_for_output:
        raise ValueError("Missing Lean output path")
    for arg in shlex.split(os.environ.get("SMARTGRID_LEAN_INPUTS", "")):
        path = relpath(normalize(arg))
        if not path.is_relative_to(SCRATCH) or not (REPO / path).is_file():
            raise ValueError(f"Extra inputs must be files under {SCRATCH}: {arg}")
        inputs.add(path)
    if any(not returnable(path) for path in outputs):
        raise ValueError(f"Direct Lean outputs must be under {SCRATCH} or {GENERATED}")
    return inputs, outputs


def snapshot(request, extra_inputs, destination):
    files = {relpath(os.fsdecode(name)) for name in git(
        "ls-files", "--cached", "--others", "--exclude-standard", "-z"
    ).split(b"\0") if name}
    with tarfile.open(destination, "w:gz") as archive:
        for path in sorted(files | extra_inputs):
            if not source_path(path):
                continue
            file = REPO / path
            if file.is_symlink():
                raise ValueError(f"Cannot submit a symlink: {path}")
            if file.is_file():
                archive.add(file, arcname=path.as_posix(), recursive=False)
    request["base_commit"] = git("rev-parse", "HEAD").decode().strip()
    request["source_sha256"] = file_digest(destination)


@contextmanager
def lock_outputs():
    directory = REPO / RESULTS
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / "outputs.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def submit(kind, args, memory_mib, detach):
    args = [normalize(arg) for arg in args]
    inputs, outputs = inputs_and_outputs(args) if kind == "lake" else (set(), set())
    request = {
        "kind": kind, "args": args, "memory_mib": memory_mib, "thread": os.environ.get("CODEX_THREAD_ID"),
        "worktree": str(REPO), "return_paths": sorted(p.as_posix() for p in outputs),
    }
    (REPO / RESULTS).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=REPO / RESULTS) as temp:
        archive = Path(temp) / "source.tar.gz"
        with lock_outputs():
            before = outputs | {p.relative_to(REPO) for p in (REPO / GENERATED).glob("*.hpp")}
            if kind == "make" and args[0] in FORMAL_OUTPUTS:
                before.update(p.relative_to(REPO) for p in (REPO / FORMAL_OUTPUTS[args[0]]).glob("*.hpp"))
            request["outputs"] = {p.as_posix(): file_digest(REPO / p) for p in sorted(before)}
            snapshot(request, inputs, archive)
        stream = Path(temp) / "submission"
        with stream.open("wb") as wire, archive.open("rb") as source:
            wire.write(json.dumps(request).encode() + b"\n")
            shutil.copyfileobj(source, wire)
        with stream.open("rb") as wire:
            receipt = subprocess.check_output(remote("submit"), stdin=wire)
    job = json.loads(receipt)["id"]
    directory = REPO / RESULTS / job
    directory.mkdir()
    (directory / "request.json").write_text(json.dumps(request), encoding="utf-8")
    print(f"Studio Lean job {job} submitted", file=sys.stderr, flush=True)
    return 0 if detach else wait(job)


def receive_outputs(job, directory):
    with lock_outputs():
        apply_outputs(job, directory)


def apply_outputs(job, directory):
    output = directory / "outputs"
    request = json.loads((directory / "request.json").read_text(encoding="utf-8"))
    if not output.exists():
        with tempfile.TemporaryDirectory(dir=directory) as temp:
            staged = Path(temp) / "files"
            staged.mkdir()
            archive = Path(temp) / "outputs.tar.gz"
            with archive.open("wb") as stream:
                subprocess.run(remote("fetch", job), stdin=subprocess.DEVNULL, stdout=stream, check=True)
            with tarfile.open(archive, "r:gz") as stream:
                extract_files(stream, staged, returnable)
            staged.rename(output)
    paths = [file.relative_to(output) for file in output.rglob("*") if file.is_file()]
    conflicts = []
    for path in paths:
        current = file_digest(REPO / path)
        baseline = request["outputs"].get(path.as_posix())
        if current not in (baseline, file_digest(output / path)):
            conflicts.append(str(path))
    if conflicts:
        patch = directory / "generated.patch"
        with patch.open("w", encoding="utf-8") as stream:
            for path in paths:
                if path.suffix != ".hpp":
                    continue
                old = (REPO / path).read_text(encoding="utf-8").splitlines(keepends=True) if (REPO / path).is_file() else []
                new = (output / path).read_text(encoding="utf-8").splitlines(keepends=True)
                stream.writelines(difflib.unified_diff(old, new, f"a/{path}", f"b/{path}"))
        raise ValueError(f"Local outputs changed: {', '.join(conflicts)}. Review {patch} and {output}.")
    for path in paths:
        target = REPO / path
        target.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as temporary:
            pending = Path(temporary.name)
        try:
            shutil.copyfile(output / path, pending)
            pending.chmod((output / path).stat().st_mode & 0o777)
            pending.replace(target)
        finally:
            pending.unlink(missing_ok=True)


def wait(job):
    if len(job) != 32 or any(c not in "0123456789abcdef" for c in job):
        raise ValueError("Expected the Studio job ID")
    code = subprocess.call(remote("follow", job), stdin=subprocess.DEVNULL)
    directory = REPO / RESULTS / job
    if code == 0 and (directory / "request.json").is_file():
        receive_outputs(job, directory)
    return code


def install():
    path = shlex.quote(str(STUDIO_PATH / "service"))
    # Publish complete files so an SSH request never opens a partial script.
    command = (f"umask 077; mkdir -p {path} && stage=$(mktemp -d {path}/.install.XXXXXX) && "
               "trap 'rm -rf \"$stage\"' EXIT && tar -xf - -C \"$stage\" && "
               f"mv \"$stage/resource_lock.py\" {path}/ && mv \"$stage/lean_studio_worker.py\" {path}/")
    with tempfile.TemporaryFile() as package:
        with tarfile.open(fileobj=package, mode="w") as archive:
            for name in ("lean_studio_worker.py", "resource_lock.py"):
                archive.add(REPO / "scripts" / name, arcname=name)
        package.seek(0)
        subprocess.run(ssh(command), stdin=package, check=True)
    print("Installed the native Studio Lean worker")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--memory-mib", type=int,
                        default=os.environ.get("SMARTGRID_LEAN_MEMORY_MIB", DEFAULT_MEMORY_MIB))
    parser.add_argument("--detach", action="store_true")
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ("lake", "make"):
        subparsers.add_parser(name, add_help=False).add_argument("args", nargs=argparse.REMAINDER)
    for name in ("wait", "cancel"):
        subparsers.add_parser(name).add_argument("job")
    subparsers.add_parser("status").add_argument("job", nargs="?")
    subparsers.add_parser("unlock").add_argument("--force", action="store_true", required=True)
    subparsers.add_parser("install")
    args = parser.parse_args()
    if args.command in ("lake", "make"):
        words = args.args[1:] if args.args[:1] == ["--"] else args.args
        if not words and args.command == "make":
            parser.error("Specify a Lean command or Make target")
        return submit(args.command, words, args.memory_mib, args.detach)
    if args.command == "install":
        install()
    elif args.command == "wait":
        return wait(args.job)
    elif args.command == "unlock":
        return subprocess.call(remote("unlock", "--force"), stdin=subprocess.DEVNULL)
    else:
        return subprocess.call(remote(args.command, *([args.job] if args.job else [])), stdin=subprocess.DEVNULL)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit("Stopped waiting. The Studio job can be reattached or canceled by job ID.")
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, tarfile.TarError) as error:
        sys.exit(f"Studio Lean: {error}")

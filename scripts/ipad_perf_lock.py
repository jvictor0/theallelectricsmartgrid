"""Persistent ownership of the shared iPad across benchmark launches."""

from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import sys
import time


LOCK_FILE = Path.home() / ".cache/theallelectricsmartgrid/ipad-perf.lock"
LOCK_COMMAND = "python3 scripts/ipad_perf.py --lock"
UNLOCK_COMMAND = "python3 scripts/ipad_perf.py --unlock"


def thread_id():
    owner = os.environ.get("CODEX_THREAD_ID", "").strip()
    if not owner:
        raise ValueError("CODEX_THREAD_ID is required to claim, release, or use the iPad perf lock")
    return owner


@contextmanager
def edit_lock():
    LOCK_FILE.parent.mkdir(parents=True, exist_ok=True)
    # Keep the inode so all callers synchronize on the same file. The owner
    # text persists between commands.
    with LOCK_FILE.open("a+", encoding="utf-8") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        lock.seek(0)
        yield lock, lock.read().strip()


def lock_owner():
    try:
        with LOCK_FILE.open(encoding="utf-8") as lock:
            fcntl.flock(lock, fcntl.LOCK_SH)
            return lock.read().strip()
    except FileNotFoundError:
        return ""


def acquire_lock(owner, wait):
    deadline = time.monotonic() + wait
    previous_holder = None
    while True:
        with edit_lock() as (lock, holder):
            if not holder:
                lock.write(f"{owner}\n")
            if not holder or holder == owner:
                return

        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise ValueError(f"iPad perf lock is held by Codex thread {holder}")
        if holder != previous_holder:
            print(f"Waiting for the iPad perf lock held by Codex thread {holder}",
                  file=sys.stderr, flush=True)
            previous_holder = holder
        time.sleep(min(1, remaining))


def release_lock(owner, *, force=False):
    with edit_lock() as (lock, holder):
        if holder and holder != owner and not force:
            raise ValueError(f"iPad perf lock is held by Codex thread {holder}")
        lock.truncate(0)
        return holder


def require_lock(owner):
    holder = lock_owner()
    if holder == owner:
        return
    if holder:
        status = f"iPad perf lock is held by Codex thread {holder}."
    else:
        status = "iPad perf lock is free."
    raise ValueError(f"{status} Acquire it once before the first case with `{LOCK_COMMAND} --wait 600`. "
                     f"Keep it for the entire battery, then run `{UNLOCK_COMMAND}` after the last case finishes.")


def remind_at_stop(event):
    holder = lock_owner()
    if not holder or holder != event["session_id"]:
        return
    reason = ("This Codex thread still holds the iPad perf lock. Wait for any active benchmark "
              f"to finish, then run `{UNLOCK_COMMAND}` from the repo root before ending the turn.")
    if event.get("stop_hook_active"):
        print(json.dumps({"systemMessage": reason}))
    else:
        print(json.dumps({"decision": "block", "reason": reason}))

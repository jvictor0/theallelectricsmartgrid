"""Persistent ownership of the shared iPad across benchmark launches."""

import json
import os
from pathlib import Path
import sys
import time

import resource_lock


LOCK_FILE = Path.home() / ".cache/theallelectricsmartgrid/ipad-perf.lock"
LOCK_COMMAND = "python3 scripts/ipad_perf.py --lock"
UNLOCK_COMMAND = "python3 scripts/ipad_perf.py --unlock"


def thread_id():
    owner = os.environ.get("CODEX_THREAD_ID", "").strip()
    if not owner:
        raise ValueError("CODEX_THREAD_ID is required to claim, release, or use the iPad perf lock")
    return owner


def lock_owner():
    return resource_lock.owner(LOCK_FILE)


def acquire_lock(owner, wait):
    deadline = time.monotonic() + wait
    previous_holder = None
    while True:
        try:
            resource_lock.claim(LOCK_FILE, owner)
            return
        except resource_lock.LockHeld as error:
            holder = error.holder

        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise ValueError(f"iPad perf lock is held by Codex thread {holder}")
        if holder != previous_holder:
            print(f"Waiting for the iPad perf lock held by Codex thread {holder}",
                  file=sys.stderr, flush=True)
            previous_holder = holder
        time.sleep(min(1, remaining))


def release_lock(owner, *, force=False):
    try:
        return resource_lock.release(LOCK_FILE, owner, force=force)
    except resource_lock.LockHeld as error:
        raise ValueError(f"iPad perf lock is held by Codex thread {error.holder}") from error


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

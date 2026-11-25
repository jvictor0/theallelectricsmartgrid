"""A persistent owner record with serialized updates."""

from contextlib import contextmanager
import fcntl


class LockHeld(ValueError):
    def __init__(self, holder):
        self.holder = holder
        super().__init__(holder)


@contextmanager
def edit(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    # Keep the inode so all callers synchronize on the same file.
    with path.open("a+", encoding="utf-8") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        lock.seek(0)
        yield lock, lock.read().strip()


def owner(path):
    try:
        with path.open(encoding="utf-8") as lock:
            fcntl.flock(lock, fcntl.LOCK_SH)
            return lock.read().strip()
    except FileNotFoundError:
        return ""


def claim(path, claimant):
    with edit(path) as (lock, holder):
        if holder and holder != claimant:
            raise LockHeld(holder)
        if not holder:
            lock.write(f"{claimant}\n")


def release(path, claimant, *, force=False):
    with edit(path) as (lock, holder):
        if holder and holder != claimant and not force:
            raise LockHeld(holder)
        lock.truncate(0)
        return holder

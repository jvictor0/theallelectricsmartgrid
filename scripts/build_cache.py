"""Resolve and populate SmartGrid's shared build cache."""

from contextlib import contextmanager
import fcntl
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory


REPO_ROOT = Path(__file__).resolve().parents[1]
JUCE_PATH = Path("third_party/JUCE")
JUCE_HEADER = Path("modules/juce_core/juce_core.h")


def cache_root():
    user_cache = Path(os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache")
    return Path(os.environ.get("SMARTGRID_BUILD_CACHE_DIR") or user_cache / "theallelectricsmartgrid").resolve()


@contextmanager
def cache_lock(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def git_output(*args):
    return subprocess.check_output(["git", "-C", REPO_ROOT, *args], text=True).strip()


def juce_source():
    local = REPO_ROOT / JUCE_PATH
    if (local / JUCE_HEADER).is_file():
        return local

    revision = git_output("rev-parse", "--verify", f":{JUCE_PATH}")
    source = cache_root() / "juce" / revision
    if (source / JUCE_HEADER).is_file():
        return source

    with cache_lock(source.with_suffix(".lock")):
        if (source / JUCE_HEADER).is_file():
            return source

        # A submodule initialized in the main checkout already has a shared Git object store.
        common_git = Path(git_output("rev-parse", "--path-format=absolute", "--git-common-dir"))
        objects = common_git / "modules/JUCE8"
        with TemporaryDirectory(dir=source.parent) as temp:
            scratch = Path(temp)
            if subprocess.run(
                ["git", "--git-dir", objects, "cat-file", "-e", f"{revision}^{{commit}}"],
                stderr=subprocess.DEVNULL,
            ).returncode:
                objects = scratch / "git"
                url = git_output("config", "--file", ".gitmodules", "--get", "submodule.JUCE8.url")
                subprocess.run(["git", "init", "--quiet", "--bare", objects], check=True)
                subprocess.run(
                    ["git", "--git-dir", objects, "fetch", "--quiet", "--depth=1", "--no-tags", url, revision],
                    check=True,
                )

            archive = scratch / "juce.tar"
            unpacked = scratch / "source"
            unpacked.mkdir()
            subprocess.run(["git", "--git-dir", objects, "archive", "-o", archive, revision], check=True)
            subprocess.run(["tar", "-xf", archive, "-C", unpacked], check=True)
            if not (unpacked / JUCE_HEADER).is_file():
                raise RuntimeError(f"Pinned JUCE revision {revision} is missing {JUCE_HEADER}")
            unpacked.rename(source)

    return source


def lean_setup():
    elan_home = Path(os.environ.get("ELAN_HOME") or cache_root() / "elan").resolve()
    os.environ["ELAN_HOME"] = str(elan_home)
    elan = elan_home / "bin/elan"
    lake = elan_home / "bin/lake"
    toolchain = (REPO_ROOT / "lean-toolchain").read_text().strip()
    with cache_lock(elan_home / "smartgrid-setup.lock"):
        if not os.access(elan, os.X_OK) or not os.access(lake, os.X_OK):
            with TemporaryDirectory(dir=elan_home) as temp:
                installer = Path(temp) / "elan-init.sh"
                subprocess.run(
                    ["curl", "--fail", "--show-error", "--location",
                     "https://raw.githubusercontent.com/leanprover/elan/master/elan-init.sh",
                     "--output", installer],
                    check=True,
                )
                subprocess.run(["sh", installer, "-y", "--no-modify-path", "--default-toolchain", "none"], check=True)
        subprocess.run([elan, "toolchain", "install", toolchain], check=True)
    subprocess.run([lake, "--version"], cwd=REPO_ROOT, check=True)


if __name__ == "__main__":
    try:
        args = sys.argv[1:]
        if args == ["root"]:
            print(cache_root())
        elif args == ["juce"]:
            print(juce_source())
        elif args == ["lean-setup"]:
            lean_setup()
        else:
            sys.exit("Usage: build_cache.py {root|juce|lean-setup}")
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(f"Build cache: {error}")

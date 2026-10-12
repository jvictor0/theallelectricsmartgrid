#!/usr/bin/env python3
"""Remove only disposable canonical build trees in this script's checkout."""

import shutil
import sys
from pathlib import Path


BUILD_TREES = (
    "build/core",
    "build/visualizers",
    "build/midi",
    "build/xcode/macos",
    "build/xcode/ios",
    "build/clang-tidy",
    "build/tools",
)


def clean_builds(repo_root):
    repo_root = repo_root.resolve(strict=True)
    targets = [repo_root / name for name in BUILD_TREES]
    for target in targets:
        for path in (target, *target.parents):
            if path == repo_root:
                break
            if path.is_symlink():
                raise ValueError(f"Refusing to clean through symlink: {path}")
        if target.exists() and not target.is_dir():
            raise ValueError(f"Expected a build directory: {target}")
    for target in targets:
        if target.exists():
            print(f"Removing {target}")
            shutil.rmtree(target)


if __name__ == "__main__":
    try:
        clean_builds(Path(__file__).resolve().parents[1])
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)

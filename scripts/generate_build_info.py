"""Generate the compile-time Git commit header from a repository checkout."""

import argparse
from pathlib import Path
import subprocess


def Generate(repository, output):
    sha = subprocess.check_output(
        ["git", "-C", str(repository), "rev-parse", "--verify", "HEAD^{commit}"],
        text=True,
    ).strip()
    content = (
        "#pragma once\n\n"
        "namespace SmartGridBuildInfo\n"
        "{\n"
        f'    inline constexpr char x_gitCommitSha[] = "{sha}";\n'
        "}\n\n"
    )
    if not output.exists() or output.read_text() != content:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    Generate(args.repository, args.output)

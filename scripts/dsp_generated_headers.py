#!/usr/bin/env python3
"""Copy or check the production headers emitted by GenerateDSP.lean."""

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def synchronize(generated: Path, snapshots: Path, *, copy: bool) -> None:
    headers = sorted(generated.glob("*.hpp"))
    if not headers:
        raise SystemExit(f"No DSP headers emitted in {generated}")

    if copy:
        snapshots.mkdir(parents=True, exist_ok=True)

    issues = []
    for header in headers:
        snapshot = snapshots / header.name
        code = header.read_bytes()
        previous = snapshot.read_bytes() if snapshot.is_file() else None
        if previous == code:
            continue
        if copy:
            snapshot.write_bytes(code)
        else:
            status = "Missing" if previous is None else "Stale"
            issues.append(f"{status} DSP header {snapshot.name}")

    obsolete = {header.name for header in snapshots.glob("*.hpp")} - {header.name for header in headers}
    issues.extend(f"DSP header no longer emitted: {name}" for name in sorted(obsolete))
    if issues:
        advice = "Run make lean-dsp-generate to refresh outputs and remove obsolete checked-in headers."
        raise SystemExit("\n".join([*issues, advice]))

    action = "Synced" if copy else "Checked"
    print(f"{action} {len(headers)} generated DSP headers.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("copy", "check"))
    args = parser.parse_args()
    synchronize(ROOT / "build/lean/generated", ROOT / "formal/DSP/generated", copy=args.action == "copy")


if __name__ == "__main__":
    main()

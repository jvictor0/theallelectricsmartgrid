#!/usr/bin/env python3
"""Compare the lowered resynthesizer with the Lean spectral grain step."""

import math
import os
from pathlib import Path
import shlex
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/lean"


def unpack(bits, precision):
    integer, floating = ("I", "f") if precision == 32 else ("Q", "d")
    return struct.unpack(floating, struct.pack(integer, int(bits)))[0]


def main():
    runner = BUILD / "resynthesis-runner"
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    subprocess.run(compiler + [
        "-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-I", str(BUILD / "generated"), "-I", str(ROOT / "private/src"),
        str(ROOT / "formal/DSP/Tests/codegen_resynthesis_runner.cpp"), "-o", str(runner),
    ], check=True)
    expected = subprocess.run([
        str(ROOT / "scripts/lean.sh"), "env", "lean", "-DautoImplicit=false",
        "-DwarningAsError=true", "--run", str(ROOT / "formal/DSP/Tests/ResynthesisReference.lean"),
    ], check=True, capture_output=True, text=True, timeout=60).stdout.splitlines()
    actual = subprocess.run([str(runner)], check=True, capture_output=True, text=True, timeout=60).stdout.splitlines()
    assert len(actual) == len(expected) == 40
    for found, wanted in zip(actual, expected):
        found, wanted = found.split(), wanted.split()
        assert len(found) == len(wanted) == 9 + 2 * 128
        assert found[:5] == wanted[:5], (found[:5], wanted[:5])
        for i in range(5, len(wanted)):
            precision = 64 if 6 <= i <= 8 else 32
            observed, reference = unpack(found[i], precision), unpack(wanted[i], precision)
            tolerance = 1e-12 if precision == 64 else 2e-6
            assert math.isclose(observed, reference, rel_tol=tolerance, abs_tol=tolerance), (
                wanted[:5], i, observed, reference,
            )
    print("Resynthesis: 40 channel-grains match Lean phase, gate, draw count, slew and spectrum checks.")


if __name__ == "__main__":
    main()

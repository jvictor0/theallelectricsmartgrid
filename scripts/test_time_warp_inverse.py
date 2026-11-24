#!/usr/bin/env python3
"""Replay identical inverse traces through native Lean and both C++ implementations."""

import os
from pathlib import Path
import random
import shlex
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/lean"


def bits(value):
    return struct.unpack("<Q", struct.pack("<d", value))[0]


def trace(capacity, resolution, origin):
    random_steps = random.Random(20261123)
    phases = [-0.375, -0.125, 0, 1, 0.25, 0.75, 1, 0.5, 0.9, 1, 1.125, 0.625,
              0.875, 12, 12, 4.5, 4.25, 4.75, 4.5, 5, 40.5, -2.25, -1.75, 50.25]
    for _ in range(384):
        phases.append(phases[-1] + random_steps.randrange(-128, 161) / 8)

    yield "R", 0.0, origin
    for sample, phase in enumerate(phases):
        time = origin + sample
        yield "W", phase / resolution, time
        for query in (phase, phase - 0.125, phase + 0.125, phase - capacity,
                      int(phase) - 0.5, int(phase), int(phase) + 0.5,
                      phase - random_steps.randrange(capacity)):
            for cutoff in (time, time - 0.5, time - 10, float("inf")):
                yield "R", query / resolution, cutoff


def main():
    output = BUILD / "codegen-test"
    output.mkdir(parents=True, exist_ok=True)
    native = output / "time_warp_inverse_reference"
    compiler = shlex.split(os.environ.get("CXX", "clang++"))
    subprocess.run([*compiler, "-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
                    "-I", str(ROOT / "private/src"),
                    str(ROOT / "private/test/perf/time_warp_codegen_reference.cpp"),
                    "-o", str(native)], check=True)
    lean = BUILD / "artifacts/bin/time_warp_inverse_reference"
    checked = 0
    for capacity, resolution, origin in ((1, 1, 0), (4, 1, 1e12), (37, 16, 1e12), (1024, 2**18, 0)):
        rows = list(trace(capacity, resolution, origin))
        data = "".join(f"{operation} {bits(a)} {bits(b)}\n" for operation, a, b in rows)
        args = [str(capacity), str(bits(resolution))]
        expected = subprocess.run([str(lean), *args], input=data, text=True, check=True,
                                  stdout=subprocess.PIPE, timeout=60).stdout.splitlines()
        actual = subprocess.run([str(native), *args], input=data, text=True, check=True,
                                stdout=subprocess.PIPE, timeout=60).stdout.splitlines()
        queries = [row for row in rows if row[0] == "R"]
        if len(expected) != len(queries) or len(actual) != len(queries):
            raise AssertionError(f"Unexpected result count at capacity {capacity}")
        for query, lean_time, cpp_time in zip(queries, expected, actual):
            if lean_time != cpp_time:
                raise AssertionError(f"Capacity {capacity}, query {query}: Lean {lean_time}, C++ {cpp_time}")
        checked += len(queries)
    print(f"Time-warp inverse: {checked:,} reads matched native Lean and the C++ reference exactly.")


if __name__ == "__main__":
    main()

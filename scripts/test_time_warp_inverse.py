#!/usr/bin/env python3
"""Replay identical inverse traces through native Lean and both C++ implementations."""

import os
from pathlib import Path
import random
import re
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


def boundary_trace(limit, resolution, direction):
    for time, offset in enumerate((53.5, 45.5, 49.5, 32, 39.5, 38.5, 1, 0, 7.5, 4.5, 0)):
        phase = direction * (limit - offset)
        yield "W", phase / resolution, float(time)
        for query in (phase, phase - 0.5, phase + 0.5, phase - 8, phase - 37):
            if -limit <= query <= limit:
                yield "R", query / resolution, float(time)


def timestamp_boundary_trace():
    limit = 2.0 ** 64
    observations = ((0.0, -limit), (1.5, -limit / 2), (0.25, 0.0),
                    (0.75, limit / 4), (1.25, limit / 2), (0.375, 3 * limit / 4),
                    (2.25, limit))
    for phase, time in observations:
        yield "W", phase, time
        for query in (0.0, 0.25, 0.375, 0.5, 0.75, 1.0, 1.25, 2.0):
            for cutoff in (time, float("inf")):
                yield "R", query, cutoff


def main():
    for name in ("TimeWarpInverseRead.hpp", "TimeWarpInverseWrite.hpp"):
        code = (ROOT / "formal/PhaseTime/generated" / name).read_text()
        checked_index = re.search(
            r"CodegenInteger::(?:(?:Add|Subtract|FromFloat|ToIndex)\b|FromIndex\((?![0-9]+\)))", code
        )
        if checked_index:
            raise AssertionError(f"{name} emitted a checked index operation: {checked_index[0]}")
        if name == "TimeWarpInverseRead.hpp" and "std::isfinite(" in code:
            raise AssertionError(f"{name} emitted a numerical finiteness check")
        if name == "TimeWarpInverseWrite.hpp" and (
            "Process(State& state, double input_m_phase, double input_m_time)" not in code
            or "state.LoadPrevious()" not in code
            or "state.GridResolution()" not in code
        ):
            raise AssertionError(f"{name} did not pass the observation separately from writer state")
        if name == "TimeWarpInverseWrite.hpp":
            scratch = re.findall(r"std::array<double, 8> (\w+);", code)
            if len(scratch) != 1:
                raise AssertionError("Scatter must allocate only one timestamp scratch array")
            array = scratch[0]
            writes = re.findall(rf"\b{array}\[[^\]\n]+\]\s*=", code)
            if len(writes) != 1 or re.search(rf"=\s*{array}\s*;", code):
                raise AssertionError("Scatter scratch must be written once per active index without a copy")

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
    scenarios = [(capacity, resolution, trace(capacity, resolution, origin))
                 for capacity, resolution, origin in ((1, 1, 0), (4, 1, 1e12),
                                                     (37, 16, 1e12), (1024, 2**18, 0))]
    scenarios.extend((37, 2**18, boundary_trace(2**52, 2**18, direction)) for direction in (-1, 1))
    scenarios.append((37, 1, timestamp_boundary_trace()))
    for capacity, resolution, events in scenarios:
        rows = list(events)
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

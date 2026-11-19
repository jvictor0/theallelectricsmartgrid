#!/usr/bin/env python3
"""Check generated delay storage and kernels against executable Lean."""

import math
import cmath
import os
from pathlib import Path
import shlex
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/lean"
HEADERS = ("CircularBufferGeneric", "SchroederAllPassCoreGeneric", "TunedDecayCombCoreGeneric",
           "TunedDecayCombTuningGeneric", "TunedDecayCombResponseGeneric")


def number(bits, precision):
    return struct.unpack("<f" if precision == 32 else "<d",
                         int(bits).to_bytes(precision // 8, "little"))[0]


def bits(value):
    return str(int.from_bytes(struct.pack("<d", value), "little"))


def main():
    output = BUILD / "codegen-test"
    output.mkdir(parents=True, exist_ok=True)
    for name in HEADERS:
        assert (BUILD / "generated" / (name + ".hpp")).read_bytes() == (
            ROOT / "formal/DSP/generated" / (name + ".hpp")).read_bytes(), f"Stale {name}"
    reference = subprocess.run([str(BUILD / "artifacts/bin/dsp_delay_codegen_reference")],
                               check=True, capture_output=True, text=True, timeout=30).stdout
    (output / "delay-reference.txt").write_text(reference)
    rows = [line.split() for line in reference.splitlines()]
    inputs, expected, labels = [], [], []
    counts = {"comb": 17, "allpass": 5, "tuning": 5, "response": 28}
    for precision in (32, 64):
        for width in (2, 4):
            for kind, count in counts.items():
                cases = [r[2:] for r in rows if r[:2] == [kind, str(precision)]]
                assert cases
                for start in range(0, len(cases), width):
                    lanes = cases[start:start + width]
                    lanes += [lanes[-1]] * (width - len(lanes))
                    inputs.append(f"{kind} {precision} {width} " + " ".join(v for r in lanes for v in r[:count]))
                    expected.append([v for r in lanes for v in r[count:]])
                    labels.append((kind, precision, width, start))
    for width in (2, 4):
        positions = {}
        for row in rows:
            if row[0] == "address":
                _, _, capacity, head, age, found, next_head = row
                inputs.append(f"address 64 {width} {capacity} {head} {age}")
                expected.append([bits(number(found, 64) + lane) for lane in range(width)] + [next_head])
                labels.append(("address", 64, width, (capacity, head, age)))
                continue
            if row[0] != "ring":
                continue
            _, _, capacity, age, value, found, head = row
            n = positions.get(capacity, 0)
            positions[capacity] = n + 1
            inputs.append(f"ring 64 {width} {capacity} {age} {value}")
            expected.append([bits(number(found, 64) + lane) if n > int(age) else bits(0)
                             for lane in range(width)] + [head])
            labels.append(("ring", 64, width, (capacity, n, age)))
    runner = output / "delay-runner"
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    subprocess.run(compiler + ["-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
                              "-Wall", "-Wextra", "-Werror", "-I", str(BUILD / "generated"),
                              "-I", str(ROOT / "private/src"),
                              str(ROOT / "formal/DSP/Tests/codegen_delay_runner.cpp"), "-o", str(runner)], check=True)
    actual = subprocess.run([str(runner)], input="\n".join(inputs) + "\n", check=True,
                            capture_output=True, text=True, timeout=60).stdout.splitlines()
    assert len(actual) == len(expected)
    errors = {}
    for label, line, wanted, source in zip(labels, actual, expected, inputs):
        kind, precision, width, _ = label
        found = line.split()
        assert len(found) == len(wanted), label
        if kind in ("comb", "allpass", "ring", "address"):
            assert found == wanted, f"{label}\nC++: {found}\nLean: {wanted}"
        else:
            for i, (a, b) in enumerate(zip(found, wanted)):
                a, b = number(a, precision), number(b, precision)
                assert math.isfinite(a) and math.isfinite(b), (label, i, a, b)
                error = abs(a - b) / (1 + abs(b))
                errors[kind, precision] = max(error, errors.get((kind, precision), 0))
                tolerance = 3e-5 if precision == 32 else 3e-6
                if kind == "tuning" and i % 11 in (1, 2):
                    # Subtracting the integer delay amplifies phase error by
                    # 1/f. Validate the assembled fundamental below as well.
                    frequency = number(source.split()[3 + (i // 11) * 5], precision)
                    tolerance = max(tolerance, 4e-8 / frequency)
                assert error < tolerance, (label, i, a, b, error)
            if kind == "tuning":
                for lane in range(width):
                    c = [number(v, precision) for v in found[lane * 11:(lane + 1) * 11]]
                    delay, a, b, r, delay_gain, g, k, h, gain, sign, bypass = c
                    assert delay.is_integer() and 1 <= delay <= 16384
                    assert abs(a) < 1 and 0 <= r <= 1 and 0 <= delay_gain <= 1
                    assert abs(a * a + b * b - 1) < 2e-6
                    assert abs(h * (1 + k * g + g * g) - 1) < 2e-6
                    assert g > 0 and k > 0 and gain > 0 and bypass == (sign == 0)
                    frequency = number(source.split()[3 + lane * 5], precision)
                    if sign:
                        q = cmath.exp(-2j * math.pi * frequency)
                        lowpass = gain * g * g * (1 + q) ** 2 / (
                            (1 + k * g + g * g) + 2 * (g * g - 1) * q + (1 - k * g + g * g) * q * q)
                        loop = sign * (a + q) / (1 + a * q) * lowpass * q ** int(delay)
                        assert abs(abs(lowpass) - 1) < 3e-6, (label, lane, lowpass)
                        assert abs(loop - 1) < 3e-6, (label, lane, loop)
    print(f"Delay codegen: {len(actual)} stereo/quad cases; buffer and processing bits match Lean. "
          f"Tuning/response maximum normalized error: {errors}")


if __name__ == "__main__":
    main()

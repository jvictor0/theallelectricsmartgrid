#!/usr/bin/env python3
"""Compare all generated state/output fields with native Lean, bit for bit."""

import os
from pathlib import Path
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/lean"
GENERATED = BUILD / "generated"
HEADERS = (
    "OnePoleZOHLowPass.hpp", "OnePoleTPTLowPass.hpp", "OnePoleTPTHighPass.hpp",
    "TPTBaseWidth.hpp", "TPTSVF.hpp", "MorphingTPTSVF.hpp", "TPTSVF4PoleHighPass.hpp",
    "TPTSVF4PoleLowPass.hpp", "TPTSVF4PoleBandPass.hpp", "TPTLadder.hpp",
    "LinkwitzRileyCrossover.hpp", "CoupledProcessor.hpp",
)


def main():
    reference = subprocess.run(
        [str(BUILD / "artifacts/bin/dsp_codegen_reference")],
        check=True, capture_output=True, text=True, timeout=30,
    ).stdout
    cases = reference.splitlines()
    assert cases, "Lean produced no reference cases"
    input_counts = {
        "zoh": 3, "tpt": 4, "tptHigh": 5, "baseWidth": 8, "svf": 11, "morph": 15,
        "svf4High": 21, "svf4Low": 21, "svf4Band": 21, "ladder": 15, "crossover": 31,
        "swap": 4, "gain": 2, "coupled": 3,
    }
    inputs, expected = [], []
    for case in cases:
        columns = case.split()
        split = 1 + input_counts[columns[0]]
        inputs.append(" ".join(columns[:split]))
        expected.append(" ".join(columns[split:]))

    output = BUILD / "codegen-test"
    output.mkdir(parents=True, exist_ok=True)
    (output / "reference.txt").write_text(reference)
    runner = output / "runner"
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    subprocess.run(compiler + [
        "-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-Wall", "-Wextra", "-Werror", "-I", str(GENERATED),
        str(ROOT / "formal/DSP/Tests/codegen_runner.cpp"), "-o", str(runner),
    ], check=True)
    actual = subprocess.run(
        [str(runner)], input="\n".join(inputs) + "\n",
        check=True, capture_output=True, text=True, timeout=30,
    ).stdout.splitlines()
    assert len(actual) == len(expected), "C++ returned a different number of results"
    for index, (found, wanted) in enumerate(zip(actual, expected)):
        assert found == wanted, (
            f"Case {index}: {inputs[index]}\nLean: {wanted}\nC++:  {found}"
        )

    for name in HEADERS:
        snapshot = ROOT / "formal/DSP/generated" / name
        assert snapshot.read_bytes() == (GENERATED / name).read_bytes(), (
            f"Generated {name} is stale; run make lean-dsp-generate and review its diff"
        )
    print(f"C++ codegen: {len(cases)} native Lean/C++ cases match bit for bit; snapshot current.")


if __name__ == "__main__":
    main()

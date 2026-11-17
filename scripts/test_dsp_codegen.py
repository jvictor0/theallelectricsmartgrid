#!/usr/bin/env python3
"""Compare all generated state/output fields with native Lean, bit for bit."""

import os
from pathlib import Path
import shlex
import struct
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
HEADERS += tuple(name.replace(".hpp", "Generic.hpp") for name in HEADERS)


def test_response(cases, output):
    """Compare pure calls with independently chosen coefficients and inputs per lane."""
    inputs, expected, labels = [], [], []
    for precision in (32, 64):
        tag = "zohResponse32" if precision == 32 else "zohResponse"
        rows = [case.split()[1:] for case in cases if case.split()[0] == tag]
        assert rows, f"No {precision}-bit response reference cases"
        for width in (1, 2, 3, 4):
            for start in range(0, len(rows), width):
                lanes = rows[start:start + width]
                lanes += [lanes[-1]] * (width - len(lanes))
                inputs.append(f"zohResponse {precision} {width} 1")
                inputs.append(" ".join(value for lane in lanes for value in lane[:6]))
                expected.append(" ".join(value for lane in lanes for value in lane[6:]))
                labels.append(f"ZOH response, {precision} bits, {width} channels, batch {start}")
    actual = subprocess.run(
        [str(output / "channels-runner")], input="\n".join(inputs) + "\n",
        check=True, capture_output=True, text=True, timeout=30,
    ).stdout.splitlines()
    assert len(actual) == len(expected), "Response C++ returned a different number of results"
    for label, found, wanted in zip(labels, actual, expected):
        assert found == wanted, f"{label}\nLean: {wanted}\nC++:  {found}"
    print(f"ZOH response codegen: {len(expected)} pure vector calls match native Lean and scalar C++ "
          "bit for bit at widths 1, 2, 3, 4 in float and double.")


def test_channels(cases, compiler, output, input_counts):
    """Pack distinct scalar trajectories into SIMD lanes, retaining C++ state."""
    trajectories = {}
    for case in cases:
        columns = case.split()
        tag = columns[0]
        precision = 32 if tag.endswith("32") else 64
        kind = tag.removesuffix("32")
        if kind in ("swap", "gain"):
            continue
        split = 1 + input_counts[kind]
        state, sample, expected = columns[1:split - 1], columns[split - 1], columns[split:]
        if kind == "coupled":
            fmt = ("f", "I") if precision == 32 else ("d", "Q")
            coefficients = [0.75, -0.125, 0.25, 0.5, 0.5, -0.25, 0.75, -0.5, 0.125]
            state = [str(struct.unpack(fmt[1], struct.pack(fmt[0], x))[0]) for x in coefficients] + state
        chains = trajectories.setdefault((kind, precision), [])
        if not chains or len(chains[-1]) == 64 or chains[-1][-1][2][:len(state)] != state:
            chains.append([])
        chains[-1].append((state, sample, expected))

    inputs, expected, labels = [], [], []
    for (kind, precision), chains in trajectories.items():
        for width in (1, 2, 3, 4):
            for start in range(0, len(chains), width):
                lanes = chains[start:start + width]
                lanes += [lanes[-1]] * (width - len(lanes))
                # Unequal lengths get separate batches; every continued lane
                # must have a reference sample for the entire trajectory.
                lengths = {len(lane) for lane in lanes}
                if len(lengths) != 1:
                    batches = [[lane] * width for lane in lanes]
                else:
                    batches = [lanes]
                for batch in batches:
                    steps = len(batch[0])
                    inputs.append(f"{kind} {precision} {width} {steps}")
                    inputs.append(" ".join(value for lane in batch for value in lane[0][0]))
                    for step in range(steps):
                        inputs.append(" ".join(lane[step][1] for lane in batch))
                        expected.append(" ".join(value for lane in batch for value in lane[step][2]))
                        labels.append(f"{kind}, {precision} bits, {width} channels, batch {start}, step {step}")

    runner = output / "channels-runner"
    subprocess.run(compiler + [
        "-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-Wall", "-Wextra", "-Werror", "-I", str(GENERATED), "-I", str(ROOT / "private/src"),
        str(ROOT / "formal/DSP/Tests/codegen_channels_runner.cpp"), "-o", str(runner),
    ], check=True)
    actual = subprocess.run(
        [str(runner)], input="\n".join(inputs) + "\n", check=True,
        capture_output=True, text=True, timeout=60,
    ).stdout.splitlines()
    assert len(actual) == len(expected), "Multichannel C++ returned a different number of results"
    for label, found, wanted in zip(labels, actual, expected):
        assert found == wanted, f"{label}\nLean: {wanted}\nC++:  {found}"
    print(f"Multichannel codegen: {len(expected)} vector steps match native Lean bit for bit "
          "at widths 1, 2, 3, 4 in float and double.")


def test_all_responses(cases, compiler, output):
    """Every TPT response at both precisions, independent coefficients in each lane."""
    runner = output / "responses-runner"
    subprocess.run(compiler + [
        "-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-Wall", "-Wextra", "-Werror", "-I", str(GENERATED), "-I", str(ROOT / "private/src"),
        str(ROOT / "formal/DSP/Tests/codegen_responses_runner.cpp"), "-o", str(runner),
    ], check=True)
    inputs, expected, labels = [], [], []
    for precision in (32, 64):
        tag = "allResponses32" if precision == 32 else "allResponses"
        rows = [case.split()[1:] for case in cases if case.split()[0] == tag]
        assert rows, f"No TPT response cases for precision {precision}"
        for width in (1, 2, 3, 4):
            for start in range(0, len(rows), width):
                lanes = rows[start:start + width]
                lanes += [lanes[-1]] * (width - len(lanes))
                inputs.append(f"{precision} {width}")
                inputs.append(" ".join(value for lane in lanes for value in lane[:15]))
                expected.append(" ".join(value for lane in lanes for value in lane[15:]))
                labels.append(f"TPT responses, {precision} bits, width {width}, row {start}")
    actual = subprocess.run([str(runner)], input="\n".join(inputs) + "\n", check=True,
                            capture_output=True, text=True, timeout=30).stdout.splitlines()
    assert len(actual) == len(expected), "Different number of TPT response results"
    for label, found, wanted in zip(labels, actual, expected):
        assert found == wanted, f"{label}\nLean: {wanted}\nC++: {found}"
    print(f"TPT response codegen: {len(expected)} vector batches, 14 responses each, match native Lean "
          "bit for bit in float/double at widths 1, 2, 3, 4; scalar double and independent complex checks pass.")


def main():
    reference = subprocess.run(
        [str(BUILD / "artifacts/bin/dsp_codegen_reference")],
        check=True, capture_output=True, text=True, timeout=30,
    ).stdout
    response_cases = [case for case in reference.splitlines() if case.startswith("zohResponse")]
    all_response_cases = [case for case in reference.splitlines() if case.startswith("allResponses")]
    cases = [case for case in reference.splitlines()
             if not case.startswith(("zohResponse", "allResponses"))]
    assert cases, "Lean produced no reference cases"
    input_counts = {
        "zoh": 3, "tpt": 4, "tptHigh": 5, "baseWidth": 8, "svf": 11, "morph": 15,
        "svf4High": 21, "svf4Low": 21, "svf4Band": 21, "ladder": 15, "crossover": 31,
        "swap": 4, "gain": 2, "coupled": 3, "arithmetic": 3, "fma": 3, "fma32": 3,
    }
    inputs, expected = [], []
    scalar_cases = [case for case in cases
                    if (not case.split()[0].endswith("32") or case.split()[0] == "fma32")
                    and case.split()[0] != "arithmetic"]
    for case in scalar_cases:
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
        "-Wall", "-Wextra", "-Werror", "-I", str(GENERATED), "-I", str(ROOT / "private/src"),
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
    print(f"C++ codegen: {len(scalar_cases)} native Lean/C++ cases match bit for bit; snapshot current.")
    test_channels(cases, compiler, output, input_counts)
    test_response(response_cases, output)
    test_all_responses(all_response_cases, compiler, output)


if __name__ == "__main__":
    main()

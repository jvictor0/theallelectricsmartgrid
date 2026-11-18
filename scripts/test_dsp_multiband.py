"""Differential execution of the generated coupled-channel mastering core."""
import subprocess


def test_multiband(compiler, output, root):
    build = root / "build/lean"
    reference = subprocess.run([str(build / "artifacts/bin/dsp_multiband_reference")],
                               capture_output=True, text=True, check=True, timeout=60).stdout
    (output / "multiband-reference.txt").write_text(reference)
    inputs, expected = [], []
    for row in reference.splitlines():
        if " | " in row:
            sample, result = row.split(" | ")
            inputs.append(sample)
            expected.append(result)
        else:
            inputs.append(row)
    runner = output / "multiband-runner"
    subprocess.run(compiler + ["-std=c++17", "-O2", "-fno-fast-math", "-ffp-contract=off",
        "-Wall", "-Wextra", "-Werror", "-I", str(build / "generated"), "-I", str(root / "private/src"),
        str(root / "formal/DSP/Tests/codegen_multiband_runner.cpp"), "-o", str(runner)], check=True)
    result = subprocess.run([str(runner)], input="\n".join(inputs)+"\n", capture_output=True,
                            text=True, check=True, timeout=30).stdout.splitlines()
    assert len(result) == len(expected), "Multiband result count differs"
    for i, (actual, wanted) in enumerate(zip(result, expected)):
        assert actual == wanted, f"Multiband sample {i}:\nLean: {wanted}\nC++: {actual}"
    print(f"Multiband: {len(expected)} coupled-channel steps match native Lean bit for bit; "
          "all histories, meter observations and power responses, both precisions, widths 1–4, switching mono bass.")

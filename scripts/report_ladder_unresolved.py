#!/usr/bin/env python3
"""Merge the diagnostic and longer checks without overwriting the original grid."""

import argparse
import collections
import csv
import json
from pathlib import Path

import numpy as np

from analyze_ladder_periodic_grid import ROOT


def read_rows(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grid", type=Path, default=ROOT / "build/results/ladder-periodic-grid")
    parser.add_argument("--diagnostics", type=Path, default=ROOT / "build/results/ladder-unresolved")
    parser.add_argument("--extended", type=Path, default=ROOT / "build/results/ladder-unresolved-extended")
    parser.add_argument("--chaos-check", type=Path, default=ROOT / "build/results/ladder-chaos-check/neighbor-check.json")
    args = parser.parse_args()
    results = {row["id"]: row for row in read_rows(args.diagnostics / "diagnostics.jsonl")}
    extended = read_rows(args.extended / "diagnostics.jsonl")
    for row in extended:
        assert row["id"] in results
        results[row["id"]] = row
    rows = sorted(results.values(), key=lambda row: row["id"])
    counts = dict(collections.Counter(row["classification"] for row in rows))
    labels = dict(near_neutral_nonperiodic="Drifting / quasiperiodic-looking oscillation",
                  longer_periodic="Longer locked cycle", late_periodic="Late input-period settling",
                  positive_tangent_growth="Chaos candidate", ambiguous_finite_time="Ambiguous",
                  contracting_not_yet_periodic="Contracting; no period found")
    assert len(rows) == 2868 and sum(counts.values()) == len(rows)
    assert all(row["critical_ratio"] > 1 for row in rows)
    drift = [row for row in rows if row["classification"] == "near_neutral_nonperiodic"]
    power_change = [abs(row["power_first_last_relative_change"]) for row in drift]
    summary = dict(rows=len(rows), counts=counts, extended_cases=len(extended),
        power_change_percentiles={str(p): float(np.percentile(power_change, p)) for p in [0, 50, 90, 99, 100]},
        longer_period_multipliers=dict(collections.Counter(row["candidate_period_multiplier"] for row in rows
                                                          if row["classification"] == "longer_periodic")),
        critical_ratio_range=[min(row["critical_ratio"] for row in rows), max(row["critical_ratio"] for row in rows)],
        evidence="Numerical classification, not a proof of quasiperiodicity, asymptotic chaos, or uniqueness.")
    (args.diagnostics / "final-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    (args.diagnostics / "final-diagnostics.jsonl").write_text("".join(json.dumps(row, allow_nan=False) + "\n" for row in rows))
    flat = [{key: value for key, value in row.items() if key != "verification" and not isinstance(value, (dict, list))}
            for row in rows]
    with (args.diagnostics / "final-classification.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(flat[0]))
        writer.writeheader()
        writer.writerows(flat)
    with (args.grid / "responses.csv").open() as stream:
        original = list(csv.DictReader(stream))
    for row in original:
        record = results.get(row["id"])
        row["diagnostic_classification"] = record["classification"] if record else "previously_periodic"
        row["diagnostic_lyapunov_per_second"] = record["lyapunov_tail_per_second"] if record else ""
        row["diagnostic_power"] = record["power_tail"] if record else ""
        row["diagnostic_period_multiplier"] = (record["candidate_period_multiplier"]
                                              if record and record["classification"] in ["longer_periodic", "late_periodic"] else "")
        row["diagnostic_audio_seconds"] = record["observed_audio_seconds"] if record else ""
    with (args.diagnostics / "annotated-grid.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(original[0]))
        writer.writeheader()
        writer.writerows(original)
    chaos = json.loads(args.chaos_check.read_text())
    assert all(abs(result["relative_difference_from_tangent"]) < 0.001 for result in chaos["finite_perturbations"])
    qperiods = sorted(summary["longer_period_multipliers"])
    report = ["# What happened in the 2,868 unresolved settings?", "",
              "These are continuations of the saved binary64 states from the detector-disabled real-tanh sweep. "
              "The original table is preserved. All cases received at least eight additional seconds and "
              "512 input periods; 50 ambiguous or unusual cases received a further 64 seconds.", "",
              "| Numerical classification | Settings |", "| --- | ---: |"]
    for classification, count in counts.items():
        report.append(f"| {labels[classification]} | {count:,} |")
    report += ["", "## Drifting oscillations", "",
               "The 2,838 cases in this group have large one-period recurrence errors, nearly neutral "
               "conditional tangent growth, and a thin loop-like Poincare projection. Output spectra show "
               "an oscillation near the filter's resonant region, often alongside the input, harmonics, "
               "and mixing sidebands. These observations are consistent with quasiperiodicity; they do not "
               "prove irrational frequency ratios, exclude very long locking periods, or rule out extremely weak chaos.", "",
               f"Mean power changes between the first and second halves of each diagnostic window: "
               f"median {100*np.percentile(power_change, 50):.5f}%, 99th percentile "
               f"{100*np.percentile(power_change, 99):.4f}%, maximum {100*max(power_change):.4f}%. "
               "Phase can keep moving while energy statistics are already steady. These are finite-window "
               "comparisons, not convergence bounds.", "",
               "## Longer cycles", "",
               f"The 25 new whole-waveform repeats have multipliers {', '.join(map(str, qperiods))} "
               "of the input's sample period. Each passes the original per-stage absolute/relative tolerance "
               "through eight consecutive repeat checks and a separate measurement window. The multipliers "
               "are verified repeat lengths, not assertions of minimal period. This is in addition to the "
               "27 longer cycles already found in the original sweep. None of these 2,868 cases newly passed "
               "the one-input-period test in this diagnostic.", "",
               "## One strongly sensitive configuration, five knob entries", "",
               "Input and cutoff both 20 kHz, drive knob 88.889% (g = 3.584355822), resonance knobs "
               "55.556–100%. All five resonance settings are capped to the same k = 0.617917522 by kSafe. "
               "They produce the same effective coefficients and observed trajectory, so this is one "
               "distinct physical setting represented five times in the grid.", "",
               "The extended conditional Lyapunov estimate is about +5,165 per second. Independent "
               "two-trajectory calculations with perturbations 1e-6, 1e-8, and 1e-10 reproduce the analytic "
               "tangent estimate on a common two-second trajectory to within 0.024%. Thus the positive "
               "growth is not merely an error in the tangent implementation. This is strong numerical "
               "evidence of chaos, not a mathematical proof. Saturation keeps the output bounded; its "
               "long-window mean power is also steady in these observations.", "",
               "## Parameter pattern", "",
               "Every originally unresolved case is above the zero-state linearized oscillation threshold. "
               "The ratio (k*s^3)/kCritical ranges from "
               f"{summary['critical_ratio_range'][0]:.6f} to {summary['critical_ratio_range'][1]:.6f}. "
               "This is a statement about the zero-state tangent, not the stability of a finite-amplitude response. "
               "The earlier sweep also found many periodic responses above that threshold.", "",
               "Of the drifting cases, 1,170 have input below half the cutoff, 1,578 above twice the cutoff, "
               "and 90 within one octave. Of all 2,868 originally unresolved rows, the corresponding counts "
               "are 1,189 / 1,582 / 97. The grid has 1,000 near-cutoff rows and 9,000 far-from-cutoff rows: "
               "9.7% versus 30.8% were originally unresolved.", "",
               "## Method and limits", "",
               "The exponent follows an infinitesimal four-state perturbation under the exact derivative "
               "of the nonlinear sample recurrence, renormalized every 32 samples and at period boundaries. "
               "The forcing phase is held fixed, avoiding a trivially neutral extra clock coordinate. "
               "The first half of each observation is discarded for the exponent estimate. "
               "A positive-growth candidate must have a positive rate in every one of the last four blocks, "
               "exceeding max(0.2, 2/tailDuration) per second. A neutral candidate has a tail mean within "
               "that tolerance. Longer observation is used to resolve marginal classifications.", "",
               "Endpoint recurrence search considers periods up to 128 input periods, then checks promising "
               "candidates on their full sampled waveforms. It is not an exhaustive proof of all periodic solutions. "
               "Spectral peaks use a Hann-windowed FFT and parabolic interpolation; frequency precision is "
               "limited by the stored spectral_resolution_hz. Strobe projection geometry and near-zero finite-time "
               "growth alone cannot establish an invariant torus.", "",
               "For background on tangent-growth diagnostics and quasiperiodic stability: "
               "[Fiedler, Hetzler and Bäuerle (2024)](https://doi.org/10.1007/s11071-024-09497-9). "
               "This experiment uses direct tangent iteration, not that paper's specialized invariant-manifold algorithm.", "",
               "## Files", "",
               "- final-classification.csv: one row for each of the 2,868 diagnosed settings.",
               "- annotated-grid.csv: original 10,000-row response table plus diagnostic columns; original response_status retained.",
               "- final-diagnostics.jsonl: merged observations, block powers/exponents, peaks and repeat verification.",
               "- strobes/: compact snapshots of the first diagnostic's Poincare sections.",
               "- ../ladder-unresolved-extended/: longer checks and their sections.",
               "- ../ladder-chaos-check/neighbor-check.json: independent finite-perturbation check.", "",
               "## Reproduce", "", "```sh", "make ladder-periodic-grid-build",
               "OPENBLAS_NUM_THREADS=1 python3 scripts/classify_ladder_periodic_grid.py",
               "OPENBLAS_NUM_THREADS=1 python3 scripts/classify_ladder_periodic_grid.py --refine-from build/results/ladder-unresolved --seconds 64 --output build/results/ladder-unresolved-extended",
               "OPENBLAS_NUM_THREADS=1 python3 scripts/classify_ladder_periodic_grid.py --neighbor-case f09-c09-g08-r05 --output build/results/ladder-chaos-check",
               "python3 scripts/report_ladder_unresolved.py", "```", ""]
    (args.diagnostics / "report.md").write_text("\n".join(report))
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()

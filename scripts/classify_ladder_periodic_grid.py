#!/usr/bin/env python3
"""Diagnose unresolved ladder trajectories; finite-time evidence, not proofs.

Continue the saved states with the same periodic forcing. Measure conditional
tangent growth with the forcing phase held fixed, longer recurrence periods,
Poincare geometry, spectral peaks and windowed powers. Preserve the input table.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
import ctypes
import hashlib
import json
import math
from pathlib import Path
import time

import numpy as np
from scipy.signal import find_peaks

from analyze_ladder_periodic_grid import ROOT, LIBRARY, DOUBLE_ARRAY, load_library, run_point


def load_trace_library():
    library = load_library()
    library.LadderGridTrace.argtypes = [DOUBLE_ARRAY, DOUBLE_ARRAY, ctypes.c_int, ctypes.c_int,
        ctypes.c_double, DOUBLE_ARRAY, DOUBLE_ARRAY, DOUBLE_ARRAY, DOUBLE_ARRAY, DOUBLE_ARRAY]
    library.LadderGridTrace.restype = None
    return library


def tones_for(row):
    q = row["period_samples"]
    phase = 2 * np.pi * ((np.arange(q) * row["input_cycles"]) % q) / q
    return np.ascontiguousarray(np.column_stack((np.cos(phase), np.sin(phase))))


def trace(library, row, tones, periods, state, tangent):
    q = len(tones)
    endpoints = np.empty((periods + 1, 4))
    features = np.empty((periods, 7))
    output = np.empty(periods * q)
    library.LadderGridTrace(np.array([row["alpha"], row["feedback"], row["gain"]]),
        tones, q, periods, row["amplitude"], state, tangent, endpoints, features, output)
    assert np.all(np.isfinite(features)) and np.all(np.isfinite(endpoints))
    return endpoints, features, output


def validate(library):
    rng = np.random.default_rng(4711)
    errors = []
    for alpha, feedback, gain in [(0.003, 3.9, 5), (0.1, 0.5, 2), (0.4, 1, 0.25)]:
        row = dict(alpha=alpha, feedback=feedback, gain=gain, amplitude=0.25)
        tones = np.ascontiguousarray(rng.normal(size=(19, 2)))
        initial, direction = rng.normal(size=(2, 4))
        direction /= np.linalg.norm(direction)
        state, tangent = initial.copy(), direction.copy()
        endpoints, features, output = trace(library, row, tones, 1, state, tangent)
        coefficients = np.array([alpha, feedback, gain])
        nearby = []
        step_trace = np.empty((len(tones), 4))
        for offset in [-1e-6, 0, 1e-6]:
            point = initial + offset * direction
            library.LadderGridCycle(coefficients, tones, len(tones), 0.25, point, step_trace)
            nearby.append(point)
            if offset == 0:
                assert np.array_equal(point, state)
                expected = (1 + feedback) * np.tanh(gain * step_trace[:, 3]) / math.tanh(gain)
                assert np.max(np.abs(expected - output)) < 1e-13
        numerical = (nearby[2] - nearby[0]) / 2e-6
        propagated = tangent * math.exp(features[0, 3])
        error = float(np.max(np.abs(numerical - propagated)))
        assert error < 2e-8, error
        errors.append(error)
    return dict(tangent_central_difference_max_error=max(errors), identical_state_updates=True)


def spectrum(output, fs):
    count = min(131072, len(output))
    signal = output[-count:]
    window = np.hanning(count)
    amplitude = 2 * np.abs(np.fft.rfft((signal - np.mean(signal)) * window)) / np.sum(window)
    peaks, _ = find_peaks(amplitude)
    peaks = sorted(peaks, key=lambda j: amplitude[j], reverse=True)[:8]
    result = []
    for j in peaks:
        logs = np.log(np.maximum(amplitude[j-1:j+2], 1e-300))
        correction = 0.5 * (logs[0] - logs[2]) / (logs[0] - 2 * logs[1] + logs[2])
        result.append(dict(frequency_hz=float((j + correction) * fs / count),
                           peak_amplitude=float(amplitude[j])))
    return result, fs / count


def candidate_period(endpoints, maximum=128):
    tail = endpoints[len(endpoints)//2:]
    maximum = min(maximum, len(tail)//2)
    scale = np.max(np.abs(tail), axis=0)
    tolerance = 1e-12 + 1e-9 * scale
    distances = np.max(np.abs(tail[-1] - tail[-1-np.arange(1, maximum+1)]) / tolerance, axis=1)
    candidates = sorted(set([1] + (np.argsort(distances)[:12] + 1).tolist()))
    scores = []
    for lag in candidates:
        width = min(256, len(tail) - lag)
        score = float(np.max(np.abs(tail[-width:] - tail[-width-lag:-lag]) / tolerance))
        scores.append((score, lag))
    good = [(score, lag) for score, lag in scores if score <= 100]
    return min(good, key=lambda x: x[1]) if good else min(scores)


def diagnose(library, row, args):
    fs = args.sample_rate
    q = row["period_samples"]
    per_block = max(64, math.ceil(args.seconds * fs / q / 8))
    periods = per_block * 8
    tones = tones_for(row)
    state = np.array([row[f"state_{j}"] for j in range(1, 5)])
    tangent = np.full(4, 0.5)
    started = time.perf_counter()
    endpoints, features, output = trace(library, row, tones, periods, state, tangent)
    block_seconds = per_block * q / fs
    blocks = features.reshape(8, per_block, 7)
    exponents = np.sum(blocks[:, :, 3], axis=1) / block_seconds
    powers = np.mean(blocks[:, :, 0], axis=1)
    coherent = np.mean(blocks[:, :, 1:3], axis=1)
    tail = endpoints[len(endpoints)//2:]
    centered = tail - np.mean(tail, axis=0)
    covariance = centered.T @ centered / len(centered)
    eigenvalues, vectors = np.linalg.eigh(covariance)
    plane = centered @ vectors[:, -2:]
    plane /= np.sqrt(np.maximum(eigenvalues[-2:], 1e-300))
    angles = np.unwrap(np.arctan2(plane[:, 1], plane[:, 0]))
    phase_steps = np.diff(angles)
    period_score, lag = candidate_period(endpoints)
    verification = None
    if period_score <= 100:
        local = argparse.Namespace(seconds=10.0, sample_rate=fs, amplitude=row["amplitude"],
                                  absolute_tolerance=1e-12, relative_tolerance=1e-9)
        verification, _ = run_point(library, row, np.ascontiguousarray(np.tile(tones, (lag, 1))), state, local)
    peaks, resolution = spectrum(output, fs)
    ratio = row["alpha"] / (1 - row["alpha"])
    critical = (ratio + 2) ** 2 if ratio < 2 else ((ratio + 2) / ratio) ** 4
    last = exponents[4:]
    threshold = max(0.2, 2 / (4 * block_seconds))
    if verification and verification["status"] == "periodic":
        classification = "late_periodic" if lag == 1 else "longer_periodic"
    elif float(np.min(last)) > threshold:
        classification = "positive_tangent_growth"
    elif abs(float(np.mean(last))) <= threshold:
        classification = "near_neutral_nonperiodic"
    elif float(np.max(last)) < -threshold:
        classification = "contracting_not_yet_periodic"
    else:
        classification = "ambiguous_finite_time"
    result = dict(id=row["id"], classification=classification,
        frequency_hz=row["frequency_hz"], cutoff_hz=row["cutoff_hz"], drive_knob=row["drive_knob"],
        resonance_knob=row["resonance_knob"], effective_resonance=row["effective_resonance"],
        critical_ratio=row["effective_resonance"] / critical,
        observed_audio_seconds=periods*q/fs, block_seconds=block_seconds,
        lyapunov_blocks_per_second=exponents.tolist(), lyapunov_tail_per_second=float(np.mean(last)),
        lyapunov_threshold_per_second=threshold,
        power_blocks=powers.tolist(), coherent_blocks=coherent.tolist(),
        power_tail=float(np.mean(powers[4:])),
        power_tail_relative_range=float(np.ptp(powers[4:]) / np.mean(powers[4:])),
        power_first_last_relative_change=float((np.mean(powers[4:]) - np.mean(powers[:4])) / np.mean(powers)),
        strobe_eigenvalues=eigenvalues.tolist(),
        strobe_out_of_plane_fraction=float(np.sum(eigenvalues[:2]) / max(np.sum(eigenvalues), 1e-300)),
        strobe_radius_cv=float(np.std(np.linalg.norm(plane, axis=1)) / max(np.mean(np.linalg.norm(plane, axis=1)), 1e-300)),
        strobe_phase_step_mean=float(np.mean(phase_steps)), strobe_phase_step_std=float(np.std(phase_steps)),
        strobe_phase_direction_fraction=float(max(np.mean(phase_steps > 0), np.mean(phase_steps < 0))),
        candidate_period_multiplier=lag, candidate_period_score=period_score,
        final_q_repeat_score=float(np.max(features[-8:, 4])),
        final_q_repeat_absolute=float(np.max(features[-8:, 5])),
        spectral_peaks=peaks, spectral_resolution_hz=resolution,
        verification=verification, compute_seconds=time.perf_counter() - started,
        state=state.tolist(), tangent=tangent.tolist())
    indices = np.linspace(0, len(tail)-1, min(len(tail), 512), dtype=int)
    np.savez_compressed(args.output / "strobes" / f"{row['id']}.npz", states=tail[indices],
                        sample_period=q, phase_indices=indices, plane=plane[indices])
    return result


def neighbor_check(library, row, args):
    """Independent two-trajectory perturbations, renormalized once per period."""
    tones = tones_for(row)
    q = len(tones)
    coefficients = np.array([row["alpha"], row["feedback"], row["gain"]])
    initial = np.array([row[f"state_{j}"] for j in range(1, 5)])
    periods = 8 * math.ceil(2 * args.sample_rate / q / 8)
    reference = initial.copy()
    tangent = np.full(4, 0.5)
    _, features, _ = trace(library, row, tones, periods, reference, tangent)
    seconds = periods * q / args.sample_rate
    analytical = float(np.sum(features[:, 3]) / seconds)
    results = []
    buffer = np.empty((q, 4))
    for epsilon in [1e-6, 1e-8, 1e-10]:
        state = initial.copy()
        direction = np.full(4, 0.5)
        stretches = []
        for _ in range(periods):
            neighbor = state + epsilon * direction
            library.LadderGridCycle(coefficients, tones, q, row["amplitude"], state, buffer)
            library.LadderGridCycle(coefficients, tones, q, row["amplitude"], neighbor, buffer)
            delta = neighbor - state
            norm = np.linalg.norm(delta)
            stretches.append(math.log(norm / epsilon))
            direction = delta / norm
        assert np.array_equal(state, reference), "Reference recurrence differs between trace and cycle kernels"
        estimate = float(np.sum(stretches) / seconds)
        results.append(dict(epsilon=epsilon, exponent_per_second=estimate,
                            relative_difference_from_tangent=(estimate - analytical) / analytical))
    state, neighbor = initial.copy(), initial + 1e-9 * np.full(4, 0.5)
    separation = []
    for cycle in range(80):
        library.LadderGridCycle(coefficients, tones, q, row["amplitude"], state, buffer)
        library.LadderGridCycle(coefficients, tones, q, row["amplitude"], neighbor, buffer)
        separation.append([(cycle + 1) * q / args.sample_rate, float(np.linalg.norm(neighbor - state))])
    record = dict(id=row["id"], seconds=seconds, tangent_exponent_per_second=analytical,
                  finite_perturbations=results, unrenormalized_separation=separation,
                  parameters={key: row[key] for key in ["alpha", "gain", "feedback", "frequency_hz", "cutoff_hz"]})
    (args.output / "neighbor-check.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({key: value for key, value in record.items() if key != "unrenormalized_separation"}), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=ROOT / "build/results/ladder-periodic-grid")
    parser.add_argument("--output", type=Path, default=ROOT / "build/results/ladder-unresolved")
    parser.add_argument("--seconds", type=float, default=8.0)
    parser.add_argument("--sample-rate", type=float, default=192000.0)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--refine-from", type=Path,
                        help="Extend ambiguous, strongly expanding, or geometrically unusual previous cases")
    parser.add_argument("--neighbor-case", help="Cross-check one case using finite nearby trajectories")
    args = parser.parse_args()
    if (args.output / "diagnostics.jsonl").exists():
        raise FileExistsError("Choose a new --output directory to preserve prior results")
    (args.output / "strobes").mkdir(parents=True, exist_ok=True)
    rows = [json.loads(line) for line in (args.input / "table.jsonl").read_text().splitlines()]
    if args.neighbor_case:
        row = next(row for row in rows if row["id"] == args.neighbor_case)
        neighbor_check(load_trace_library(), row, args)
        return
    longer = {row["id"] for row in json.loads((args.input / "multiple-period-checks.json").read_text())["rows"]
              if row["status"] == "periodic"}
    selected = [row for row in rows if row["status"] != "periodic" and row["id"] not in longer]
    if args.refine_from:
        previous = [json.loads(line) for line in (args.refine_from / "diagnostics.jsonl").read_text().splitlines()]
        chosen = {row["id"]: row for row in previous
                  if row["classification"] not in ["near_neutral_nonperiodic", "longer_periodic", "late_periodic"]
                  or (row["classification"] == "near_neutral_nonperiodic" and
                      (abs(row["lyapunov_tail_per_second"]) > 0.25 or row["strobe_out_of_plane_fraction"] > 0.03
                       or row["strobe_radius_cv"] > 0.5))}
        selected = [row for row in selected if row["id"] in chosen]
        for row in selected:
            for j, value in enumerate(chosen[row["id"]]["state"], 1):
                row[f"state_{j}"] = value
    if args.limit:
        indices = np.linspace(0, len(selected)-1, args.limit, dtype=int)
        selected = [selected[index] for index in indices]
    library = load_trace_library()
    validation = validate(library)
    print(json.dumps(dict(validation=validation, cases=len(selected))), flush=True)
    metadata = dict(arguments={key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
        input_sha256=hashlib.sha256((args.input / "table.jsonl").read_bytes()).hexdigest(),
        library_sha256=hashlib.sha256(LIBRARY.read_bytes()).hexdigest(), validation=validation,
        method="Continue saved endpoint with fixed forcing phase; 8 blocks of >=64 periods and >=1 second. Discard first half for tangent summaries. Search endpoint periods <=128, validate promising ones against full waveforms.",
        limitation="Finite-time classifications are empirical. Near-zero tangent growth does not prove quasiperiodicity; positive finite-time growth does not prove asymptotic chaos.")
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (args.output / Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    (args.output / "ladder_periodic_grid.cpp").write_bytes(Path(__file__).with_name("ladder_periodic_grid.cpp").read_bytes())
    started = time.perf_counter()
    results = []
    with (args.output / "diagnostics.jsonl").open("w") as stream, ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(diagnose, library, row, args) for row in selected]
        for future in as_completed(futures):
            result = future.result()
            results.append(result)
            stream.write(json.dumps(result, allow_nan=False) + "\n")
            stream.flush()
            if len(results) % 100 == 0 or len(results) == len(selected):
                counts = {key: sum(row["classification"] == key for row in results)
                          for key in sorted({row["classification"] for row in results})}
                print(json.dumps(dict(done=len(results), counts=counts, seconds=time.perf_counter()-started)), flush=True)
    flat = [{key: value for key, value in row.items() if key != "verification" and not isinstance(value, (dict, list))}
            for row in results]
    with (args.output / "classification.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(flat[0]))
        writer.writeheader()
        writer.writerows(sorted(flat, key=lambda row: row["id"]))
    (args.output / "summary.json").write_text(json.dumps(dict(rows=len(results), counts=counts,
        wall_seconds=time.perf_counter()-started, validation=validation), indent=2) + "\n")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Brute-force periodic-response table for the detector-disabled real-tanh ladder.

All acceptance tests are binary64 numerical observations, not proof certificates.
Iteration cannot find repelling fixed points. A timeout is not nonexistence.
The native loop mirrors the existing Lean recurrence and is checked against the
independent Python Model before running. No app or generated DSP is changed.
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

from analyze_ladder_harmonic_reduction import Model


ROOT = Path(__file__).resolve().parents[1]
LIBRARY = ROOT / "build/tools/ladder-periodic-grid/libladder_periodic_grid.so"
DOUBLE_ARRAY = np.ctypeslib.ndpointer(dtype=np.float64, flags="C_CONTIGUOUS")


def load_library():
    library = ctypes.CDLL(str(LIBRARY))
    library.LadderGridCycle.argtypes = [DOUBLE_ARRAY, DOUBLE_ARRAY, ctypes.c_int,
                                      ctypes.c_double, DOUBLE_ARRAY, DOUBLE_ARRAY]
    library.LadderGridCycle.restype = None
    library.LadderGridSettle.argtypes = [DOUBLE_ARRAY, DOUBLE_ARRAY, ctypes.c_int,
        ctypes.c_double, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
        ctypes.c_double, ctypes.c_double, DOUBLE_ARRAY, DOUBLE_ARRAY]
    library.LadderGridSettle.restype = None
    return library


def periodic_frequency(target, sample_rate, tolerance):
    """Use the fewest input cycles whose nearest integer period meets tolerance."""
    for cycles in range(1, 10001):
        period = round(sample_rate * cycles / target)
        actual = sample_rate * cycles / period
        if abs(actual / target - 1) <= tolerance:
            divisor = math.gcd(cycles, period)
            cycles, period = cycles // divisor, period // divisor
            phase = 2 * np.pi * ((np.arange(period) * cycles) % period) / period
            tones = np.ascontiguousarray(np.column_stack((np.cos(phase), np.sin(phase))))
            return cycles, period, actual, tones
    raise ValueError("Could not select a short repeating frequency")


def parameters(drive, resonance, cutoff, sample_rate):
    gain = 0.25 * 20 ** drive
    slope = gain / math.tanh(gain)
    requested = 3.9 * math.expm1(math.log(49) * resonance) / 48
    sine = math.sin(math.pi * cutoff / sample_rate)
    alpha = 2 * sine * (math.sqrt(1 + sine * sine) - sine)
    safe = 0.9 / ((alpha / (2 - alpha) * slope) ** 4 + 1e-8)
    feedback = min(requested, safe)
    return dict(gain=gain, slope=slope, requested_feedback=requested,
                feedback=feedback, k_safe=safe, alpha=alpha,
                effective_resonance=feedback * slope ** 3)


def run_point(library, params, tones, seed, args):
    period = len(tones)
    state = np.array(seed, dtype=np.float64, copy=True)
    statistics = np.empty(18)
    # Require both a reasonable number of forcing periods and 20 standalone
    # stage time constants. This does not presume closed-loop settling.
    minimum = max(16, math.ceil(20 / params["alpha"] / period))
    maximum = max(minimum + 8, math.ceil(args.seconds * args.sample_rate / period))
    measured = max(8, math.ceil(0.1 * args.sample_rate / period))
    coefficients = np.array([params["alpha"], params["feedback"], params["gain"]])
    started, cpu_started = time.perf_counter(), time.thread_time()
    library.LadderGridSettle(coefficients, tones, period, args.amplitude,
        maximum, minimum, 8, measured, args.absolute_tolerance,
        args.relative_tolerance, state, statistics)
    wall, cpu = time.perf_counter() - started, time.thread_time() - cpu_started
    if not np.all(np.isfinite(statistics)) or not np.all(np.isfinite(state)):
        raise ArithmeticError((params, statistics, state))
    converged = bool(statistics[0])
    power, real, imag = statistics[6:9]
    result = dict(status="periodic" if converged else
                  ("verification_failed" if statistics[17] else "timeout"),
        settling_periods=int(statistics[1]),
        settling_samples=int(statistics[1]) * period,
        settling_audio_seconds=statistics[1] * period / args.sample_rate,
        compute_cpu_seconds=cpu, compute_wall_seconds=wall,
        first_passing_period=int(statistics[16]),
        period_defect_score=statistics[2], period_defect_absolute=statistics[3],
        verification_defect_score=statistics[4], verification_defect_absolute=statistics[5],
        measurement_periods=measured, measurement_samples=measured * period,
        measurement_audio_seconds=measured * period / args.sample_rate,
        total_power=power, rms=math.sqrt(power), coherent_real=real, coherent_imag=imag,
        coherent_power=(real * real + imag * imag) / 2,
        output_mean=statistics[9], output_peak=statistics[10],
        minimum_period_power=statistics[11], maximum_period_power=statistics[12],
        endpoint_best_lag=int(statistics[13]), endpoint_lag_score=statistics[14],
        maximum_periods=maximum, minimum_periods=minimum,
        state_1=state[0], state_2=state[1], state_3=state[2], state_4=state[3])
    result["other_power"] = max(0.0, power - result["coherent_power"])
    return result, state


def warm_start(completed, target):
    """Interpolate normalized histories at equal k*s^3 within a solved drive slice.

    The linearized histories (x1,x2/s,x3/s^2,x4/s^3) depend on k*s^3,
    not on drive separately. This is a seed heuristic at finite input amplitude.
    Only accepted periodic rows are eligible; timeout endpoints are not roots.
    """
    by_drive = {}
    for row, state in completed:
        if row["status"] == "periodic":
            by_drive.setdefault(row["drive_knob"], []).append((row, state))
    kappa = target["effective_resonance"]
    candidates = []
    powers = np.arange(4)
    for drive, rows in by_drive.items():
        rows.sort(key=lambda item: item[0]["effective_resonance"])
        below = [item for item in rows if item[0]["effective_resonance"] <= kappa]
        above = [item for item in rows if item[0]["effective_resonance"] >= kappa]
        if below and above:
            left, right = below[-1], above[0]
            lower, upper = left[0]["effective_resonance"], right[0]["effective_resonance"]
            fraction = 0.0 if upper == lower else (kappa - lower) / (upper - lower)
            normalized = ((1 - fraction) * left[1] / left[0]["slope"] ** powers +
                          fraction * right[1] / right[0]["slope"] ** powers)
            mismatch = 0.0
            method = "equal_kappa_interpolation" if upper != lower else "equal_kappa"
            spread = math.log1p(upper) - math.log1p(lower)
            sources = [left[0]["id"], right[0]["id"]]
        else:
            nearest = min(rows, key=lambda item: abs(math.log1p(item[0]["effective_resonance"]) -
                                                     math.log1p(kappa)))
            normalized = nearest[1] / nearest[0]["slope"] ** powers
            mismatch = abs(math.log1p(nearest[0]["effective_resonance"]) - math.log1p(kappa))
            method, spread, fraction, sources = "nearest_kappa", 0.0, 0.0, [nearest[0]["id"]]
        drive_distance = abs(math.log(target["slope"] / rows[0][0]["slope"]))
        score = (0 if mismatch == 0 else 1, mismatch + 0.05 * drive_distance + 0.02 * spread)
        candidates.append((score, normalized * target["slope"] ** powers,
                           dict(seed_method=method, seed_sources=";".join(sources),
                                seed_fraction=fraction, seed_log_kappa_mismatch=mismatch)))
    if not candidates:
        return np.zeros(4), dict(seed_method="reset", seed_sources="", seed_fraction=0.0,
                                seed_log_kappa_mismatch=0.0)
    _, state, info = min(candidates, key=lambda candidate: candidate[0])
    return state, info


def validate(library, args):
    rng = np.random.default_rng(271828)
    worst = 0.0
    for drive, resonance, cutoff in [(0, 0, 20), (0.5, 1, 1000), (1, 1, 20000)]:
        params = parameters(drive, resonance, cutoff, args.sample_rate)
        model = Model("tanh", drive, resonance, cutoff, args.sample_rate)
        model.k = params["feedback"]
        state = rng.normal(0, 0.2, 4)
        expected = state.copy()
        tones = np.ascontiguousarray(rng.normal(size=(257, 2)))
        trace = np.empty((len(tones), 4))
        library.LadderGridCycle(np.array([params["alpha"], model.k, model.g]),
                               tones, len(tones), args.amplitude, state, trace)
        for index, tone in enumerate(tones):
            expected = model.step(expected, args.amplitude * tone[0])
            worst = max(worst, float(np.max(np.abs(trace[index] - expected))))
    assert worst < 2e-13, worst
    # Near the zero-drive limit, compare the independently solved complex
    # linear recurrence, including old-stage-four feedback and output gain.
    _, period, actual, tones = periodic_frequency(1000, args.sample_rate, 0.001)
    params = parameters(0, 0.5, 1000, args.sample_rate)
    params.update(gain=1e-7)
    result, _ = run_point(library, params, tones, np.zeros(4), args)
    delay = np.exp(-2j * np.pi * actual / args.sample_rate)
    pole = params["alpha"] / (1 - (1 - params["alpha"]) * delay)
    response = (1 + params["feedback"]) * pole ** 4 / (1 + params["feedback"] * delay * pole ** 4)
    measured = complex(result["coherent_real"], result["coherent_imag"])
    linear_error = abs(measured - args.amplitude * response)
    assert result["status"] == "periodic" and linear_error < 2e-9, (result, response)
    assert abs(result["total_power"] - abs(args.amplitude * response) ** 2 / 2) < 2e-10
    return dict(maximum_step_error=worst, linear_coherent_error=linear_error,
                linear_period=period, linear_status=result["status"])


def sweep_pair(library, fi, ci, frequencies, cutoffs, args):
    target_frequency, cutoff = float(frequencies[fi]), float(cutoffs[ci])
    cycles, period, actual, tones = periodic_frequency(target_frequency, args.sample_rate,
                                                      args.frequency_tolerance)
    completed = []
    for gi, drive in enumerate(np.linspace(0, 1, args.size)):
        for ki, resonance in enumerate(np.linspace(0, 1, args.size)):
            params = parameters(float(drive), float(resonance), cutoff, args.sample_rate)
            seed, seed_info = warm_start(completed, params)
            result, state = run_point(library, params, tones, seed, args)
            row = dict(id=f"f{fi:02d}-c{ci:02d}-g{gi:02d}-r{ki:02d}",
                frequency_index=fi, cutoff_index=ci, drive_index=gi, resonance_index=ki,
                target_frequency_hz=target_frequency, frequency_hz=actual, cutoff_hz=cutoff,
                frequency_error_relative=actual / target_frequency - 1,
                period_samples=period, input_cycles=cycles, amplitude=args.amplitude,
                drive_knob=float(drive), resonance_knob=float(resonance),
                **params, **seed_info, **result)
            completed.append((row, state))
    return [row for row, _ in completed]


def summarize(rows, elapsed, validation):
    periodic = [row for row in rows if row["status"] == "periodic"]
    def quantiles(key):
        return {str(p): float(np.percentile([row[key] for row in periodic], p))
                for p in (0, 50, 90, 99, 100)} if periodic else {}
    counts = {}
    for row in rows:
        counts[row["status"]] = counts.get(row["status"], 0) + 1
    return dict(rows=len(rows), counts=counts, elapsed_wall_seconds=elapsed,
        total_native_thread_cpu_seconds=sum(row["compute_cpu_seconds"] for row in rows),
        settling_audio_seconds_percentiles=quantiles("settling_audio_seconds"),
        settling_periods_percentiles=quantiles("settling_periods"),
        maximum_accepted_verification_score=max((row["verification_defect_score"] for row in periodic), default=None),
        validation=validation,
        by_drive=[dict(drive_knob=drive, periodic=sum(row["status"] == "periodic" for row in rows
                      if row["drive_knob"] == drive), total=sum(row["drive_knob"] == drive for row in rows))
                  for drive in sorted({row["drive_knob"] for row in rows})])


def refine_rejections(library, directory):
    """Spend the remaining budget on rows rejected by the extra verification.

    Retain the original table and an attempt log. Downstream grid points keep
    their original warm starts; this is a separate continuation of rejected rows.
    """
    metadata = json.loads((directory / "metadata.json").read_text())
    args = argparse.Namespace(**metadata["arguments"])
    rows = [json.loads(line) for line in (directory / "table.jsonl").read_text().splitlines()]
    summary = json.loads((directory / "summary.json").read_text())
    original = directory / "initial-table.jsonl"
    if original.exists():
        raise ValueError("Rejections have already been refined in this directory")
    original.write_text((directory / "table.jsonl").read_text())
    (directory / "initial-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    budget = args.seconds
    started = time.perf_counter()

    def refine(row):
        attempts = []
        spent_samples = row["settling_samples"]
        original_maximum = row["maximum_periods"]
        cpu, wall = row["compute_cpu_seconds"], row["compute_wall_seconds"]
        period = row["period_samples"]
        phase = 2 * np.pi * ((np.arange(period) * row["input_cycles"]) % period) / period
        tones = np.ascontiguousarray(np.column_stack((np.cos(phase), np.sin(phase))))
        while row["status"] == "verification_failed":
            spent_samples += row["measurement_samples"]
            remaining = budget - spent_samples / args.sample_rate
            if remaining <= 0:
                break
            local = argparse.Namespace(**vars(args))
            local.seconds = remaining
            state = np.array([row[f"state_{j}"] for j in range(1, 5)])
            result, _ = run_point(library, row, tones, state, local)
            if result["first_passing_period"]:
                result["first_passing_period"] += spent_samples // period
            spent_samples += result["settling_samples"]
            cpu += result["compute_cpu_seconds"]
            wall += result["compute_wall_seconds"]
            attempts.append(dict(status=result["status"], settling_samples=result["settling_samples"],
                                 measurement_samples=result["measurement_samples"],
                                 verification_defect_score=result["verification_defect_score"]))
            row.update(result)
        row.update(settling_samples=spent_samples, settling_periods=spent_samples // period,
                   settling_audio_seconds=spent_samples / args.sample_rate,
                   compute_cpu_seconds=cpu, compute_wall_seconds=wall,
                   refinement_attempts=len(attempts), maximum_periods=original_maximum)
        return dict(id=row["id"], attempts=attempts, final_status=row["status"])

    rejected = [row for row in rows if row["status"] == "verification_failed"]
    for row in rows:
        row["refinement_attempts"] = 0
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        log = list(pool.map(refine, rejected))
    elapsed = time.perf_counter() - started
    (directory / "verification-refinement.json").write_text(json.dumps(log, indent=2) + "\n")
    (directory / "table.jsonl").write_text("".join(json.dumps(row, allow_nan=False) + "\n" for row in rows))
    with (directory / "table.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(sorted(rows, key=lambda row: row["id"]))
    summary = summarize(rows, summary["elapsed_wall_seconds"] + elapsed, summary["validation"])
    summary["verification_refinement"] = dict(rows=len(rejected), wall_seconds=elapsed,
        recovered=sum(item["final_status"] == "periodic" for item in log))
    (directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    metadata["verification_refinement"] = dict(
        script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        note="Rejected rows continued from their measurement endpoints with the remaining audio budget. Settling samples include earlier failed verification windows; minimum observation blocks can slightly exceed the nominal budget.")
    (directory / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (directory / "refine_ladder_periodic_grid.py").write_bytes(Path(__file__).read_bytes())
    print(json.dumps(summary, indent=2), flush=True)


def check_multiples(library, directory):
    """Check whole waveforms for the 2..8-period candidates seen at endpoints."""
    metadata = json.loads((directory / "metadata.json").read_text())
    args = argparse.Namespace(**metadata["arguments"])
    rows = [json.loads(line) for line in (directory / "table.jsonl").read_text().splitlines()]
    candidates = [row for row in rows if row["status"] != "periodic" and row["endpoint_lag_score"] <= 1]
    def check(row):
        lag = row["endpoint_best_lag"]
        period = row["period_samples"]
        phase = 2 * np.pi * ((np.arange(period) * row["input_cycles"]) % period) / period
        tones = np.ascontiguousarray(np.tile(np.column_stack((np.cos(phase), np.sin(phase))), (lag, 1)))
        state = np.array([row[f"state_{j}"] for j in range(1, 5)])
        result, _ = run_point(library, row, tones, state, args)
        return dict(id=row["id"], multiplier=lag, original_period_samples=period,
                    tested_period_samples=lag * period, **result)
    started = time.perf_counter()
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        results = list(pool.map(check, candidates))
    record = dict(candidate_rows=len(candidates),
        periodic=sum(row["status"] == "periodic" for row in results),
        wall_seconds=time.perf_counter() - started,
        note="Supplementary whole-waveform checks of endpoint-suggested multiples only; not an exhaustive search of longer periods. Main table status remains the original q-period test.",
        script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), rows=results)
    (directory / "multiple-period-checks.json").write_text(json.dumps(record, indent=2) + "\n")
    (directory / "check_ladder_periodic_multiples.py").write_bytes(Path(__file__).read_bytes())
    print(json.dumps({key: value for key, value in record.items() if key != "rows"}), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=10)
    parser.add_argument("--seconds", type=float, default=30.0, help="Settling budget in audio seconds per row")
    parser.add_argument("--sample-rate", type=float, default=192000.0)
    parser.add_argument("--amplitude", type=float, default=0.25)
    parser.add_argument("--frequency-tolerance", type=float, default=0.001)
    parser.add_argument("--absolute-tolerance", type=float, default=1e-12)
    parser.add_argument("--relative-tolerance", type=float, default=1e-9)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--pair", type=int, nargs=2, help="Run one frequency/cutoff pair for a pilot")
    parser.add_argument("--refine-rejections", action="store_true",
                        help="Continue marginal verification failures in an existing completed table")
    parser.add_argument("--check-multiples", action="store_true",
                        help="Check the longer-period candidates in an existing completed table")
    parser.add_argument("--output", type=Path, default=ROOT / "build/results/ladder-periodic-grid")
    args = parser.parse_args()
    assert args.size >= 2 and args.seconds > 0 and args.sample_rate > 40000
    assert args.amplitude > 0 and args.workers > 0
    assert 0 < args.frequency_tolerance < 0.01
    args.output.mkdir(parents=True, exist_ok=True)
    library = load_library()
    if args.refine_rejections:
        refine_rejections(library, args.output)
        return
    if args.check_multiples:
        check_multiples(library, args.output)
        return
    if (args.output / "table.jsonl").exists():
        raise FileExistsError("Results already exist; choose a different --output directory to retain the earlier experiment")
    validation = validate(library, args)
    print(json.dumps({"validation": validation}), flush=True)
    frequencies = np.geomspace(20, 20000, args.size)
    cutoffs = np.geomspace(20, 20000, args.size)
    metadata = dict(arguments={key: str(value) if isinstance(value, Path) else value
                               for key, value in vars(args).items()},
        model="binary64 real tanh, detector disabled, retained Lean kSafe coefficient cap",
        saturation="tanh(g*x)/tanh(g)",
        drive_mapping="g=0.25*20^knob", resonance_mapping="requested_k=3.9*(49^knob-1)/48",
        frequency_targets_hz=frequencies.tolist(), cutoff_values_hz=cutoffs.tolist(),
        input_phase="cosine, phase zero at every period boundary",
        acceptance="8 consecutive whole-waveform state repeat checks, then >=8 further periods and >=0.1 audio seconds",
        norm=f"max over stages of max sample delta / ({args.absolute_tolerance} + {args.relative_tolerance} * max absolute history)",
        timing="Audio settling time excludes measurement; CPU and wall compute time include measurement. Multiple independent frequency/cutoff pairs run concurrently.",
        limitation="Numerical recurrence evidence, not exact fixed-point certification or proof of attraction. Timeouts are not nonexistence. Timeout powers are finite-window measurements.",
        library_sha256=hashlib.sha256(LIBRARY.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        source_sha256=hashlib.sha256(Path(__file__).with_name("ladder_periodic_grid.cpp").read_bytes()).hexdigest())
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    pairs = [tuple(args.pair)] if args.pair else [(fi, ci) for fi in range(args.size) for ci in range(args.size)]
    started = time.perf_counter()
    rows = []
    with (args.output / "table.jsonl").open("w") as stream, ThreadPoolExecutor(max_workers=args.workers) as pool:
        jobs = {pool.submit(sweep_pair, library, fi, ci, frequencies, cutoffs, args): (fi, ci)
                for fi, ci in pairs}
        for job in as_completed(jobs):
            batch = job.result()
            rows.extend(batch)
            for row in batch:
                stream.write(json.dumps(row, allow_nan=False) + "\n")
            stream.flush()
            count = sum(row["status"] == "periodic" for row in batch)
            print(json.dumps(dict(pair=jobs[job], completed=len(rows), periodic_in_pair=count,
                                  seconds=round(time.perf_counter() - started, 2))), flush=True)
    rows.sort(key=lambda row: row["id"])
    with (args.output / "table.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    summary = summarize(rows, time.perf_counter() - started, validation)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == "__main__":
    main()

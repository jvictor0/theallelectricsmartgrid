#!/usr/bin/env python3
"""Empirical basin survey for one detector-disabled tanh ladder setting.

All states start at the same forcing phase. Compare the full four-state
stroboscopic trajectories with a previously observed drifting loop, allowing
different phase along that loop. This is finite-time evidence, not a basin or
uniqueness proof. Existing app, Lean, and generated DSP remain untouched.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
import itertools
import json
import math
from pathlib import Path
import time

import numpy as np
from scipy.signal import find_peaks
from scipy.spatial import cKDTree
from scipy.stats import qmc

from classify_ladder_periodic_grid import load_trace_library, tones_for, trace, validate


ROOT = Path(__file__).resolve().parents[1]


class ReferenceLoop:
    """Piecewise-linear interpolation in all four normalized state coordinates."""

    def __init__(self, states, scale):
        self.scale = scale
        points = states / scale
        self.center = np.mean(points, axis=0)
        covariance = np.cov(points.T)
        eigenvalues, eigenvectors = np.linalg.eigh(covariance)
        self.projection = eigenvectors[:, -2:]
        plane = (points - self.center) @ self.projection / np.sqrt(eigenvalues[-2:])
        phase = np.arctan2(plane[:, 1], plane[:, 0])
        order = np.argsort(phase)
        self.points = points[order]
        self.tree = cKDTree(self.points)
        self.max_angular_gap = float(np.max(np.diff(np.r_[phase[order], phase[order][0] + 2*np.pi])))
        assert self.max_angular_gap < 0.05, self.max_angular_gap

    def distance(self, states):
        points = np.atleast_2d(states) / self.scale
        _, nearest = self.tree.query(points)
        distances = []
        for offset in [-1, 0]:
            start = self.points[(nearest + offset) % len(self.points)]
            end = self.points[(nearest + offset + 1) % len(self.points)]
            direction = end - start
            length_squared = np.sum(direction * direction, axis=1)
            assert np.all(length_squared > 0)
            fraction = np.clip(np.sum((points - start) * direction, axis=1) / length_squared, 0, 1)
            distances.append(np.linalg.norm(points - start - fraction[:, None] * direction, axis=1))
        return np.minimum(*distances)


def seeds_for(reference, root_state, scale, sobol_power):
    seeds = []

    def add(family, values, detail):
        seeds.append(dict(id=len(seeds), family=family, detail=detail, state=np.asarray(values, dtype=float)))

    add('reset', np.zeros(4), 'zero histories')
    add('periodic_root', root_state, 'binary64 shooting root, no intentional perturbation')
    for index, values in enumerate(qmc.Sobol(4, scramble=True, seed=20261119).random_base2(sobol_power)):
        add('bounded_sobol', (2*values-1)*scale, str(index))
    for values in itertools.product([-1, 1], repeat=4):
        add('box_corners', np.array(values)*scale, str(values))
    for axis in range(4):
        for sign in [-1, 1]:
            add('box_axes', sign*np.eye(4)[axis]*scale, f'axis={axis}, sign={sign}')
    for index in np.linspace(0, len(reference)-1, 32, dtype=int):
        add('drifting_loop', reference[index], f'reference index={index}')
    rng = np.random.default_rng(620411)
    directions = rng.normal(size=(12, 4))
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    for epsilon in [1e-14, 1e-12, 1e-9, 1e-6, 1e-3, 0.1]:
        for index, direction in enumerate(directions):
            for sign in [-1, 1]:
                add('near_periodic_root', root_state + sign*epsilon*direction*scale,
                    f'epsilon={epsilon:g}, direction={index}, sign={sign}')
    for multiplier in [10, 1000]:
        for index in range(32):
            direction = rng.normal(size=4)
            direction /= np.linalg.norm(direction)
            add('large_states', multiplier*direction*scale, f'radius={multiplier}, direction={index}')
    for epsilon in [1e-12, 1e-10, 1e-8, 1e-6, 1e-4, 1e-2]:
        for index in range(8):
            direction = rng.normal(size=4)
            direction /= np.linalg.norm(direction)
            add('near_reset', epsilon*direction*scale, f'epsilon={epsilon:g}, direction={index}')
    return seeds


def spectrum_features(output, sample_rate):
    window = np.hanning(len(output))
    amplitude = 2 * abs(np.fft.rfft((output - np.mean(output))*window)) / np.sum(window)
    candidates, _ = find_peaks(amplitude, height=np.max(amplitude)*1e-4)
    strongest = candidates[np.argsort(amplitude[candidates])[-6:][::-1]]
    peaks = []
    for j in strongest:
        left, center, right = np.log(np.maximum(amplitude[j-1:j+2], 1e-300))
        shift = 0.5*(left-right)/(left-2*center+right)
        peaks.append([(j+shift)*sample_rate/len(output), float(amplitude[j])])
    return peaks


def investigate(seed, library, row, tones, loop, args, duration=None):
    fs, q = 192000.0, len(tones)
    seconds = duration if duration is not None else args.seconds
    periods = math.ceil(seconds*fs/q)
    state = seed['state'].copy()
    endpoints, features, output = trace(library, row, tones, periods, state, np.full(4, 0.5))
    distances = loop.distance(endpoints)
    tail_start = periods//3 if duration is None else periods//2
    tail = features[tail_start:]
    distances_tail = distances[tail_start:]
    curve_match = float(np.max(distances_tail)) <= args.distance_tolerance
    periodic = float(np.max(tail[-8:, 4])) <= 1.0
    classification = 'same_drifting_loop' if curve_match else ('input_periodic' if periodic else 'needs_longer_check')
    outside = np.flatnonzero(distances > args.distance_tolerance)
    entered = None if (len(outside) and outside[-1] == periods) else (int(outside[-1])+1)*q/fs if len(outside) else 0.0
    window_output = output[tail_start*q:]
    peaks = spectrum_features(window_output, fs)
    half = len(tail)//2
    blocks = [np.mean(block[:, 0]) for block in np.array_split(tail, 4)]
    record = dict(id=seed['id'], family=seed['family'], detail=seed['detail'],
        initial_state=seed['state'].tolist(), final_state=state.tolist(),
        classification=classification, simulated_seconds=periods*q/fs,
        measurement_seconds=len(tail)*q/fs, power=float(np.mean(tail[:, 0])),
        coherent_peak_amplitude=float(np.linalg.norm(np.mean(tail[:, 1:3], axis=0))),
        coherent_complex=np.mean(tail[:, 1:3], axis=0).tolist(),
        power_blocks=blocks, power_half_difference=float(np.mean(tail[half:, 0])-np.mean(tail[:half, 0])),
        max_tail_curve_distance=float(np.max(distances_tail)),
        rms_tail_curve_distance=float(np.sqrt(np.mean(distances_tail**2))),
        entered_observed_tube_seconds=entered,
        tangent_growth_per_second=float(np.sum(tail[:, 3])/(len(tail)*q/fs)),
        final_q_repeat_score=float(np.max(tail[-8:, 4])),
        dominant_frequency_hz=float(peaks[0][0]), spectral_peaks=peaks)
    indices = np.unique(np.r_[np.arange(min(81, len(endpoints))),
        np.linspace(0, periods, 81, dtype=int)])
    trajectory = dict(indices=indices, distances=distances[indices],
        tail_states=endpoints[-32:], final_state=state)
    return record, trajectory


def check_long_windows(seeds, records, library, row, tones, loop, args):
    """Recheck reset, the root, and extremes of the finite-window observations."""
    chosen = {0, 1}
    for key in ['power', 'coherent_peak_amplitude', 'max_tail_curve_distance']:
        chosen.add(min(records, key=lambda r: r[key])['id'])
        chosen.add(max(records, key=lambda r: r[key])['id'])
    chosen.add(max(records, key=lambda r: r['entered_observed_tube_seconds'] or 0)['id'])
    chosen.add(max(seeds, key=lambda s: np.linalg.norm(s['state']/loop.scale))['id'])
    results = []
    for index in sorted(chosen):
        record, _ = investigate(seeds[index], library, row, tones, loop, args, duration=64.0)
        results.append(record)
        print(json.dumps(dict(long_check=index, classification=record['classification'], power=record['power'])), flush=True)
    (args.output/'long-checks.json').write_text(json.dumps(results, indent=2)+'\n')
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'build/results/ladder-periodic-case')
    parser.add_argument('--output', type=Path, default=ROOT/'build/results/ladder-initial-conditions')
    parser.add_argument('--sobol-power', type=int, default=12)
    parser.add_argument('--seconds', type=float, default=6.0)
    parser.add_argument('--distance-tolerance', type=float, default=1e-4)
    parser.add_argument('--workers', type=int, default=4)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source = json.loads((args.source/'analysis.json').read_text())
    row = source['case']
    data = np.load(args.source/'traces.npz')
    reference = data['endpoints']
    saturation_bound = 1/math.tanh(row['gain'])
    scale = np.array([row['amplitude'] + row['feedback']*saturation_bound] + [saturation_bound]*3)
    loop = ReferenceLoop(reference, scale)
    held_out_distances = ReferenceLoop(reference[::2], scale).distance(reference[1::2])
    assert np.max(held_out_distances) < args.distance_tolerance/10, np.max(held_out_distances)
    seeds = seeds_for(reference, np.array(source['periodic']['state']), scale, args.sobol_power)
    library, tones = load_trace_library(), tones_for(row)
    validation = validate(library)
    metadata = dict(case=row, seed_count=len(seeds), normalized_state_scale=scale.tolist(),
        model='detector-disabled real tanh, binary64', seconds=args.seconds,
        measurement='last two thirds, at fixed input phase', workers=args.workers,
        family_counts={family:sum(seed['family']==family for seed in seeds) for family in sorted({s['family'] for s in seeds})},
        tube_distance_tolerance=args.distance_tolerance,
        reference_held_out_max_distance=float(np.max(held_out_distances)),
        reference_max_angular_gap=loop.max_angular_gap,
        native_validation=validation)
    (args.output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print(json.dumps(metadata), flush=True)
    records, trajectories = {}, {}
    started = time.perf_counter()
    with (args.output/'observations.jsonl').open('w') as stream:
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            jobs = {pool.submit(investigate, seed, library, row, tones, loop, args):seed for seed in seeds}
            for job in as_completed(jobs):
                record, trajectory = job.result()
                records[record['id']], trajectories[record['id']] = record, trajectory
                stream.write(json.dumps(record)+'\n')
                stream.flush()
                if len(records) % 256 == 0 or len(records) == len(seeds):
                    print(json.dumps(dict(completed=len(records), total=len(seeds),
                        elapsed_seconds=time.perf_counter()-started,
                        classifications={kind:sum(r['classification']==kind for r in records.values())
                            for kind in sorted({r['classification'] for r in records.values()})})), flush=True)
    outliers = [r for r in records.values() if r['classification'] != 'same_drifting_loop']
    for old in outliers:
        result, _ = investigate(seeds[old['id']], library, row, tones, loop, args, duration=64.0)
        (args.output/f'extended-{old["id"]}.json').write_text(json.dumps(result, indent=2)+'\n')
        records[old['id']] = result
        print(json.dumps(dict(extended=result['id'], classification=result['classification'])), flush=True)
    ordered = [records[j] for j in range(len(seeds))]
    (args.output/'final-observations.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in ordered))
    first = trajectories[0]
    np.savez_compressed(args.output/'states.npz', initial=np.array([s['state'] for s in seeds]),
        final=np.array([r['final_state'] for r in ordered]),
        tail_states=np.array([trajectories[j]['tail_states'] for j in range(len(seeds))]),
        sampled_period_indices=first['indices'],
        sampled_curve_distances=np.array([trajectories[j]['distances'] for j in range(len(seeds))]),
        reference=reference, scale=scale)
    fields = ['id', 'family', 'detail', 'classification', 'simulated_seconds', 'power',
        'coherent_peak_amplitude', 'dominant_frequency_hz', 'max_tail_curve_distance',
        'entered_observed_tube_seconds', 'tangent_growth_per_second', 'final_q_repeat_score']
    with (args.output/'results.csv').open('w') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(ordered)
    long_checks = check_long_windows(seeds, ordered, library, row, tones, loop, args)
    summary = dict(seed_count=len(seeds),
        classifications={kind:sum(r['classification']==kind for r in ordered) for kind in sorted({r['classification'] for r in ordered})},
        ranges={key:[min(r[key] for r in ordered), max(r[key] for r in ordered)]
                for key in ['power','coherent_peak_amplitude','dominant_frequency_hz','max_tail_curve_distance']},
        entrance_time_quantiles={str(p):float(np.quantile([r['entered_observed_tube_seconds'] for r in ordered
            if r['entered_observed_tube_seconds'] is not None], p)) for p in [0, 0.5, 0.9, 0.99, 1]},
        family_counts=metadata['family_counts'], refinements=len(outliers),
        long_check_count=len(long_checks),
        long_check_classifications=sorted({r['classification'] for r in long_checks}),
        long_check_power_range=[min(r['power'] for r in long_checks), max(r['power'] for r in long_checks)],
        total_compute_seconds=time.perf_counter()-started)
    (args.output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary), flush=True)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Read complete records from the resumable SIMD ladder energy survey.

Supports a running output directory: an incomplete trailing record is ignored,
and existing records are immutable. The small validation mode independently
replays zero-feedback cases through the older scalar research recurrence.
"""

import argparse
import ctypes
import json
import math
from pathlib import Path
import struct

import numpy as np


MASK = (1 << 64) - 1
FORMATS = {1: (0x314752454444414c, 52), 2: (0x324752454444414c, 216), 3: (0x334752454444414c, 216)}


def mix(value):
    value = (value + 0x9e3779b97f4a7c15) & MASK
    value = ((value ^ (value >> 30))*0xbf58476d1ce4e5b9) & MASK
    value = ((value ^ (value >> 27))*0x94d049bb133111eb) & MASK
    return value ^ (value >> 31)


def uniform(key):
    return (mix(key) >> 11)*2**-53


def checksum(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte)*1099511628211) & MASK
    return value


def chunks(folder, size=8192):
    manifest = json.loads((folder/'manifest.json').read_text())
    magic_expected, fields = FORMATS[manifest['format']]
    dtype = np.dtype([('job', '<u8'), ('points', '<f4', (4, fields)), ('checksum', '<u8')])
    assert dtype.itemsize == manifest['record_bytes']
    identity = None
    for path in sorted(folder.glob('worker-*.bin')):
        if path.stat().st_size < 24:
            continue
        count = (path.stat().st_size-24)//dtype.itemsize
        with path.open('rb') as stream:
            magic, fingerprint, record_size = struct.unpack('<QQQ', stream.read(24))
        assert magic == magic_expected and record_size == dtype.itemsize, path
        if identity is None:
            identity = fingerprint
        assert fingerprint == identity, path
        if count <= 0:
            continue
        values = np.memmap(path, dtype=dtype, mode='r', offset=24, shape=(count,))
        for offset in range(0, count, size):
            yield values[offset:offset+size]


def indices(jobs, grid):
    group = jobs.astype(np.int64)
    cutoff = 4*(group % (grid//4))
    group //= grid//4
    resonance = group % grid
    group //= grid
    drive = group % grid
    frequency = group//grid
    return frequency, drive, resonance, cutoff


def summarize(folder, manifest):
    counts = dict(points=0, across_starts_cv_below_1pct=0, across_starts_cv_below_5pct=0,
                  mean_temporal_cv_below_1pct=0, mean_temporal_cv_below_5pct=0)
    groups = {}
    worst = []
    spectral = {}
    for block in chunks(folder):
        values = block['points'].reshape(-1, FORMATS[manifest['format']][1]).astype(np.float64)
        assert np.all(np.isfinite(values[:, :12]))
        mean, sd = values[:, 4], values[:, 5]
        cv = np.divide(sd, mean, out=np.zeros_like(mean), where=mean != 0)
        temporal = values[:, 8]
        counts['points'] += len(values)
        counts['across_starts_cv_below_1pct'] += int(np.sum(cv < .01))
        counts['across_starts_cv_below_5pct'] += int(np.sum(cv < .05))
        counts['mean_temporal_cv_below_1pct'] += int(np.sum(temporal < .01))
        counts['mean_temporal_cv_below_5pct'] += int(np.sum(temporal < .05))
        if manifest['format'] >= 2:
            assert np.all(np.isfinite(values[:, :136]))
            scopes = values[:, 40:136].reshape(-1, 4, 6, 4)
            fundamental = scopes[:, 3, 1, :]
            fundamental_cv = np.divide(fundamental[:, 1], fundamental[:, 0],
                out=np.zeros_like(mean), where=fundamental[:, 0] != 0)
            measures = {
                'fundamental_across_starts_cv_below_1pct': fundamental_cv < .01,
                'fundamental_across_starts_cv_below_5pct': fundamental_cv < .05,
                'mean_fundamental_temporal_cv_below_1pct': values[:, 12] < .01,
                'mean_fundamental_temporal_cv_below_5pct': values[:, 12] < .05,
                'mean_fundamental_coherence_above_99pct': values[:, 16] > .99,
                'mean_total_prefix_drift_below_1pct_input_power': values[:, 28] < .01,
                'mean_fundamental_prefix_drift_below_1pct_input_power': values[:, 32] < .01,
                'mean_coherent_prefix_drift_below_1pct_input_amplitude': values[:, 36] < .01,
            }
            for name, mask in measures.items():
                spectral[name] = spectral.get(name, 0) + int(np.sum(mask))
        f, d, r, c = indices(block['job'], manifest['grid'])
        for label, selected in [('drive_res_at_most_80pct', (d <= .8*(manifest['grid']-1)) & (r <= .8*(manifest['grid']-1))),
                                ('drive_or_res_above_80pct', (d > .8*(manifest['grid']-1)) | (r > .8*(manifest['grid']-1)))]:
            mask = np.repeat(selected, 4)
            item = groups.setdefault(label, dict(points=0, across_starts_cv_below_1pct=0, mean_temporal_cv_below_1pct=0))
            item['points'] += int(np.sum(mask))
            item['across_starts_cv_below_1pct'] += int(np.sum(cv[mask] < .01))
            item['mean_temporal_cv_below_1pct'] += int(np.sum(temporal[mask] < .01))
        take = np.argsort(cv)[-20:]
        for index in take:
            job_index, lane = divmod(int(index), 4)
            worst.append(dict(job=int(block['job'][job_index]), lane=lane,
                frequency_index=int(f[job_index]), drive_index=int(d[job_index]),
                resonance_index=int(r[job_index]), cutoff_index=int(c[job_index])+lane,
                late_energy_mean=float(mean[index]), across_starts_cv=float(cv[index]),
                mean_temporal_cv=float(temporal[index])))
        worst = sorted(worst, key=lambda row: row['across_starts_cv'], reverse=True)[:20]
    selected_points = manifest.get('selected_points', manifest['grid']**4)
    return dict(status='complete' if counts['points'] == selected_points else 'partial',
                total_points=selected_points, full_grid_points=manifest['grid']**4,
                minimum_effective_resonance=manifest.get('min_effective_resonance', -1), counts=counts, groups=groups,
                spectral_counts=spectral,
                worst_across_start_variation=worst,
                caveat='Finite observations, not asymptotic convergence proofs. Partial grid ordering is not a random sample. Relative CV is ill-conditioned near zero fundamental power; use absolute prefix drift too.')


def signal_metrics(output, tones):
    coherent = 2*np.mean(output[:, None]*tones, axis=0)*np.array([1, -1])
    total = np.mean(output**2)
    fundamental = np.dot(coherent, coherent)/2
    dc = np.mean(output)
    residual = total-fundamental-dc**2
    assert residual >= -1e-9*total
    return np.array([total, fundamental, coherent[0], coherent[1], dc, max(0, residual)])


def trial_measurements(output, tones, frequency, amplitude, version):
    count = len(output)
    length, windows = frequency['window_samples'], frequency['complete_windows']
    window = np.array([signal_metrics(output[b*length:(b+1)*length],
                      tones[b*length:(b+1)*length]) for b in range(windows)])
    late = window[frequency['first_late_window']:]
    cv = lambda values: np.std(values)/np.mean(values) if np.mean(values) else 0
    aggregates = [np.mean(output**2), np.mean(output[count//2:]**2), cv(late[:, 0])]
    if version == 2:
        scopes = np.array([signal_metrics(output[begin:end], tones[begin:end])
                           for begin, end in frequency['scopes']])
        begin, end = frequency['scopes'][3]
        peak = np.max(np.abs(output[begin:end]))
        coherence = np.sum(np.mean(late[:, 2:4], axis=0)**2)/(2*np.mean(late[:, 1])) if np.mean(late[:, 1]) else 0
        aggregates += [cv(late[:, 1]), min(1, coherence), peak,
                       peak/np.sqrt(scopes[3, 0]) if scopes[3, 0] else 0,
                       abs(scopes[2, 0]-scopes[1, 0])/(amplitude**2/2),
                       abs(scopes[2, 1]-scopes[1, 1])/(amplitude**2/2),
                       np.linalg.norm(scopes[2, 2:4]-scopes[1, 2:4])/amplitude]
        aggregates += list(scopes.ravel())
    return np.array(aggregates), window


def expected_record(trials, windows, version):
    aggregates = np.column_stack((trials.mean(axis=0), trials.std(axis=0),
                                  trials.min(axis=0), trials.max(axis=0))).ravel()
    result = list(aggregates)
    for metric in ([0, 1] if version == 2 else [0]):
        for stat in (np.mean, np.std):
            values = stat(windows[:, :, metric], axis=0)
            result += list(values) + [np.nan]*(20-len(values))
    return np.array(result)


def validate_scalar(folder, manifest):
    assert manifest['format'] in (1, 2), 'Real-tanh replay does not validate the rational model; use validate_ladder_gpu_grid.py'
    from analyze_ladder_periodic_grid import load_library, parameters
    library = load_library()
    result = []
    count = manifest['samples']
    for block in chunks(folder):
        f, d, r, c = indices(block['job'], manifest['grid'])
        for index in range(len(block)):
            record = block[index]
            assert checksum(record.tobytes()[:-8]) == int(record['checksum'])
            if r[index] != 0:
                continue
            frequency = manifest['frequencies'][f[index]]
            phase = 2*np.pi*((np.arange(count)*frequency['input_cycles']) % frequency['period_samples'])/frequency['period_samples']
            tones = np.ascontiguousarray(np.column_stack((np.cos(phase), np.sin(phase))))
            length = frequency['window_samples']
            windows = frequency['complete_windows']
            for lane in range(4):
                cutoff = 20*math.pow(1000, (int(c[index])+lane)/(manifest['grid']-1))
                params = parameters(int(d[index])/(manifest['grid']-1), 0, cutoff, manifest['sample_rate'])
                bounds = np.array([manifest['amplitude']] + [1/math.tanh(params['gain'])]*3)
                trial_values, window_values = [], []
                for trial in range(manifest['starts']):
                    key = mix(manifest['seed'] ^ mix(int(record['job'])*4+lane) ^ mix(0x100000000+trial))
                    state = np.array([(2*uniform((key+stage)&MASK)-1)*bounds[stage] for stage in range(4)])
                    histories = np.empty((count, 4))
                    library.LadderGridCycle(np.array([params['alpha'], 0, params['gain']]),
                        tones, count, manifest['amplitude'], state, histories)
                    output = np.tanh(params['gain']*histories[:, 3])/math.tanh(params['gain'])
                    trial, window = trial_measurements(output, tones, frequency,
                                                       manifest['amplitude'], manifest['format'])
                    trial_values.append(trial)
                    window_values.append(window)
                trials = np.array(trial_values)
                expected = expected_record(trials, np.array(window_values), manifest['format'])
                actual = record['points'][lane].astype(float)
                assert np.array_equal(np.isnan(actual), np.isnan(expected))
                valid = np.isfinite(expected)
                # All summaries are rounded to float32. An absolute allowance
                # covers cancellation in tiny DC, residuals and near-zero CVs.
                tolerance = 2e-6*np.abs(expected)+1e-10
                wrong = np.flatnonzero(valid & (np.abs(actual-expected) > tolerance))
                assert not len(wrong), (int(record['job']), lane,
                                       [(int(i), actual[i], expected[i]) for i in wrong])
                result.append(dict(job=int(record['job']), lane=lane,
                    max_energy_relative_error=float(np.max(abs(actual[:8]-expected[:8])/np.repeat([expected[0],expected[4]],4))),
                    max_summary_absolute_error=float(np.max(abs(actual[valid]-expected[valid]))),
                    compared_fields=int(np.sum(valid))))
    assert result, 'No zero-feedback validation points present'
    return dict(status='passed', scalar_points=len(result), trials_per_point=manifest['starts'], comparisons=result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--validate-scalar', action='store_true')
    args = parser.parse_args()
    manifest = json.loads((args.folder/'manifest.json').read_text())
    result = validate_scalar(args.folder, manifest) if args.validate_scalar else summarize(args.folder, manifest)
    path = args.folder/('scalar-validation.json' if args.validate_scalar else 'summary.json')
    path.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

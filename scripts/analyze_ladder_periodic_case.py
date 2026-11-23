#!/usr/bin/env python3
"""Compare a drifting ladder trajectory with a directly solved periodic orbit.

Research only: binary64 Newton shooting and finite-window spectra, not an
existence certificate. The detector is disabled and saturation is real tanh.
The periodic spectrum repeats one computed cycle, since an unstable orbit
cannot be followed indefinitely by ordinary floating-point forward iteration.
"""

import argparse
import json
import math
from pathlib import Path

import numpy as np
from scipy.optimize import root
from scipy.signal import find_peaks

from analyze_ladder_attraction import sample_with_jacobian
from analyze_ladder_harmonic_reduction import Model
from classify_ladder_periodic_grid import load_trace_library, tones_for, trace


ROOT = Path(__file__).resolve().parents[1]


def shoot(model, forcing, initial):
    state, jacobian = initial.copy(), np.eye(4)
    for value in forcing:
        state, step = sample_with_jacobian(model, state, value)
        jacobian = step @ jacobian
    return state - initial, jacobian - np.eye(4)


def linear_seed(model, amplitude, frequency):
    _, matrix = sample_with_jacobian(model, np.zeros(4), 0.0)
    slope = model.g / math.tanh(model.g)
    input_vector = np.array([model.a ** (j + 1) * slope ** j for j in range(4)])
    return np.linalg.solve(np.exp(2j * np.pi * frequency / model.fs) * np.eye(4)
                           - matrix, amplitude * input_vector).real


def native_period(library, row, tones, initial):
    state = initial.copy()
    states = np.empty((len(tones), 4))
    library.LadderGridCycle(np.array([row['alpha'], row['feedback'], row['gain']]),
        tones, len(tones), row['amplitude'], state, states)
    output = (1 + row['feedback']) * np.tanh(row['gain'] * states[:, 3]) / math.tanh(row['gain'])
    return state, states, output


def peaks_of(signal, sample_rate):
    window = np.hanning(len(signal))
    amplitudes = 2 * np.abs(np.fft.rfft((signal - np.mean(signal)) * window)) / np.sum(window)
    bins, _ = find_peaks(amplitudes)
    strongest = sorted(bins, key=lambda j: amplitudes[j], reverse=True)[:18]
    peaks = []
    for j in strongest:
        left, center, right = np.log(np.maximum(amplitudes[j-1:j+2], 1e-300))
        shift = 0.5 * (left - right) / (left - 2 * center + right)
        peaks.append(dict(frequency_hz=float((j + shift) * sample_rate / len(signal)),
                          bin_peak_amplitude=float(amplitudes[j]),
                          bin_amplitude_db=float(20 * np.log10(amplitudes[j]))))
    return amplitudes, peaks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', default='f03-c05-g06-r06')
    parser.add_argument('--seconds', type=float, default=16.0)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/results/ladder-periodic-case')
    args = parser.parse_args()
    row = next(r for r in map(json.loads, (ROOT / 'build/results/ladder-periodic-grid/table.jsonl').read_text().splitlines())
               if r['id'] == args.case)
    args.output.mkdir(parents=True, exist_ok=True)
    fs, q = 192000.0, row['period_samples']
    tones = tones_for(row)
    forcing = row['amplitude'] * tones[:, 0]
    model = Model('tanh', row['drive_knob'], row['resonance_knob'], row['cutoff_hz'], fs)
    model.k, model.a, model.b = row['feedback'], row['alpha'], 1 - row['alpha']
    seed = linear_seed(model, row['amplitude'], row['frequency_hz'])
    fit = root(lambda x: shoot(model, forcing, x), seed, jac=True, tol=1e-11)
    residual, derivative = shoot(model, forcing, fit.x)
    assert np.max(np.abs(residual)) < 1e-10, (fit.message, residual)
    monodromy = derivative + np.eye(4)
    multipliers = np.linalg.eigvals(monodromy)
    library = load_trace_library()
    final, periodic_states, periodic_output = native_period(library, row, tones, fit.x)
    native_residual = float(np.max(np.abs(final - fit.x)))
    assert native_residual < 1e-10, native_residual
    finite_difference = np.empty((4, 4))
    step = 1e-7
    for j in range(4):
        direction = np.eye(4)[j] * step
        plus = native_period(library, row, tones, fit.x + direction)[0]
        minus = native_period(library, row, tones, fit.x - direction)[0]
        finite_difference[:, j] = (plus - minus) / (2 * step)
    derivative_error = float(np.max(np.abs(finite_difference - monodromy)))
    assert derivative_error < 1e-5, derivative_error

    # Continue the same input-periodic branch from the lowest drive knob value.
    continuation = []
    current = None
    for knob in np.linspace(0, row['drive_knob'], 25):
        local = Model('tanh', knob, row['resonance_knob'], row['cutoff_hz'], fs)
        local.k, local.a, local.b = model.k, model.a, model.b
        if current is None:
            current = linear_seed(local, row['amplitude'], row['frequency_hz'])
        solved = root(lambda x: shoot(local, forcing, x), current, jac=True, tol=1e-11)
        defect, jac = shoot(local, forcing, solved.x)
        assert np.max(np.abs(defect)) < 1e-10, (knob, solved.message, defect)
        current = solved.x
        continuation.append(dict(drive_knob=float(knob), gain=local.g,
            spectral_radius=float(max(abs(np.linalg.eigvals(jac + np.eye(4))))),
            state=current.tolist(), residual=float(max(abs(defect)))))
    assert np.max(np.abs(current - fit.x)) < 1e-10

    periods = math.ceil(args.seconds * fs / q)
    state = np.array([row[f'state_{j}'] for j in range(1, 5)])
    endpoints, features, drifting_output = trace(library, row, tones, periods, state, np.full(4, 0.5))
    # Independently inspect the reset response, after a fresh eight-second burn-in.
    reset_state = np.zeros(4)
    tangent = np.full(4, 0.5)
    trace(library, row, tones, math.ceil(8 * fs / q), reset_state, tangent)
    _, reset_features, _ = trace(library, row, tones, periods, reset_state, tangent)
    escape_states, _, _ = trace(library, row, tones, 20, fit.x.copy(), np.full(4, 0.5))
    periodic_tiled = np.tile(periodic_output, periods)
    drifting_spectrum, drifting_peaks = peaks_of(drifting_output, fs)
    periodic_spectrum, periodic_peaks = peaks_of(periodic_tiled, fs)
    periodic_coeff = 2 * np.fft.rfft(periodic_output) / q
    oscillator = drifting_peaks[0]['frequency_hz']
    for peak in drifting_peaks:
        matches = [(abs(abs(m * oscillator + n * row['frequency_hz']) - peak['frequency_hz']), m, n)
                   for m in range(9) for n in range(-8, 9) if m != 0 or n != 0]
        error, m, n = min(matches)
        peak.update(mixing_oscillator_coefficient=m, mixing_input_coefficient=n,
                    mixing_error_hz=error)

    mean_state = np.mean(endpoints, axis=0)
    _, vectors = np.linalg.eigh(np.cov(endpoints.T))
    plane = (endpoints - mean_state) @ vectors[:, -2:]
    root_plane = (fit.x - mean_state) @ vectors[:, -2:]
    summary = dict(case=row, model='detector-disabled real tanh, binary64',
        periodic=dict(state=fit.x.tolist(), residual=float(max(abs(residual))), native_residual=native_residual,
            function_evaluations=fit.nfev, monodromy=monodromy.tolist(),
            multipliers=[[float(v.real), float(v.imag)] for v in multipliers],
            spectral_radius=float(max(abs(multipliers))),
            jacobian_central_difference_max_error=derivative_error,
            power=float(np.mean(periodic_output ** 2)),
            fundamental_peak_amplitude=float(abs(periodic_coeff[row['input_cycles']])),
            harmonics=[dict(harmonic=h, amplitude=float(abs(periodic_coeff[h * row['input_cycles']])))
                       for h in range(1, 16)], spectral_peaks=periodic_peaks),
        drifting=dict(seconds=periods * q / fs, fft_resolution_hz=fs / len(drifting_output),
            power=float(np.mean(features[:, 0])),
            coherent_fundamental_peak_amplitude=float(np.linalg.norm(np.mean(features[:, 1:3], axis=0))),
            reset_power=float(np.mean(reset_features[:, 0])),
            reset_coherent_fundamental_peak_amplitude=float(np.linalg.norm(np.mean(reset_features[:, 1:3], axis=0))),
            tangent_growth_per_second=float(np.sum(features[periods//2:, 3]) / ((periods-periods//2)*q/fs)),
            spectral_peaks=drifting_peaks),
        continuation=continuation,
        root_escape=[dict(seconds=i*q/fs, distance=float(np.linalg.norm(point-fit.x)))
                     for i, point in enumerate(escape_states)])
    (args.output / 'analysis.json').write_text(json.dumps(summary, indent=2) + '\n')
    np.savez_compressed(args.output / 'traces.npz', periodic_states=periodic_states,
        periodic_output=periodic_output, drifting_output=drifting_output, endpoints=endpoints,
        features=features, reset_features=reset_features, plane=plane, root_plane=root_plane,
        escape_states=escape_states, forcing=forcing)

    # Preserve narrow peaks when reducing the FFT for the conversation chart.
    indices = set()
    edges = np.unique(np.rint(np.geomspace(20, 10000, 700) * len(drifting_output) / fs).astype(int))
    for lo, hi in zip(edges[:-1], edges[1:]):
        for spectrum in [drifting_spectrum, periodic_spectrum]:
            indices.add(lo + int(np.argmax(spectrum[lo:hi])))
    spectrum_rows = [[j * fs / len(drifting_output), max(-100.0, float(20 * np.log10(drifting_spectrum[j]))),
                      max(-100.0, float(20 * np.log10(periodic_spectrum[j])))] for j in sorted(indices)]
    assert len(spectrum_rows) <= 2000
    spectrum_chart = dict(id='ladder-periodic-spectrum', title='Same filter, two responses',
        description=f'{periods*q/fs:g}-second Hann FFT · values below −100 dB shown at the display floor · input amplitude {row["amplitude"]:g}',
        summary=f'The drifting trajectory is dominated by a {oscillator:.1f} Hz oscillation and mixing tones. Compare the directly solved periodic solution.',
        height=360, columns=['Frequency', 'Drifting trajectory', 'Periodic solution'], rows=spectrum_rows,
        spec=dict(type='line', x='Frequency', y=['Drifting trajectory', 'Periodic solution'],
            xScale='log', xDomain=[20, 10000], yDomain=[-100, 10], startAtZero=False,
            xLabel='Frequency (Hz, log scale)', yLabel='FFT amplitude (dB relative to 1)',
            valueFormat=dict(style='decimal', maximumFractionDigits=2)))
    # Consecutive return-map samples; no subsampling of strobe indices.
    strobe_rows = [[float(x), float(y), 'Drifting trajectory', f'Input period {j}']
                   for j, (x, y) in enumerate(plane[-800:])]
    strobe_rows.append([float(root_plane[0]), float(root_plane[1]), 'Periodic solution', 'Fixed point'])
    strobe_chart = dict(id='ladder-periodic-strobes', title=f'State sampled every {q/fs*1000:g} ms',
        description='Two-dimensional projection of the four filter states; forcing phase held fixed.',
        summary='The drifting states trace a loop. The periodic solution is a single point inside its projection.',
        height=320, columns=['Axis 1', 'Axis 2', 'Response', 'name'], rows=strobe_rows,
        spec=dict(type='scatter', x='Axis 1', y='Axis 2', series='Response', startAtZero=False,
                  xLabel='State projection 1', yLabel='State projection 2'))
    (args.output / 'charts.json').write_text(json.dumps([spectrum_chart, strobe_chart], separators=(',', ':')) + '\n')
    print(json.dumps({key: summary[key] for key in ['periodic', 'drifting']}, indent=2))


if __name__ == '__main__':
    main()

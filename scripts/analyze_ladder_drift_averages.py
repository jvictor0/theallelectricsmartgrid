#!/usr/bin/env python3
"""Discrete coboundary diagnostics for finite-level ladder powers.

The proposed potential V makes observable - mean approximately V - V o F.
Telescoping then bounds time averages without an assumed phase density.
These are sampled binary64 proposals, not uniform or Lean certificates.
"""

import json
from pathlib import Path
import numpy as np

from analyze_ladder_drift_attraction import Chart, advance
from analyze_ladder_harmonic_reduction import Model, basis


def observe_block(model, states, forcing):
    states = np.array(states, copy=True)
    n = len(forcing)
    carrier = np.exp(-2j*np.pi*np.arange(n)/54)
    coherent = np.zeros(len(states), complex)
    power = np.zeros(len(states))
    for j, u in enumerate(forcing):
        states = model.step(states, u)
        output = (1+model.k)*model.sat(states[:, 3])
        coherent += 2/n*output*carrier[j]
        power += output**2/n
    return states, np.c_[coherent.real, coherent.imag, power]


def analyze():
    root = Path(__file__).resolve().parents[1]
    row = json.loads((root/'build/results/ladder-drift-attraction/diagnostics.json').read_text())[-1]
    coefficients = np.array(row['profileFourierReal'])+1j*np.array(row['profileFourierImag'])
    # The chart is a finite polynomial; discarded coefficients are not assumed
    # to describe the exact invariant curve. Its defect is measured afresh.
    modes = np.fft.fftfreq(len(coefficients), 1/len(coefficients))
    coefficients[abs(modes)>80] = 0
    chart = Chart(np.array(row['coordinateTransform']), coefficients)
    model = Model('tanh', row['drive'], row['resonance'])
    forcing = 0.25*np.cos(2*np.pi*np.arange(162)/54)
    theta = 2*np.pi*np.arange(2048)/2048
    final, observables = observe_block(model, chart.decode(theta, np.zeros((len(theta), 3))), forcing)
    phase, normal = chart.encode(final)
    test = 2*np.pi*(np.arange(8192)+0.37)/8192
    final_test, observed_test = observe_block(model, chart.decode(test, np.zeros((len(test), 3))), forcing)
    phase_test, normal_test = chart.encode(final_test)
    results = []
    for highest in [16, 32, 64, 96, 128]:
        harmonics = np.arange(1, highest+1)
        matrix = np.c_[np.ones(len(theta)), basis(theta, harmonics)-basis(phase, harmonics)]
        solution, _, rank, singular = np.linalg.lstsq(matrix, observables, rcond=None)
        potential = basis(test, harmonics)@solution[1:]
        residual = observed_test-solution[0]-potential+basis(phase_test, harmonics)@solution[1:]
        result = {'highest': highest, 'mean': solution[0].tolist(),
                  'sampledResidual': np.max(abs(residual), axis=0).tolist(),
                  'sampledPotentialBound': np.max(abs(potential), axis=0).tolist(),
                  'fourierPotentialBound': np.sum(np.hypot(solution[1::2], solution[2::2]), axis=0).tolist(),
                  'fourierPotentialDerivativeBound': np.sum(harmonics[:, None]*np.hypot(solution[1::2], solution[2::2]), axis=0).tolist(),
                  'condition': float(singular[0]/singular[-1]), 'rank': int(rank),
                  'potential': solution[1:].tolist()}
        results.append(result)
    # Direct runs from several phases check which averages are being estimated.
    states = chart.decode(np.array([0.1, 1.7, 3.4, 5.2]), np.zeros((4, 3)))
    accumulated = np.zeros((4, 3))
    checkpoints = []
    for n in range(4096):
        states, measured = observe_block(model, states, forcing)
        accumulated += measured
        if n+1 in [64, 256, 1024, 4096]:
            checkpoints.append({'blocks': n+1, 'averages': (accumulated/(n+1)).tolist()})
    output = {'status': 'sampled proposals, not uniform certificates',
              'candidateGraphSampledDefect': float(np.max(np.linalg.norm(normal_test, axis=-1))),
              'results': results, 'directCheckpoints': checkpoints,
              'chart': {'transform': chart.transform.tolist(),
                        'coefficientsReal': coefficients.real.tolist(),
                        'coefficientsImag': coefficients.imag.tolist()}}
    directory = root/'build/results/ladder-drift-certificate'
    directory.mkdir(exist_ok=True, parents=True)
    (directory/'averages.json').write_text(json.dumps(output, indent=2)+'\n')
    for result in results:
        print({k:v for k,v in result.items() if k!='potential'}, flush=True)
    print({'candidateGraphSampledDefect': output['candidateGraphSampledDefect'],
           'directCheckpoints': checkpoints}, flush=True)


if __name__ == '__main__':
    analyze()

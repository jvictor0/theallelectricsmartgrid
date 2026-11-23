#!/usr/bin/env python3
"""Local stability diagnostics for the detector-disabled discrete ladder.

Double-precision diagnostics, not interval certificates. The autonomous test
removes phase along an approximate invariant circle. The forced test uses an
integer sample period and differentiates the actual repeated sample map.
"""

import json
from pathlib import Path

import numpy as np
from scipy.linalg import null_space, solve_discrete_lyapunov
from scipy.optimize import root

from analyze_ladder_harmonic_reduction import Model, basis


def sample_with_jacobian(model, state, u):
    next_state = model.step(state, u)
    jac = np.zeros((4, 4))
    jac[0, 0], jac[0, 3] = model.b, -model.a*model.k
    for j in range(1, 4):
        jac[j] = model.a*model.derivative(next_state[j-1])*jac[j-1]
        jac[j, j] += model.b
    return next_state, jac


def autonomous_normal_test(model, orbit):
    harmonics = np.arange(1, orbit['highest']+1, 2)
    coeff = np.array(orbit['coefficients'])
    count = round(2*np.pi/orbit['omega'])
    values, drifts = [], []
    for phase in np.linspace(0, 2*np.pi, 128, endpoint=False):
        state = (basis(np.array([phase]), harmonics)@coeff)[0]
        tangent = (basis(np.array([phase]), harmonics, True)@coeff)[0]
        normal = null_space(tangent[None, :])
        propagated = normal.copy()
        for _ in range(count):
            state, jac = sample_with_jacobian(model, state, 0)
            propagated = jac@propagated
        final_phase = phase+count*orbit['omega']
        final_tangent = (basis(np.array([final_phase]), harmonics, True)@coeff)[0]
        final_normal = null_space(final_tangent[None, :])
        values.append(np.linalg.norm(final_normal.T@propagated, 2))
        reference = (basis(np.array([final_phase]), harmonics)@coeff)[0]
        drifts.append(np.linalg.norm(state-reference))
    return {'blockSamples': count, 'maxNormalOperatorNorm': max(values),
            'minNormalOperatorNorm': min(values), 'maxOrbitDefect': max(drifts),
            'sampledPhases': len(values)}


def periodic_test(model, orbit, amplitude=0.25, harmonic=3):
    count = round(model.fs/orbit['frequencyHz'])
    forcing = amplitude*np.cos(2*np.pi*harmonic*np.arange(count)/count)
    def period(state):
        jac = np.eye(4)
        for u in forcing:
            state, j = sample_with_jacobian(model, state, u)
            jac = j@jac
        return state, jac
    state = np.array(orbit['coefficients'])[0].copy()
    for _ in range(100):
        state, _ = period(state)
    solution = root(lambda x: period(x)[0]-x, state,
                    jac=lambda x: period(x)[1]-np.eye(4), tol=1e-11)
    state = solution.x
    final, jac = period(state)
    residual = np.linalg.norm(final-state)
    if residual > 1e-10:
        raise RuntimeError(('periodic solve failed', model.kind, model.g, residual))
    eigenvalues = np.linalg.eigvals(jac)
    p = solve_discrete_lyapunov(jac.T, np.eye(4))
    eigen_p = np.linalg.eigvalsh(p)
    if min(eigen_p) <= 0:
        raise RuntimeError(('not an attracting solution', eigenvalues))
    # ||x||_P = ||L*x||_2. This norm removes non-normal Euclidean transients.
    transform = np.linalg.cholesky(p).T
    transformed = transform@jac@np.linalg.inv(transform)
    return {'samplePeriod': count, 'inputCycles': harmonic,
            'inputFrequencyHz': harmonic*model.fs/count, 'amplitude': amplitude,
            'state': state.tolist(), 'periodResidual': float(residual),
            'jacobian': jac.tolist(), 'adaptedNorm': transform.tolist(),
            'multipliers': [[float(v.real), float(v.imag)] for v in eigenvalues],
            'spectralRadius': float(max(abs(eigenvalues))),
            'euclideanNorm': float(np.linalg.norm(jac, 2)),
            'adaptedOperatorNorm': float(np.linalg.norm(transformed, 2)),
            'adaptedInfinityNorm': float(np.linalg.norm(transformed, np.inf))}


def main():
    root_dir = Path(__file__).resolve().parents[1]
    rows = json.loads((root_dir/'build/results/fundamental-third/reduction.json').read_text())
    result = []
    for row in rows:
        model = Model(row['model'], row['drive'], row['resonance'])
        item = {'model': row['model'], 'drive': row['drive'], 'resonance': row['resonance'],
                'autonomous': autonomous_normal_test(model, row['orbits'][-1]),
                'periodic': periodic_test(model, row['orbits'][-1])}
        result.append(item)
        print(json.dumps(item), flush=True)
    out = root_dir/'build/results/ladder-attraction'
    out.mkdir(parents=True, exist_ok=True)
    (out/'diagnostics.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()

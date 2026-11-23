#!/usr/bin/env python3
"""Invariant-curve / graph-transform diagnostics for the exact sample recurrence.

These are double-precision, sampled diagnostics, NOT uniform interval bounds.
The input is exactly periodic in sample time; the response phase may drift.
No continuous averaged phase equation is substituted for the sample map.
"""

import json
from pathlib import Path

import numpy as np
from scipy.interpolate import CubicSpline

from analyze_ladder_harmonic_reduction import Model, basis
from analyze_ladder_attraction import sample_with_jacobian


def advance(model, states, forcing):
    """Batch the same scalar sample equations and their exact chain rule."""
    states = np.array(states, copy=True)
    jacobian = np.broadcast_to(np.eye(4), states.shape[:-1]+(4, 4)).copy()
    for u in forcing:
        updated = model.step(states, u)
        jac = np.zeros_like(jacobian)
        jac[..., 0, 0], jac[..., 0, 3] = model.b, -model.a*model.k
        for j in range(1, 4):
            jac[..., j, :] = model.a*model.derivative(updated[..., j-1])[..., None]*jac[..., j-1, :]
            jac[..., j, j] += model.b
        states = updated
        jacobian = jac@jacobian
    return states, jacobian


def periodic_fit(phase, values, theta):
    order = np.argsort(phase % (2*np.pi))
    phase, values = (phase % (2*np.pi))[order], values[order]
    phase = np.r_[phase, phase[0]+2*np.pi]
    values = np.concatenate([values, values[:1]])
    spline = CubicSpline(phase, values, bc_type='periodic')
    return spline((theta-phase[0]) % (2*np.pi)+phase[0])


class Chart:
    def __init__(self, transform, coefficients):
        self.transform = transform
        self.inverse = np.linalg.inv(transform)
        self.coefficients = coefficients
        self.modes = np.fft.fftfreq(len(coefficients), 1/len(coefficients))

    def evaluate_profile(self, theta, derivative=0):
        weights = np.exp(1j*np.asarray(theta)[..., None]*self.modes)
        return np.real(weights@(((1j*self.modes)**derivative)[:, None]*self.coefficients))

    def decode(self, theta, transverse):
        profile = self.evaluate_profile(theta)+transverse
        z = np.stack([profile[..., 0]*np.cos(theta), profile[..., 0]*np.sin(theta),
                      profile[..., 1], profile[..., 2]], axis=-1)
        return z@self.inverse.T

    def encode(self, states):
        z = states@self.transform.T
        phase = np.arctan2(z[..., 1], z[..., 0])
        profile = np.stack([np.hypot(z[..., 0], z[..., 1]), z[..., 2], z[..., 3]], axis=-1)
        return phase, profile-self.evaluate_profile(phase)

    def derivative_decode(self, theta, transverse):
        profile = self.evaluate_profile(theta)+transverse
        derivative = self.evaluate_profile(theta, 1)
        c, s = np.cos(theta), np.sin(theta)
        matrix = np.zeros(np.shape(theta)+(4, 4))
        matrix[..., 0, 0] = derivative[..., 0]*c-profile[..., 0]*s
        matrix[..., 1, 0] = derivative[..., 0]*s+profile[..., 0]*c
        matrix[..., 2:, 0] = derivative[..., 1:]
        matrix[..., 0, 1], matrix[..., 1, 1] = c, s
        matrix[..., 2, 2], matrix[..., 3, 3] = 1, 1
        return self.inverse@matrix

    def derivative_encode(self, states):
        z = states@self.transform.T
        radius = np.hypot(z[..., 0], z[..., 1])
        phase = np.arctan2(z[..., 1], z[..., 0])
        derivative = self.evaluate_profile(phase, 1)
        matrix = np.zeros(states.shape[:-1]+(4, 4))
        matrix[..., 0, 0], matrix[..., 0, 1] = -z[..., 1]/radius**2, z[..., 0]/radius**2
        matrix[..., 1, 0], matrix[..., 1, 1] = z[..., 0]/radius, z[..., 1]/radius
        matrix[..., 2, 2], matrix[..., 3, 3] = 1, 1
        matrix[..., 1:, :] -= derivative[..., :, None]*matrix[..., :1, :]
        return matrix@self.transform


def analyze(row, sample_period=54, count=1024):
    model = Model(row['model'], row['drive'], row['resonance'])
    orbit = row['orbits'][-1]
    theta = 2*np.pi*np.arange(count)/count
    initial = basis(theta, np.arange(1, orbit['highest']+1, 2))@np.array(orbit['coefficients'])
    _, _, rotation = np.linalg.svd(initial, full_matrices=False)
    transform = rotation/np.std(initial@rotation.T, axis=0)[:, None]
    # Orient increasing polar angle along the original oscillator phase.
    z = initial@transform.T
    if np.mean(np.diff(np.unwrap(np.arctan2(z[:, 1], z[:, 0])))) < 0:
        transform[1] *= -1
    z = initial@transform.T
    polar = np.arctan2(z[:, 1], z[:, 0])
    assert np.min(np.diff(np.unwrap(polar))) > 0, 'initial polar chart folds'
    profile = periodic_fit(polar, np.c_[np.hypot(z[:, 0], z[:, 1]), z[:, 2:]], theta)
    forcing = 0.25*np.cos(2*np.pi*np.arange(3*sample_period)/sample_period)
    chart = Chart(transform, np.fft.fft(profile, axis=0)/count)
    history = []
    for _ in range(24):
        states = chart.decode(theta, np.zeros((count, 3)))
        final, _ = advance(model, states, forcing)
        phase, normal = chart.encode(final)
        history.append(float(np.max(np.linalg.norm(normal, axis=-1))))
        phase_lift = np.unwrap(phase)
        assert np.min(np.diff(phase_lift)) > 0, 'graph phase map folds'
        z = final@transform.T
        profile = periodic_fit(phase, np.c_[np.hypot(z[:, 0], z[:, 1]), z[:, 2:]], theta)
        chart = Chart(transform, np.fft.fft(profile, axis=0)/count)
    # A half-cell offset avoids reusing the points that built the graph.
    test_theta = theta[::4]+np.pi/count
    radius = 0.001
    corners = np.array(np.meshgrid(*[[-1, 1]]*3)).reshape(3, -1).T/np.sqrt(3)
    offsets = np.r_[np.zeros((1, 3)), radius*np.eye(3), -radius*np.eye(3), radius*corners]
    phases = np.repeat(test_theta, len(offsets))
    normals = np.tile(offsets, (len(test_theta), 1))
    states = chart.decode(phases, normals)
    final, jacobian = advance(model, states, forcing)
    jac = chart.derivative_encode(final)@jacobian@chart.derivative_decode(phases, normals)
    a = float(np.min(jac[:, 0, 0]))
    b = float(np.max(np.linalg.norm(jac[:, 0, 1:], axis=-1)))
    c = float(np.max(np.linalg.norm(jac[:, 1:, 0], axis=-1)))
    d = float(np.max(np.linalg.norm(jac[:, 1:, 1:], ord=2, axis=(-2, -1))))
    slope = 0.01
    separation = a-b*slope
    rate = d+(c+d*slope)*b/separation
    final_phase, final_normal = chart.encode(final)
    image_radius = float(np.max(np.linalg.norm(final_normal, axis=-1)))
    center_defect = float(np.max(np.linalg.norm(final_normal[::len(offsets)], axis=-1)))
    assert image_radius < radius, ("sampled tube fails trapping", image_radius, radius)
    assert separation > 0 and (c+d*slope)/separation < slope and rate < 1
    increment = (final_phase-phases+np.pi) % (2*np.pi)-np.pi
    # Independent scalar recurrence / Jacobian cross-check for one sample.
    seed = states[0]
    scalar_state, scalar_jac = sample_with_jacobian(model, seed, forcing[0])
    batch_state, batch_jac = advance(model, seed[None, :], forcing[:1])
    assert np.max(abs(scalar_state-batch_state[0])) < 1e-14
    assert np.max(abs(scalar_jac-batch_jac[0])) < 1e-14
    # Check the whole coordinate chain rule independently, including both
    # phase/amplitude coupling blocks. Wrap only angular differences.
    probe = np.c_[phases[:16], normals[:16]]
    def coordinate_step(coords):
        final, _ = advance(model, chart.decode(coords[:, 0], coords[:, 1:]), forcing)
        phase, normal = chart.encode(final)
        return np.c_[phase, normal]
    finite_difference = np.empty((len(probe), 4, 4))
    for j in range(4):
        delta = np.eye(4)[j]*1e-6
        difference = coordinate_step(probe+delta)-coordinate_step(probe-delta)
        difference[:, 0] = (difference[:, 0]+np.pi) % (2*np.pi)-np.pi
        finite_difference[:, :, j] = difference/(2e-6)
    derivative_error = float(np.max(abs(jac[:len(probe)]-finite_difference)))
    assert derivative_error < 1e-5, ('coordinate derivative mismatch', derivative_error)
    # Track a displaced state through the actual recurrence; measure distance
    # to the candidate graph at the state's current phase, without phase fit.
    state = chart.decode(np.array([0.23]), np.array([[radius, 0, 0]]))
    attraction = []
    phase_trace = []
    for _ in range(300):
        phase, normal = chart.encode(state)
        attraction.append(float(np.linalg.norm(normal[0])))
        phase_trace.append(float(phase[0]))
        state, _ = advance(model, state, forcing)
    return {'model': model.kind, 'drive': row['drive'], 'resonance': row['resonance'],
            'inputAmplitude': 0.25, 'inputPeriodSamples': sample_period,
            'inputFrequencyHz': model.fs/sample_period, 'blockSamples': len(forcing),
            'graphGridPoints': count, 'graphIterationDefects': history,
            'sampledTubeRadius': radius, 'sampledImageTubeRadius': image_radius,
            'sampledCandidateGraphDefect': center_defect, 'sampledPhaseLowerDerivative': a,
            'sampledPhaseFiberCoupling': b, 'sampledTransversePhaseCoupling': c,
            'sampledNormalOperatorNorm': d, 'trialGraphLipschitzBound': slope,
            'sampledGraphPhaseSeparation': separation,
            'sampledTransformedGraphSlopeBound': (c+d*slope)/separation,
            'sampledGraphContractionBound': rate,
            'coordinateDerivativeFiniteDifferenceMaxError': derivative_error,
            'sampledRelativePhaseIncrementRange': [float(min(increment)), float(max(increment))],
            'attractionErrorsFirst20Blocks': attraction[:20],
            'attractionErrorLastBlock': attraction[-1],
            'observedPhaseTurnsPerBlock': float(np.mean(np.diff(np.unwrap(phase_trace[50:])))/(2*np.pi)),
            'status': 'sampled double-precision diagnostics, not uniform interval certificates',
            'coordinateTransform': transform.tolist(),
            'profileFourierReal': chart.coefficients.real.tolist(),
            'profileFourierImag': chart.coefficients.imag.tolist()}


def main():
    root = Path(__file__).resolve().parents[1]
    rows = json.loads((root/'build/results/fundamental-third/reduction.json').read_text())
    row = next(r for r in rows if r['model'] == 'tanh' and r['drive'] == 1)
    results = [analyze(row, count=count) for count in [1024, 2048]]
    out = root/'build/results/ladder-drift-attraction'
    out.mkdir(parents=True, exist_ok=True)
    (out/'diagnostics.json').write_text(json.dumps(results, indent=2)+'\n')
    for result in results:
        print(json.dumps({k: v for k, v in result.items() if not k.startswith('profileFourier')}, indent=2))


if __name__ == '__main__':
    main()

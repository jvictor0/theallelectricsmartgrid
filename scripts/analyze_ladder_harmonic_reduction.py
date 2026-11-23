#!/usr/bin/env python3
"""Exploratory harmonic/phase reduction of the detector-disabled ladder.

Requires NumPy and SciPy. Uses binary64 quadrature and nonlinear solves, not
interval arithmetic or proof certificates. This is not production DSP code.
Outputs live under build/results, independently of disposable build trees.
"""

import argparse
import json
import math
from pathlib import Path

import numpy as np
from scipy.linalg import eigh
from scipy.optimize import brentq, root


class Model:
    def __init__(self, kind, drive, resonance, cutoff=1000.0, sample_rate=192000.0):
        self.kind = kind
        self.fs = sample_rate
        self.g = 0.25 * 20**drive
        self.k = 3.9 * (49**resonance - 1) / 48
        sine = np.sin(np.pi * cutoff / sample_rate)
        self.a = float(2 * sine * (np.sqrt(1 + sine * sine) - sine))
        self.b = 1 - self.a
        self.nominal = float(2 * np.arcsin(self.a / (2 * self.b)))
        self.normalization = float(self.raw(self.g))

    def raw(self, x):
        if self.kind == "tanh":
            return np.tanh(x)
        return np.clip(x * (x*x + 27) / (9*x*x + 27), -1, 1)

    def sat(self, x):
        return self.raw(self.g*x) / self.normalization

    def derivative(self, x):
        t = self.g*x
        if self.kind == "tanh":
            derivative = 1 - np.tanh(t)**2
        else:
            derivative = np.where(np.abs(t) < 3, (t*t - 9)**2 / (9*(t*t + 3)**2), 0)
        return self.g * derivative / self.normalization

    def step(self, q, u):
        r = np.empty_like(q)
        r[..., 0] = self.b*q[..., 0] + self.a*(u - self.k*q[..., 3])
        for j in range(1, 4):
            r[..., j] = self.b*q[..., j] + self.a*self.sat(r[..., j-1])
        return r

    def linearize(self, q):
        r = self.step(q, 0)
        jacobian = np.zeros((len(q), 4, 4))
        input_derivative = np.empty_like(q)
        jacobian[:, 0, 0] = self.b
        jacobian[:, 0, 3] = -self.a*self.k
        input_derivative[:, 0] = self.a
        for j in range(1, 4):
            slope = self.a*self.derivative(r[:, j-1])
            jacobian[:, j, :] = slope[:, None]*jacobian[:, j-1, :]
            jacobian[:, j, j] += self.b
            input_derivative[:, j] = slope*input_derivative[:, j-1]
        return jacobian, input_derivative


def basis(theta, harmonics, derivative=False):
    if derivative:
        return np.column_stack([v for h in harmonics for v in
                                (-h*np.sin(h*theta), h*np.cos(h*theta))])
    return np.column_stack([v for h in harmonics for v in
                            (np.cos(h*theta), np.sin(h*theta))])


def solve_orbit(model, highest, previous=None):
    hs = np.arange(1, highest+1, 2)
    seed = np.zeros((2*len(hs), 4))
    if previous is None:
        response = model.a/(1-model.b*np.exp(-1j*model.nominal))
        cosine = np.cos(2*np.pi*np.arange(8192)/8192)

        def after(a):
            return abs(response)*2*np.mean(model.sat(a*cosine)*cosine)

        def balance(a):
            return model.k*abs(response)*after(after(after(a)))-a

        a1 = brentq(balance, 1e-7, 100)
        a2 = after(a1)
        a3 = after(a2)
        a4 = after(a3)
        phasors = np.array([a1, a2, a3, a4])*np.exp(1j*np.angle(response)*np.arange(4))
        phasors *= np.exp(-1j*np.angle(phasors[3]))
        seed[0], seed[1] = phasors.real, -phasors.imag
        seed_frequency = model.nominal
    else:
        old = np.array(previous["coefficients"])
        seed[:len(old)] = old
        seed_frequency = previous["omega"]
    theta = 2*np.pi*np.arange(2048)/2048
    b = basis(theta, hs)
    projection = 2/len(theta)*b.T

    def defects(coeff, omega, grid):
        current = basis(grid, hs)@coeff
        past = basis(grid-omega, hs)@coeff
        r = current-model.b*past
        r[:, 0] += model.a*model.k*past[:, 3]
        r[:, 1:] -= model.a*model.sat(current[:, :3])
        return r

    def residual(v):
        coeff = v[:-1].reshape(-1, 4)
        return np.r_[(projection@defects(coeff, v[-1]*model.nominal, theta)).ravel()/model.a,
                     coeff[1, 3]]

    fit = root(residual, np.r_[seed.ravel(), seed_frequency/model.nominal], tol=1e-10)
    retained = float(np.max(np.abs(residual(fit.x))))
    if retained > 1e-7:
        raise RuntimeError(f"HB failed for {model.kind}, H{highest}: {fit.message}; {retained}")
    coeff = fit.x[:-1].reshape(-1, 4)
    omega = float(fit.x[-1]*model.nominal)
    check_theta = 2*np.pi*np.arange(16384)/16384
    q = basis(check_theta, hs)@coeff
    defect = defects(coeff, omega, check_theta)
    spectrum = np.fft.rfft((1+model.k)*model.sat(q[:, 3]))/len(q)
    normalized_rms = np.sqrt(np.mean(defect**2, axis=0))/model.a/np.sqrt(np.mean(q**2, axis=0))
    result = {"highest": highest, "omega": omega, "frequencyHz": omega*model.fs/(2*np.pi),
              "retainedResidual": retained, "normalizedDefectRmsByStage": normalized_rms.tolist(),
              "outputPowers": {str(h): float(2*abs(spectrum[h])**2) for h in [1, 3, 5, 7]},
              "coefficients": coeff.tolist()}
    return result


def phase_response(model, orbit, highest=31):
    """Numerical adjoint Z(theta)=J(theta)^T Z(theta+omega), Z.q'=1.

    Normalization is in radians. The orbit is approximate, so even the least
    singular vector has a nonzero residual. Record that residual and the
    variation in Z.q' rather than presenting this as a validated adjoint.
    """
    theta = 2*np.pi*np.arange(2048)/2048
    omega = orbit["omega"]
    hs = np.arange(1, orbit["highest"]+1, 2)
    coeff = np.array(orbit["coefficients"])
    q = basis(theta, hs)@coeff
    tangent = basis(theta, hs, True)@coeff
    jacobian, input_derivative = model.linearize(q)
    adj_hs = np.arange(1, highest+1, 2)
    b = basis(theta, adj_hs)
    shifted = basis(theta+omega, adj_hs)
    operator = (np.einsum("na,ij->niaj", b, np.eye(4)) -
                np.einsum("nji,na->niaj", jacobian, shifted)).reshape(len(theta)*4, -1)
    _, vec = eigh(operator.T@operator, subset_by_index=[0, 0])
    zc = vec[:, 0].reshape(-1, 4)
    zc /= np.mean(np.sum((b@zc)*tangent, axis=1))
    z = b@zc
    z_next = shifted@zc
    defect = z-np.einsum("nji,nj->ni", jacobian, z_next)
    gamma = np.sum(z_next*input_derivative, axis=1)
    fourier = np.fft.rfft(gamma)/len(gamma)
    phase_normalization = np.sum(z*tangent, axis=1)
    return {"highest": highest,
            "relativeAdjointDefect": float(np.linalg.norm(defect)/np.linalg.norm(z)),
            "normalizationRmsError": float(np.sqrt(np.mean((phase_normalization-1)**2))),
            "gammaAbs": {str(h): float(abs(fourier[h])) for h in [1, 3, 5, 7]},
            "halfWidthHzAtAmplitudeQuarter": {
                str(h): float(model.fs/(2*np.pi)*h*0.25*abs(fourier[h])) for h in [1, 3, 5, 7]}}


def check_linearization(model):
    q = np.random.default_rng(42).normal(0, 0.3, (32, 4))
    j, b = model.linearize(q)
    eps = 1e-6
    numeric_j = np.stack([(model.step(q+eps*np.eye(4)[i], 0)-
                           model.step(q-eps*np.eye(4)[i], 0))/(2*eps) for i in range(4)], axis=-1)
    numeric_b = (model.step(q, eps)-model.step(q, -eps))/(2*eps)
    error = max(float(np.max(np.abs(j-numeric_j))), float(np.max(np.abs(b-numeric_b))))
    assert error < 1e-8, error
    return error


def forced_check(model, amplitude, frequency, harmonic, warm=192000, count=384000, period=None):
    """Finite-time phase-slip test, not an attraction or locking certificate.

    At positive stage-four zero crossings a locked oscillator sees a constant
    external phase. Report its unwrapped drift and per-window mean frequency.
    """
    omega = 2*math.pi*frequency/model.fs
    if period is None:
        input_signal = amplitude*np.cos(omega*np.arange(warm+count))
    else:
        length, cycles = period
        assert warm % length == 0 and count % length == 0
        omega = 2*math.pi*cycles/length
        frequency = cycles*model.fs/length
        table = amplitude*np.cos(omega*np.arange(length))
        input_signal = np.resize(table, warm+count)
    a, b, k, g = model.a, model.b, model.k, model.g
    normalization = model.normalization
    if model.kind == "tanh":
        def saturate(x):
            return math.tanh(g*x)/normalization
    else:
        def saturate(x):
            t = g*x
            return max(-1.0, min(1.0, t*(t*t+27)/(9*t*t+27)))/normalization
    q1 = q2 = q3 = q4 = 0.0
    crossings = []
    output = np.empty(count)
    measurement_start = None
    for n, u in enumerate(input_signal):
        if n == warm:
            measurement_start = np.array([q1, q2, q3, q4])
        old_q4 = q4
        q1 = b*q1+a*(float(u)-k*q4)
        q2 = b*q2+a*saturate(q1)
        q3 = b*q3+a*saturate(q2)
        q4 = b*q4+a*saturate(q3)
        if n >= warm:
            output[n-warm] = (1+k)*saturate(q4)
            if old_q4 < 0 <= q4:
                crossings.append(n-1-old_q4/(q4-old_q4))
    crossings = np.asarray(crossings)
    phase = np.unwrap(np.angle(np.exp(1j*omega*crossings)))
    tail = phase[len(phase)//2:]
    frequency_error = np.polyfit(crossings[len(phase)//2:]/model.fs, tail, 1)[0]/(2*np.pi)
    projection = 2*np.mean(output*np.exp(-1j*omega*np.arange(warm, warm+count)))
    window = np.hanning(count)
    windowed_projection = 2*np.sum(window*output*np.exp(-1j*omega*np.arange(warm, warm+count)))/np.sum(window)
    return {"frequencyHz": frequency, "amplitude": amplitude, "harmonic": harmonic,
            "warmSamples": warm, "measuredSamples": count,
            "lastHalfPhaseSpanRadians": float(np.ptp(tail)),
            "phaseSlipHz": float(frequency_error),
            "observedOscillatorHz": float(model.fs/np.mean(np.diff(crossings[len(crossings)//2:]))),
            "inputFrequencyPower": float(abs(projection)**2/2),
            "coherentReal": float(projection.real), "coherentImag": float(projection.imag),
            "windowedInputFrequencyPower": float(abs(windowed_projection)**2/2),
            "measurementStateDifference": float(np.max(np.abs(np.array([q1, q2, q3, q4])-measurement_start))),
            "inputPeriod": period,
            "totalPower": float(np.mean(output**2))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("build/results/fundamental-third/reduction.json"))
    parser.add_argument("--check-locking", action="store_true",
                        help="Read existing reduction results and compare finite-time forced trajectories")
    parser.add_argument("--check-edges", action="store_true",
                        help="Refine locking boundaries after --check-locking")
    args = parser.parse_args()
    if args.check_edges:
        checks = json.loads(args.output.with_name("locking.json").read_text())
        edges = []
        for row in json.loads(args.output.read_text()):
            if row["drive"] != 1:
                continue
            model = Model(row["model"], row["drive"], row["resonance"])
            orbit = row["orbits"][-1]
            for harmonic in [3, 5, 7]:
                width = orbit["phase"]["halfWidthHzAtAmplitudeQuarter"][str(harmonic)]
                center = harmonic*orbit["frequencyHz"]
                for side in [-1, 1]:
                    candidates = [c for c in checks if c["model"] == model.kind and
                                  c["harmonic"] == harmonic and c["amplitude"] == 0.25]
                    inside = next(c for c in candidates if c["normalizedOffset"] == side*0.8)
                    outside = next(c for c in candidates if c["normalizedOffset"] == side*1.2)
                    assert inside["lastHalfPhaseSpanRadians"] < 0.01
                    assert outside["lastHalfPhaseSpanRadians"] > 0.01
                    lo, hi = 0.8, 1.2
                    refinements = []
                    for _ in range(5):
                        middle = (lo+hi)/2
                        check = forced_check(model, 0.25, center+side*middle*width, harmonic)
                        check.update(model=model.kind, normalizedOffset=side*middle)
                        refinements.append(check)
                        if check["lastHalfPhaseSpanRadians"] < 0.01:
                            lo = middle
                        else:
                            hi = middle
                    edge = {"model": model.kind, "harmonic": harmonic, "side": side,
                            "predictedEdgeHz": center+side*width,
                            "observedEdgeBracketHz": sorted([center+side*lo*width, center+side*hi*width]),
                            "normalizedEdgeBracket": [lo, hi], "refinements": refinements}
                    edges.append(edge)
                    print(json.dumps(edge), flush=True)
        args.output.with_name("edges.json").write_text(json.dumps(edges, indent=2)+"\n")
        return
    if args.check_locking:
        checks = []
        for row in json.loads(args.output.read_text()):
            if row["drive"] != 1:
                continue
            model = Model(row["model"], row["drive"], row["resonance"])
            orbit = row["orbits"][-1]
            for amplitude in [0.125, 0.25]:
                for harmonic in [3, 5, 7]:
                    half_width = (amplitude/0.25 *
                                  orbit["phase"]["halfWidthHzAtAmplitudeQuarter"][str(harmonic)])
                    center = harmonic*orbit["frequencyHz"]
                    for offset in [-1.2, -0.8, 0, 0.8, 1.2]:
                        check = forced_check(model, amplitude, center+offset*half_width, harmonic)
                        check.update(model=model.kind, normalizedOffset=offset,
                                     predictedCenterHz=center, predictedHalfWidthHz=half_width)
                        checks.append(check)
                        print(json.dumps(check), flush=True)
        args.output.with_name("locking.json").write_text(json.dumps(checks, indent=2)+"\n")
        return
    results = []
    for kind in ["tanh", "rational"]:
        for drive, resonance in [(1, 0.75), (0.75, 0.75), (0.5, 1)]:
            model = Model(kind, drive, resonance)
            row = {"model": kind, "drive": drive, "resonance": resonance,
                   "gain": model.g, "feedback": model.k,
                   "jacobianFiniteDifferenceError": check_linearization(model), "orbits": []}
            previous = None
            for highest in [1, 3, 7, 15, 31]:
                previous = solve_orbit(model, highest, previous)
                previous["phase"] = phase_response(model, previous)
                row["orbits"].append(previous)
                print(json.dumps({k: v for k, v in previous.items() if k != "coefficients"} |
                                 {"model": kind, "drive": drive, "resonance": resonance}), flush=True)
            results.append(row)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2)+"\n")


if __name__ == "__main__":
    main()

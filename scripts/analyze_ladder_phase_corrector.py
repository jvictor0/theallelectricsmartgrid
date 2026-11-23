#!/usr/bin/env python3
"""Evaluate the proved finite-correction formula for the reduced phase curve.

Binary64 evaluation is a diagnostic, not a kernel certificate for these numbers.
HB-derived input coefficients remain unvalidated for the physical ladder. The
reported band covers reduced-phase sampling error only; it excludes HB waveform,
phase-model, fast-carrier averaging and reset-selection errors.
"""

import cmath
import json
import math
from pathlib import Path


def canonical_polynomial(coefficients, orientation, positive_detuning):
    q0, q1, q2 = coefficients
    turn = -1j * orientation
    transformed = (q0, q1 * turn, q2 * turn**2)
    return tuple(z.conjugate() for z in transformed) if positive_detuning else transformed


def budget(coefficients, c, h, order, phase_error=0.0):
    _, q1, q2 = coefficients
    a, b = q1 + q2 * (-1j*c), q2
    b0 = b1 = b2 = 0.0
    for ell in range(order):
        weight = c**ell
        b0 += weight*(abs(a)/(ell+1) + abs(b)/(ell+2))
        b1 += weight*(abs(a)+abs(b))
        b2 += weight*(ell*abs(a)+(ell+1)*abs(b))
    tail = (abs(q1)*(1+c)+abs(q2)*(1+c*c))*c**order
    displacement = h*(1+c)**2+phase_error
    euler_error = phase_error+displacement**2/2
    error = tail+b1*euler_error/h+b2*displacement**2/(2*h)
    return {'order': order, 'error': error, 'tail': tail,
            'samplingAndPhaseError': error-tail, 'endpointConstant': 2*b0/h}


def evaluate(poly, z):
    return poly[0]+poly[1]*z+poly[2]*z*z


def phase_measurement(polynomials, c, h, count, initial):
    angle = initial
    totals = [0j for _ in polynomials]
    for _ in range(count):
        z = cmath.exp(1j*angle)
        for j, poly in enumerate(polynomials):
            totals[j] += evaluate(poly, z)
        angle = math.remainder(angle+h*(1+c*c+2*c*z.imag), 2*math.pi)
    return [total/count for total in totals]


def main():
    root = Path(__file__).resolve().parents[1]
    records = json.loads((root/'build/results/ladder-driven-response/response.json').read_text())
    source = next(r for r in records if r['model'] == 'tanh' and r['drive'] == 1 and r['resonance'] == 0.75)
    prepared = source['prepared']
    amplitude, harmonic = 0.25, 3
    data = prepared['harmonics'][str(harmonic)]
    radius = math.hypot(data['frequencyCos'], data['frequencySin'])
    orientation = complex(data['frequencyCos'], data['frequencySin'])/radius
    y0, yc, ys = (complex(*data[k]) for k in ('orbitPhasor', 'cosPhasor', 'sinPhasor'))
    coherent = (amplitude/2*(yc+1j*ys), y0, amplitude/2*(yc-1j*ys))
    power = (complex(prepared['totalPower']+amplitude**2/2*(data['powerCos']+data['powerSin'])),
             2*amplitude*complex(data['powerCrossCos'], -data['powerCrossSin']),
             amplitude**2*complex((data['powerCos']-data['powerSin'])/2, -data['powerCosSin']))
    polynomials = [canonical_polynomial(p, orientation, True) for p in (coherent, power)]
    count = 131072
    rows = []
    for ratio in (1.01, 1.05, 1.1, 1.2, 1.5, 2.0, 3.0):
        c = 1/(ratio+math.sqrt(ratio*ratio-1))
        h = harmonic*amplitude*radius/(2*c)
        predictions = [evaluate(p, -1j*c) for p in polynomials]
        bounds = [min((budget(p, c, h, n) for n in (8, 16, 32, 64, 128, 256, 512)),
                      key=lambda b: b['error']) for p in polynomials]
        checks = []
        for initial in (0.0, 1.3, -2.0):
            measured = phase_measurement(polynomials, c, h, count, initial)
            errors = [abs(a-b) for a, b in zip(measured, predictions)]
            finite_bounds = [b['error']+b['endpointConstant']/count for b in bounds]
            assert all(e <= b for e, b in zip(errors, finite_bounds)), (ratio, initial, errors, finite_bounds)
            checks.append({'initialPhase': initial, 'observedErrors': errors, 'finiteBounds': finite_bounds})
        rows.append({'ratio': ratio, 'c': c, 'h': h,
                     'frequencyHz': harmonic*(prepared['frequencyHz']+ratio*amplitude*radius*192000/(2*math.pi)),
                     'coherentMagnitude': abs(predictions[0]), 'power': predictions[1].real,
                     'coherentBudget': bounds[0], 'powerBudget': bounds[1], 'phaseModelChecks': checks})
    out = root/'build/results/ladder-phase-corrector'
    out.mkdir(parents=True, exist_ok=True)
    report = {'scope': __doc__, 'model': 'exact-tanh HB proposals; discrete reduced-phase model',
              'amplitude': amplitude, 'harmonic': harmonic, 'drive': 1, 'resonance': 0.75,
              'sampleCount': count, 'rows': rows}
    (out/'phase-budgets.json').write_text(json.dumps(report, indent=2)+'\n')
    for row in rows:
        print(f"r={row['ratio']:.2f}  |C|={row['coherentMagnitude']:.6f}  "
              f"C error <= {row['coherentBudget']['error']:.6g}  P={row['power']:.6f}  "
              f"P error <= {row['powerBudget']['error']:.6g}")


if __name__ == '__main__':
    main()

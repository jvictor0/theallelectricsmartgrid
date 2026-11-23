#!/usr/bin/env python3
"""Driven waveform expansion about an autonomous harmonic-balance orbit.

The bordered derivative retains frequency pulling and fixes only the phase
gauge. The quadratic observer uses stationary phase moments through order four.
No measured response is used to fit coefficients. This is a numerical research
probe, not a production evaluator or a certified approximation.
"""

import json
import math
from pathlib import Path

import numpy as np
from scipy.linalg import solve
from scipy.optimize import brentq, root, minimize_scalar
from scipy.integrate import quad_vec

from analyze_ladder_harmonic_reduction import Model, basis


def second_derivative(model, x):
    t = model.g*x
    if model.kind == 'tanh':
        value = -2*np.tanh(t)*(1-np.tanh(t)**2)
    else:
        value = np.where(np.abs(t) < 3, (16/3)*t*(t*t-9)/(t*t+3)**3, 0)
    return model.g**2*value/model.normalization


def check_second_derivative(model):
    # A centered difference of S' is independent of the implemented formula
    # for S''. Clipping joins are C2, so their centered error is first order.
    x = np.linspace(-4/model.g, 4/model.g, 1001)
    eps = 1e-5/model.g
    numeric = (model.derivative(x+eps)-model.derivative(x-eps))/(2*eps)
    error = float(np.max(np.abs(numeric-second_derivative(model, x))))
    scaled_error = error*model.normalization/model.g**2
    assert scaled_error < 5e-7, (model.kind, scaled_error)
    return error


def prepare(model, orbit, targets=(3, 5, 7), check_edge=False):
    hs = np.arange(1, orbit['highest']+1, 2)
    theta = 2*np.pi*np.arange(2048)/2048
    b = basis(theta, hs)
    past_b = basis(theta-orbit['omega'], hs)
    coeff = np.array(orbit['coefficients'])
    q = b@coeff
    past_tangent = basis(theta-orbit['omega'], hs, True)@coeff
    past_second = past_b@(-np.repeat(hs**2, 2)[:, None]*coeff)
    projection = 2/len(theta)*b.T
    # Differentiate the exact residual in the current-stage equations. The
    # previous fourth stage appears only in the feedback term of stage one.
    point_operator = np.einsum('na,ij->niaj', b-model.b*past_b, np.eye(4))
    point_operator[:, 0, :, 3] += model.a*model.k*past_b
    for j in range(1, 4):
        point_operator[:, j, :, j-1] -= model.a*model.derivative(q[:, j-1])[:, None]*b
    operator = np.einsum('ln,niaj->liaj', projection, point_operator).reshape(4*b.shape[1], -1)/model.a
    frequency_column = model.b*past_tangent
    frequency_column[:, 0] -= model.a*model.k*past_tangent[:, 3]
    frequency_column = (projection@frequency_column).ravel()/model.a
    bordered = np.zeros((len(operator)+1, len(operator)+1))
    bordered[:-1, :-1] = operator
    bordered[:-1, -1] = frequency_column
    bordered[-1, 7] = 1
    forcing = np.zeros((len(bordered), 2*len(targets)))
    for index, harmonic in enumerate(targets):
        location = int(np.where(hs == harmonic)[0][0])*2
        forcing[location*4, index*2] = 1
        forcing[(location+1)*4, index*2+1] = 1
    corrections = solve(bordered, forcing)
    solve_defect = float(np.max(np.abs(bordered@corrections-forcing)))
    assert solve_defect < 1e-8, solve_defect
    y = (1+model.k)*model.sat(q[:, 3])
    y_slope = (1+model.k)*model.derivative(q[:, 3])
    result = {'solveResidual': solve_defect, 'conditionNumber': float(np.linalg.cond(bordered)),
              'frequencyHz': orbit['frequencyHz'], 'totalPower': float(np.mean(y*y)), 'harmonics': {}}
    for index, harmonic in enumerate(targets):
        correction = corrections[:, index*2:index*2+2]
        dq_cos = b@correction[:-1, 0].reshape(-1, 4)
        dq_sin = b@correction[:-1, 1].reshape(-1, 4)
        yc, ys = y_slope*dq_cos[:, 3], y_slope*dq_sin[:, 3]
        phasor = lambda signal: 2*np.mean(signal*np.exp(-1j*harmonic*theta))
        frequency_cos, frequency_sin = correction[-1]
        def quadratic_residual(direction):
            dq = b@direction[:-1].reshape(-1, 4)
            dq_past_deriv = basis(theta-orbit['omega'], hs, True)@direction[:-1].reshape(-1, 4)
            nu = direction[-1]
            residual = model.b*nu*dq_past_deriv-0.5*model.b*nu*nu*past_second
            residual[:, 0] += (-model.a*model.k*nu*dq_past_deriv[:, 3]+
                               0.5*model.a*model.k*nu*nu*past_second[:, 3])
            residual[:, 1:] -= 0.5*model.a*second_derivative(model, q[:, :3])*dq[:, :3]**2
            return np.r_[(projection@residual).ravel()/model.a, 0]
        rhs_cc = quadratic_residual(correction[:, 0])
        rhs_ss = quadratic_residual(correction[:, 1])
        rhs_cs = quadratic_residual(correction[:, 0]+correction[:, 1])-rhs_cc-rhs_ss
        quadratic = solve(bordered, -np.column_stack([rhs_cc, rhs_ss, rhs_cs]))
        qcc, qss, qcs = [b@quadratic[:-1, j].reshape(-1, 4) for j in range(3)]
        y_second = (1+model.k)*second_derivative(model, q[:, 3])
        ycc = y_slope*qcc[:, 3]+0.5*y_second*dq_cos[:, 3]**2
        yss = y_slope*qss[:, 3]+0.5*y_second*dq_sin[:, 3]**2
        ycs = y_slope*qcs[:, 3]+y_second*dq_cos[:, 3]*dq_sin[:, 3]
        observables = np.column_stack([y, yc, ys, ycc, yss, ycs])
        # For a running phase the previous sample also has a different phase
        # mismatch. Retain this transport term in the backward invariance
        # equation; d=(omega/m-Omega)/A is the scaled detuning.
        def history_operator(direction):
            past = past_b@direction[:-1].reshape(-1, 4)
            value = model.b*past
            value[:, 0] -= model.a*model.k*past[:, 3]
            return np.r_[(projection@value).ravel()/model.a, 0]
        hc, hs_response = history_operator(correction[:, 0]), history_operator(correction[:, 1])
        transport_rhs = harmonic*np.column_stack([
            frequency_cos*hs_response, -frequency_sin*hc,
            frequency_sin*hs_response-frequency_cos*hc, -hs_response, hc])
        transport = solve(bordered, -transport_rhs)
        moving_quadratic = np.column_stack([quadratic, np.zeros((len(bordered), 2))])+transport
        moving_y = [y_slope*(b@moving_quadratic[:-1, j].reshape(-1, 4))[:, 3] for j in range(5)]
        moving_y[0] += 0.5*y_second*dq_cos[:, 3]**2
        moving_y[1] += 0.5*y_second*dq_sin[:, 3]**2
        moving_y[2] += y_second*dq_cos[:, 3]*dq_sin[:, 3]
        moving_observables = np.column_stack([y, yc, ys]+moving_y)
        # The invariance equation uses a backward phase increment. Its
        # continuous modified flow has the additional +v'*v/2 term.
        flow_correction = harmonic/2*np.array([
            frequency_cos*frequency_sin, -frequency_cos*frequency_sin,
            frequency_sin**2-frequency_cos**2, -frequency_sin, frequency_cos])
        # Independent directional check of the residual expansion at fixed phase.
        phase = 0.37
        first = correction[:, 0]*np.cos(phase)+correction[:, 1]*np.sin(phase)
        second = quadratic@np.array([np.cos(phase)**2, np.sin(phase)**2, np.cos(phase)*np.sin(phase)])
        def projected_residual(amplitude, order):
            trial = coeff+amplitude*first[:-1].reshape(-1, 4)
            frequency = orbit['omega']+amplitude*first[-1]
            if order == 2:
                trial += amplitude**2*second[:-1].reshape(-1, 4)
                frequency += amplitude**2*second[-1]
            current, past = b@trial, basis(theta-frequency, hs)@trial
            residual = current-model.b*past
            residual[:, 0] += model.a*(model.k*past[:, 3]-amplitude*np.cos(harmonic*theta-phase))
            residual[:, 1:] -= model.a*model.sat(current[:, :3])
            return float(np.linalg.norm(projection@residual)/model.a)
        residual_checks = {str(a): [projected_residual(a, order) for order in [1, 2]] for a in [0.01, 0.005, 0.0025]}
        # Verify the transport expansion against the discrete backward map,
        # before the separate continuous modified-flow approximation. This
        # check exercises changing phase mismatch, not a frozen-phase solve.
        scaled_detuning = 0.6*np.hypot(frequency_cos, frequency_sin)
        def moving_residual(amplitude):
            def at_phase(phi):
                c, s = np.cos(phi), np.sin(phi)
                first = correction@np.array([c, s])
                second = moving_quadratic@np.array([c*c, s*s, c*s,
                                                    scaled_detuning*c, scaled_detuning*s])
                return (coeff+amplitude*first[:-1].reshape(-1, 4)+
                        amplitude**2*second[:-1].reshape(-1, 4),
                        orbit['omega']+amplitude*first[-1]+amplitude**2*second[-1])
            current_coeff, increment = at_phase(phase)
            previous_phase = phase-harmonic*(increment-orbit['omega']-amplitude*scaled_detuning)
            past_coeff, _ = at_phase(previous_phase)
            current = b@current_coeff
            past = basis(theta-increment, hs)@past_coeff
            residual = current-model.b*past
            residual[:, 0] += model.a*(model.k*past[:, 3]-amplitude*np.cos(harmonic*theta-phase))
            residual[:, 1:] -= model.a*model.sat(current[:, :3])
            return float(np.linalg.norm(projection@residual)/model.a)
        moving_checks = {str(a): moving_residual(a) for a in [0.01, 0.005, 0.0025]}
        for values in [[v[1] for v in residual_checks.values()], list(moving_checks.values())]:
            ratios = np.array(values[:-1])/np.array(values[1:])
            assert np.all((ratios > 7.5) & (ratios < 8.5)), ('non-cubic projected residual', ratios)
        edge_check = None
        if check_edge and harmonic == 7:
            amplitude = 0.25
            def branch(phi, full):
                c, s = np.cos(phi), np.sin(phi)
                first = correction@np.array([c, s])
                second = quadratic@np.array([c*c, s*s, c*s])
                seed = np.r_[(coeff+amplitude*first[:-1].reshape(-1, 4)+
                              amplitude**2*second[:-1].reshape(-1, 4)).ravel(),
                             (orbit['omega']+amplitude*first[-1]+amplitude**2*second[-1])/model.nominal]
                if not full:
                    return seed[-1]*model.nominal
                def residual(v):
                    candidate = v[:-1].reshape(-1, 4)
                    current, past = b@candidate, basis(theta-v[-1]*model.nominal, hs)@candidate
                    value = current-model.b*past
                    value[:, 0] += model.a*(model.k*past[:, 3]-amplitude*np.cos(harmonic*theta-phi))
                    value[:, 1:] -= model.a*model.sat(current[:, :3])
                    return np.r_[(projection@value).ravel()/model.a, candidate[1, 3]]
                def jacobian(v):
                    candidate = v[:-1].reshape(-1, 4)
                    omega = v[-1]*model.nominal
                    shifted = basis(theta-omega, hs)
                    current = b@candidate
                    op = np.einsum('na,ij->niaj', b-model.b*shifted, np.eye(4))
                    op[:, 0, :, 3] += model.a*model.k*shifted
                    for j in range(1, 4):
                        op[:, j, :, j-1] -= model.a*model.derivative(current[:, j-1])[:, None]*b
                    matrix = np.zeros_like(bordered)
                    matrix[:-1, :-1] = np.einsum('ln,niaj->liaj', projection, op).reshape(len(operator), -1)/model.a
                    tangent = basis(theta-omega, hs, True)@candidate
                    column = model.b*tangent
                    column[:, 0] -= model.a*model.k*tangent[:, 3]
                    matrix[:-1, -1] = (projection@column).ravel()*model.nominal/model.a
                    matrix[-1, 7] = 1
                    return matrix
                solution = root(residual, seed, jac=jacobian, tol=1e-10)
                defect = float(np.max(np.abs(residual(solution.x))))
                if defect > 1e-9:
                    raise RuntimeError(('forced harmonic balance failed', solution.message, defect))
                return solution.x[-1]*model.nominal
            approx_phase = np.arctan2(frequency_sin, frequency_cos)+np.pi
            bounds = (approx_phase-0.5, approx_phase+0.5)
            approximated = minimize_scalar(lambda phi: branch(phi, False), bounds=bounds, method='bounded',
                                          options={'xatol': 1e-10})
            complete = minimize_scalar(lambda phi: branch(phi, True), bounds=bounds, method='bounded',
                                       options={'xatol': 1e-8})
            edge_check = {'quadraticLowerEdgeHz': float(approximated.fun*harmonic*model.fs/(2*np.pi)),
                          'fullProjectedLowerEdgeHz': float(complete.fun*harmonic*model.fs/(2*np.pi)),
                          'amplitude': amplitude,
                          'note': 'Finite projected periodic branch turning point, not an attraction certificate.'}
        result['harmonics'][str(harmonic)] = {
            'frequencyCos': float(frequency_cos), 'frequencySin': float(frequency_sin),
            'halfWidthHzAtAmplitudeQuarter': float(model.fs/(2*np.pi)*harmonic*0.25*np.hypot(frequency_cos, frequency_sin)),
            'orbitPhasor': [float(phasor(y).real), float(phasor(y).imag)],
            'cosPhasor': [float(phasor(yc).real), float(phasor(yc).imag)],
            'sinPhasor': [float(phasor(ys).real), float(phasor(ys).imag)],
            'powerCrossCos': float(np.mean(y*yc)), 'powerCrossSin': float(np.mean(y*ys)),
            'powerCos': float(np.mean(yc*yc)), 'powerSin': float(np.mean(ys*ys)),
            'powerCosSin': float(np.mean(yc*ys)),
            'frequencyQuadratic': quadratic[-1].tolist(),
            'quadraticPhasors': [[float(phasor(v).real), float(phasor(v).imag)] for v in [ycc, yss, ycs]],
            'powerGram': (observables.T@observables/len(theta)).tolist(),
            'projectedResidualChecks': residual_checks,
            'movingProjectedResidualChecks': moving_checks,
            'edgeCheck': edge_check,
            'moving': {
                'frequencyQuadratic': (moving_quadratic[-1]+flow_correction).tolist(),
                'quadraticPhasors': [[float(phasor(v).real), float(phasor(v).imag)] for v in moving_y],
                'powerGram': (moving_observables.T@moving_observables/len(theta)).tolist(),
            },
        }
    return result


def evaluate(model, prepared, harmonic, amplitude, frequency):
    h = prepared['harmonics'][str(harmonic)]
    fc, fs = h['frequencyCos'], h['frequencySin']
    radius = math.hypot(fc, fs)
    detuning = 2*math.pi/model.fs*(frequency/harmonic-prepared['frequencyHz'])
    r = detuning/(amplitude*radius)
    orientation = complex(fc, fs)/radius
    if abs(r) <= 1:
        moment = orientation*complex(r, math.sqrt(1-r*r))
    else:
        moment = orientation*math.copysign(1/(abs(r)+math.sqrt(r*r-1)), r)
    second_moment = moment*moment
    y0, yc, ys = [complex(*h[key]) for key in ['orbitPhasor', 'cosPhasor', 'sinPhasor']]
    oscillator = y0*moment
    driven = amplitude/2*((yc-1j*ys)*second_moment+yc+1j*ys)
    coherent = oscillator+driven
    power = prepared['totalPower']+2*amplitude*(h['powerCrossCos']*moment.real+h['powerCrossSin']*moment.imag)
    power += amplitude**2*(0.5*(h['powerCos']+h['powerSin'])+
                          0.5*(h['powerCos']-h['powerSin'])*second_moment.real+
                          h['powerCosSin']*second_moment.imag)
    return {'normalizedDetuning': r, 'coherentPower': abs(coherent)**2/2,
            'phaseOnlyPower': abs(oscillator)**2/2, 'totalPower': power,
            'coherentReal': coherent.real, 'coherentImag': coherent.imag,
            'oscillatorReal': oscillator.real, 'oscillatorImag': oscillator.imag,
            'drivenReal': driven.real, 'drivenImag': driven.imag}


def evaluate_second(model, prepared, harmonic, amplitude, frequency, moving=False):
    h = prepared['harmonics'][str(harmonic)]
    nu = np.array([h['frequencyCos'], h['frequencySin']])
    quadratic = np.array(h['frequencyQuadratic'])
    detuning = 2*np.pi/model.fs*(frequency/harmonic-prepared['frequencyHz'])
    if moving:
        quadratic = np.array(h['moving']['frequencyQuadratic'])
        nu += detuning*quadratic[3:]
        quadratic = quadratic[:3]
    def drift(phi):
        c, s = np.cos(phi), np.sin(phi)
        return amplitude*(nu[0]*c+nu[1]*s)+amplitude**2*(quadratic[0]*c*c+quadratic[1]*s*s+quadratic[2]*c*s)-detuning
    def derivative(phi):
        c, s = np.cos(phi), np.sin(phi)
        return amplitude*(-nu[0]*s+nu[1]*c)+amplitude**2*(2*(quadratic[1]-quadratic[0])*c*s+quadratic[2]*(c*c-s*s))
    grid = np.linspace(0, 2*np.pi, 1025)
    values = drift(grid)
    roots = [brentq(drift, lo, hi, xtol=1e-14) for lo, hi, a, b in
             zip(grid[:-1], grid[1:], values[:-1], values[1:]) if a*b < 0]
    stable = [phi for phi in roots if derivative(phi) < 0]
    if roots:
        assert len(stable) == 1, ('multiple stable branches', stable)
        moments = np.exp(1j*np.arange(5)*stable[0])
        integration_error = 0
    else:
        integrals, integration_error = quad_vec(lambda phi: np.exp(1j*np.arange(5)*phi)/abs(drift(phi)),
                                               0, 2*np.pi, epsabs=1e-8, epsrel=1e-10)
        moments = integrals/integrals[0]
    m1, m2, m3 = moments[1:4]
    y0, yc, ys = [complex(*h[key]) for key in ['orbitPhasor', 'cosPhasor', 'sinPhasor']]
    power_gram = np.array(h['powerGram'])
    ycc, yss, ycs = map(lambda p: complex(*p), h['quadraticPhasors'])
    if moving:
        ycc, yss, ycs, ydc, yds = map(lambda p: complex(*p), h['moving']['quadraticPhasors'])
        yc += detuning*ydc
        ys += detuning*yds
        transform = np.vstack([np.eye(6), np.zeros((2, 6))])
        transform[6, 1] = detuning
        transform[7, 2] = detuning
        power_gram = transform.T@np.array(h['moving']['powerGram'])@transform
    coherent = y0*m1+amplitude/2*((yc-1j*ys)*m2+yc+1j*ys)
    coherent += amplitude**2/4*(ycc*(m3+2*m1+m1.conjugate())+
                                yss*(-m3+2*m1-m1.conjugate())+
                                ycs*(m3-m1.conjugate())/1j)
    # Fourier coefficients in phi, indices -2..2, of 1, cos, sin, cos^2,
    # sin^2 and cos*sin. Their products require moments only through order four.
    basis_coeff = np.array([[0, 0, 1, 0, 0], [0, .5, 0, .5, 0], [0, .5j, 0, -.5j, 0],
                           [.25, 0, .5, 0, .25], [-.25, 0, .5, 0, -.25], [.25j, 0, 0, 0, -.25j]])
    basis_coeff *= np.array([1, amplitude, amplitude, amplitude**2, amplitude**2, amplitude**2])[:, None]
    power_coeff = sum(power_gram[i, j]*np.convolve(basis_coeff[i], basis_coeff[j]) for i in range(6) for j in range(6))
    all_moments = np.r_[moments[:0:-1].conjugate(), moments]
    power = float(np.real(power_coeff@all_moments))
    return {'coherentPower': abs(coherent)**2/2, 'totalPower': power,
            'coherentReal': coherent.real, 'coherentImag': coherent.imag,
            'branch': 'locked' if roots else 'running', 'phase': stable[0] if stable else None,
            'momentQuadratureError': float(integration_error)}


def main():
    root = Path(__file__).resolve().parents[1]
    rows = json.loads((root/'build/results/fundamental-third/reduction.json').read_text())
    data = root/'build/results/ladder-power-response'
    measurements = {name: json.loads((data/f'{name}.json').read_text()) for name in ['power', 'refine', 'periodic']}
    out = root/'build/results/ladder-driven-response'
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for row in rows:
        model = Model(row['model'], row['drive'], row['resonance'])
        prepared = prepare(model, row['orbits'][-1],
                           check_edge=(model.kind == 'rational' and row['drive'] == 0.75))
        result = {'model': model.kind, 'drive': row['drive'], 'resonance': row['resonance'],
                  'prepared': prepared, 'secondDerivativeCheckError': check_second_derivative(model), 'checks': []}
        for source, checks in measurements.items():
            for check in checks:
                if (check['model'], check['drive'], check['resonance']) != (model.kind, row['drive'], row['resonance']):
                    continue
                prediction = evaluate(model, prepared, check['harmonic'], check['amplitude'], check['frequencyHz'])
                second = evaluate_second(model, prepared, check['harmonic'], check['amplitude'], check['frequencyHz'])
                moving = evaluate_second(model, prepared, check['harmonic'], check['amplitude'], check['frequencyHz'], moving=True)
                measured = check.get('windowedInputFrequencyPower', check['inputFrequencyPower'])
                prediction.update(source=source, frequencyHz=check['frequencyHz'], amplitude=check['amplitude'],
                                  harmonic=check['harmonic'], measuredPower=measured,
                                  coherentErrorDb=10*math.log10(prediction['coherentPower']/measured),
                                  totalErrorDb=10*math.log10(prediction['totalPower']/check['totalPower']))
                prediction['second'] = second | {'coherentErrorDb': 10*math.log10(second['coherentPower']/measured),
                                                 'totalErrorDb': 10*math.log10(second['totalPower']/check['totalPower'])}
                prediction['moving'] = moving | {'coherentErrorDb': 10*math.log10(moving['coherentPower']/measured),
                                                 'totalErrorDb': 10*math.log10(moving['totalPower']/check['totalPower'])}
                if 'coherentReal' in check:
                    prediction['phasorError'] = abs(complex(prediction['coherentReal'], prediction['coherentImag'])-
                                                     complex(check['coherentReal'], check['coherentImag']))
                    prediction['second']['phasorError'] = abs(complex(second['coherentReal'], second['coherentImag'])-
                                                              complex(check['coherentReal'], check['coherentImag']))
                result['checks'].append(prediction)
        print(json.dumps(result), flush=True)
        results.append(result)
    (out/'response.json').write_text(json.dumps(results, indent=2)+'\n')


if __name__ == '__main__':
    main()

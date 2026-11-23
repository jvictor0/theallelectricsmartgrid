#!/usr/bin/env python3
"""Probe interval wrapping and certificate size for the drifting ladder.

Ordinary arithmetic is widened by nextafter. Transcendental endpoints are
padded binary64 proposals: this is NOT a verified interval library or a Lean
certificate. Successful margins must be replayed with proved real bounds.
"""

import argparse
import json
from pathlib import Path
import numpy as np
from analyze_ladder_drift_attraction import Chart, advance
from analyze_ladder_drift_averages import observe_block
from analyze_ladder_harmonic_reduction import Model
from analyze_ladder_harmonic_reduction import basis


class Box:
    def __init__(self, lo, hi=None):
        self.lo = np.asarray(lo, dtype=float)
        self.hi = self.lo if hi is None else np.asarray(hi, dtype=float)
        assert np.all(self.lo <= self.hi)

    @staticmethod
    def of(x):
        return x if isinstance(x, Box) else Box(x)

    def __getitem__(self, key):
        return Box(self.lo[key], self.hi[key])

    def __add__(self, other):
        other = Box.of(other)
        return Box(np.nextafter(self.lo+other.lo, -np.inf), np.nextafter(self.hi+other.hi, np.inf))

    __radd__ = __add__

    def __neg__(self):
        return Box(-self.hi, -self.lo)

    def __sub__(self, other):
        return self+-Box.of(other)

    def __rsub__(self, other):
        return Box.of(other)+-self

    def __mul__(self, other):
        other = Box.of(other)
        products = np.stack(np.broadcast_arrays(self.lo*other.lo, self.lo*other.hi,
                                                self.hi*other.lo, self.hi*other.hi))
        return Box(np.nextafter(np.min(products, axis=0), -np.inf),
                   np.nextafter(np.max(products, axis=0), np.inf))

    __rmul__ = __mul__

    def __truediv__(self, other):
        other = Box.of(other)
        assert np.all((other.lo > 0) | (other.hi < 0)), 'division straddles zero'
        return self*Box(np.nextafter(1/other.hi, -np.inf), np.nextafter(1/other.lo, np.inf))

    def square(self):
        magnitude = np.maximum(abs(self.lo), abs(self.hi))
        minimum = np.where((self.lo <= 0) & (self.hi >= 0), 0, np.minimum(abs(self.lo), abs(self.hi)))
        return Box(np.maximum(0, np.nextafter(minimum*minimum, -np.inf)), np.nextafter(magnitude*magnitude, np.inf))

    def sqrt(self):
        assert np.all(self.lo >= 0)
        return Box(np.maximum(0, np.nextafter(np.sqrt(self.lo), -np.inf)), np.nextafter(np.sqrt(self.hi), np.inf))

    def magnitude(self):
        return np.maximum(abs(self.lo), abs(self.hi))

    def sum(self, axis):
        # Sequential interval additions avoid relying on NumPy's reduction order.
        lo, hi = np.moveaxis(self.lo, axis, 0), np.moveaxis(self.hi, axis, 0)
        value = Box(np.zeros(lo.shape[1:]))
        for a, b in zip(lo, hi):
            value = value+Box(a, b)
        return value


def stack(values, axis=-1):
    return Box(np.stack([v.lo for v in values], axis=axis), np.stack([v.hi for v in values], axis=axis))


def product(a, b):
    return (a[..., :, :, None]*b[..., None, :, :]).sum(-2)


def trig(x, sine=False):
    center = (x.lo+x.hi)/2
    radius = (x.hi-x.lo)/2+2e-14
    value = np.sin(center) if sine else np.cos(center)
    return Box(np.maximum(-1, value-radius), np.minimum(1, value+radius))


def saturation(x, model):
    return Box(np.maximum(-1, np.tanh(model.g*x.lo)-2e-14),
               np.minimum(1, np.tanh(model.g*x.hi)+2e-14))/model.normalization


def slope(x, model):
    t = Box(np.maximum(-1, np.tanh(model.g*x.lo)-2e-14),
            np.minimum(1, np.tanh(model.g*x.hi)+2e-14))
    return model.g*(1-t.square())/model.normalization


def curvature(x, model):
    t = Box(np.maximum(-1, np.tanh(model.g*x.lo)-2e-14),
            np.minimum(1, np.tanh(model.g*x.hi)+2e-14))
    return -2*model.g**2*t*(1-t.square())/model.normalization


def profile(chart, theta, derivative=0):
    keep = abs(chart.modes) <= 80
    modes = chart.modes[keep]
    coeff = chart.coefficients[keep]*(1j*modes[:, None])**derivative
    phase = theta[..., None]*modes
    return (trig(phase)[..., :, None]*coeff.real-trig(phase, True)[..., :, None]*coeff.imag).sum(-2)


def potential(proposal, theta, derivative=0):
    coefficients = np.asarray(proposal['potential'])
    modes = np.arange(1, proposal['highest']+1)
    coeff = (coefficients[::2]-1j*coefficients[1::2])*(1j*modes[:, None])**derivative
    phase = theta[..., None]*modes
    return (trig(phase)[..., :, None]*coeff.real-trig(phase, True)[..., :, None]*coeff.imag).sum(-2)


def decode_derivative(chart, theta, radius):
    p = profile(chart, theta)+Box([-radius, -radius, -radius], [radius, radius, radius])
    d = profile(chart, theta, 1)
    c, s, zero, one = trig(theta), trig(theta, True), Box(np.zeros_like(theta.lo)), Box(np.ones_like(theta.lo))
    matrix = stack([stack([d[..., 0]*c-p[..., 0]*s, c, zero, zero]),
                    stack([d[..., 0]*s+p[..., 0]*c, s, zero, zero]),
                    stack([d[..., 1], zero, one, zero]),
                    stack([d[..., 2], zero, zero, one])], -2)
    return product(Box(chart.inverse), matrix)


def decode_hessian(chart, theta, radius):
    p, d, dd = profile(chart, theta), profile(chart, theta, 1), profile(chart, theta, 2)
    c, s = trig(theta), trig(theta, True)
    zero = Box(np.zeros_like(theta.lo))
    def matrix(first, mixed):
        return stack([stack([first, mixed, zero, zero]), stack([mixed, zero, zero, zero]),
                      stack([zero, zero, zero, zero]), stack([zero, zero, zero, zero])], -2)
    h = stack([matrix(dd[..., 0]*c-2*d[..., 0]*s-(p[..., 0]+Box(-radius, radius))*c, -s),
               matrix(dd[..., 0]*s+2*d[..., 0]*c-(p[..., 0]+Box(-radius, radius))*s, c),
               matrix(dd[..., 1], zero), matrix(dd[..., 2], zero)], -3)
    return (Box(chart.inverse)[..., :, :, None, None]*h[..., None, :, :, :]).sum(-3)


def encode_hessian(chart, z, phi):
    x, y = z[..., 0], z[..., 1]
    r2 = x.square()+y.square()
    r, r4 = r2.sqrt(), r2.square()
    zero = Box(np.zeros_like(x.lo))
    angle = stack([-y/r2, x/r2, zero, zero])
    ah = stack([stack([2*x*y/r4, (y.square()-x.square())/r4, zero, zero]),
                stack([(y.square()-x.square())/r4, -2*x*y/r4, zero, zero]),
                stack([zero, zero, zero, zero]), stack([zero, zero, zero, zero])], -2)
    rh = stack([stack([y.square()/(r2*r), -x*y/(r2*r), zero, zero]),
                stack([-x*y/(r2*r), x.square()/(r2*r), zero, zero]),
                stack([zero, zero, zero, zero]), stack([zero, zero, zero, zero])], -2)
    d, dd = profile(chart, phi, 1), profile(chart, phi, 2)
    aa = angle[..., :, None]*angle[..., None, :]
    zero_matrix = Box(np.zeros_like(ah.lo))
    return stack([ah]+[(rh if j==0 else zero_matrix)-dd[..., j, None, None]*aa-d[..., j, None, None]*ah
                       for j in range(3)], -3)


def encode_boxes(chart, states):
    z = product(Box(chart.transform), states[..., :, None])[..., :, 0]
    zcenter = ((states.lo+states.hi)/2)@chart.transform.T
    R2 = z[..., 0].square()+z[..., 1].square()
    assert np.all(R2.lo > 0), 'polar radius contains zero'
    R = R2.sqrt()
    width = np.maximum(abs(z.lo-zcenter), abs(z.hi-zcenter))
    angle_width = (z[..., 1].magnitude()*width[..., 0]+z[..., 0].magnitude()*width[..., 1])/R2.lo
    phi_center = np.arctan2(zcenter[..., 1], zcenter[..., 0])
    phi = Box(phi_center-angle_width-2e-14, phi_center+angle_width+2e-14)
    d = profile(chart, phi, 1)
    zero, one = Box(np.zeros_like(phi.lo)), Box(np.ones_like(phi.lo))
    angle = stack([-z[..., 1]/R2, z[..., 0]/R2, zero, zero])
    derivative = stack([angle, stack([z[..., 0]/R, z[..., 1]/R, zero, zero])-d[..., 0, None]*angle,
                        stack([zero, zero, one, zero])-d[..., 1, None]*angle,
                        stack([zero, zero, zero, one])-d[..., 2, None]*angle], -2)
    normal = stack([R, z[..., 2], z[..., 3]])-profile(chart, phi)
    return z, phi, derivative, normal


def probe(chart, model, cells, radius, chunk=256, point_check=False, measurement=None):
    half_width = 1e-9 if point_check else np.pi/cells
    maxima = dict(a=np.inf, b=0., c=0., d=0., image=0., defect=0., alignment=np.inf)
    residual_bound = np.zeros(3)
    forcing = 0.25*np.cos(2*np.pi*np.arange(162)/54)
    for first in range(0, cells, chunk):
        theta = 2*np.pi*(np.arange(first, min(first+chunk, cells))+0.5)/cells
        if point_check:
            theta = np.array([0.3, 1.7, 3.2, 5.8])
        theta_box = Box(theta-half_width, theta+half_width)
        jac = decode_derivative(chart, theta_box, radius)
        center_jac_box = decode_derivative(chart, Box(theta), 0)
        hessian = decode_hessian(chart, theta_box, radius)
        initial = chart.decode(theta, np.zeros((len(theta), 3)))
        center = Box(initial-2e-13, initial+2e-13)
        delta = Box(-np.array([half_width, radius, radius, radius]), np.array([half_width, radius, radius, radius]))
        observation = Box(np.zeros((len(theta), 3)))
        observation_jac = Box(np.zeros((len(theta), 3, 4)))
        observation_hessian = Box(np.zeros((len(theta), 3, 4, 4)))
        for sample, u in enumerate(forcing):
            center_rows = [model.b*center[..., 0]+model.a*(u-model.k*center[..., 3])]
            center_jac_rows = [model.b*center_jac_box[..., 0, :]-model.a*model.k*center_jac_box[..., 3, :]]
            hrows = [model.b*hessian[..., 0, :, :]-model.a*model.k*hessian[..., 3, :, :]]
            rows = [model.b*jac[..., 0, :]-model.a*model.k*jac[..., 3, :]]
            states = [center_rows[0]+(rows[0]*delta).sum(-1)]
            for stage in range(1, 4):
                center_jac_rows.append(model.b*center_jac_box[..., stage, :]+
                                       model.a*slope(center_rows[-1], model)[..., None]*center_jac_rows[-1])
                center_rows.append(model.b*center[..., stage]+model.a*saturation(center_rows[-1], model))
                hrows.append(model.b*hessian[..., stage, :, :]+model.a*(
                    slope(states[-1], model)[..., None, None]*hrows[-1]+
                    curvature(states[-1], model)[..., None, None]*rows[-1][..., :, None]*rows[-1][..., None, :]))
                rows.append(model.b*jac[..., stage, :]+model.a*slope(states[-1], model)[..., None]*rows[-1])
                states.append(center_rows[-1]+(rows[-1]*delta).sum(-1))
            center, full, jac = stack(center_rows), stack(states), stack(rows, -2)
            center_jac_box = stack(center_jac_rows, -2)
            hessian = stack(hrows, -3)
            if measurement is not None:
                output = (1+model.k)*saturation(full[..., 3], model)
                output_center = (1+model.k)*saturation(center[..., 3], model)
                output_jac = (1+model.k)*slope(full[..., 3], model)[..., None]*jac[..., 3, :]
                output_center_jac = (1+model.k)*slope(center[..., 3], model)[..., None]*center_jac_box[..., 3, :]
                output_hessian = (1+model.k)*(
                    slope(full[..., 3], model)[..., None, None]*hessian[..., 3, :, :]+
                    curvature(full[..., 3], model)[..., None, None]*
                    jac[..., 3, :, None]*jac[..., 3, None, :])
                real_weight = 2*np.cos(2*np.pi*sample/54)
                imaginary_weight = -2*np.sin(2*np.pi*sample/54)
                observation = observation+stack([
                    real_weight*output_center, imaginary_weight*output_center, output_center.square()])/len(forcing)
                observation_jac = observation_jac+stack([
                    real_weight*output_center_jac, imaginary_weight*output_center_jac,
                    2*output_center[..., None]*output_center_jac], -2)/len(forcing)
                observation_hessian = observation_hessian+stack([
                    real_weight*output_hessian, imaginary_weight*output_hessian,
                    2*output_jac[..., :, None]*output_jac[..., None, :]+
                    2*output[..., None, None]*output_hessian], -3)/len(forcing)
        try:
            z, phi, encode, _ = encode_boxes(chart, full)
            _, center_phi, center_encode, center_normal = encode_boxes(chart, center)
        except AssertionError:
            return {'cells': cells, 'radius': radius, 'failed': 'polar radius contains zero'}
        center_jac_box = product(product(center_encode, Box(chart.transform)), center_jac_box)
        # Positive alignment puts the phase displacement in (-pi/2, pi/2).
        # This is the extra domain check for a global degree-one phase lift;
        # positive local phase derivatives alone do not establish its degree.
        alignment = z[..., 0]*trig(theta_box)+z[..., 1]*trig(theta_box, True)
        maxima['alignment'] = min(maxima['alignment'], float(np.min(alignment.lo)))
        zj = product(Box(chart.transform), jac)
        zh = (Box(chart.transform)[..., :, :, None, None]*hessian[..., None, :, :, :]).sum(-3)
        eh = encode_hessian(chart, z, phi)
        first = (encode[..., :, :, None, None]*zh[..., None, :, :, :]).sum(-3)
        second = (zj[..., None, :, None, :, None]*eh[..., :, :, :, None, None]*
                  zj[..., None, None, :, None, :]).sum(-3).sum(-3)
        final_hessian = first+second
        if point_check:
            coordinates = np.c_[theta, np.zeros((len(theta), 3))]
            def gradient(coords):
                value, matrix = advance(model, chart.decode(coords[:, 0], coords[:, 1:]), forcing)
                return chart.derivative_encode(value)@matrix@chart.derivative_decode(coords[:, 0], coords[:, 1:])
            numerical = np.empty_like(final_hessian.lo)
            for k in range(4):
                change = np.eye(4)[k]*1e-5
                numerical[..., k] = (gradient(coordinates+change)-gradient(coordinates-change))/(2e-5)
            midpoint = (final_hessian.lo+final_hessian.hi)/2
            error = float(np.max(abs(numerical-midpoint)))
            assert error < 3e-5, ('full chart/block Hessian mismatch', error)
            check_result = {'status': 'independent finite-difference Hessian check', 'maximumError': error,
                            'maximumIntervalWidth': float(np.max(final_hessian.hi-final_hessian.lo))}
            if measurement is None:
                return check_result

        widths = np.array([half_width, radius, radius, radius])
        derivative_error = (final_hessian*Box(-widths, widths)).sum(-1).magnitude()
        final_jac = center_jac_box+Box(-derivative_error, derivative_error)
        if measurement is not None:
            unit = np.array([1., 0., 0., 0.])
            center_residual = observation-measurement['mean']-potential(measurement, Box(theta))+potential(measurement, center_phi)
            center_gradient = (observation_jac-potential(measurement, Box(theta), 1)[..., :, None]*unit+
                               potential(measurement, center_phi, 1)[..., :, None]*center_jac_box[..., None, 0, :])
            residual_hessian = (observation_hessian-
                potential(measurement, theta_box, 2)[..., :, None, None]*unit[:, None]*unit[None, :]+
                potential(measurement, phi, 2)[..., :, None, None]*
                final_jac[..., None, 0, :, None]*final_jac[..., None, 0, None, :]+
                potential(measurement, phi, 1)[..., :, None, None]*final_hessian[..., None, 0, :, :])
            residual_box = center_residual+(center_gradient*delta).sum(-1)+0.5*(
                residual_hessian*delta[..., :, None]*delta[..., None, :]).sum(-1).sum(-1)
            residual_bound = np.maximum(residual_bound, np.max(residual_box.magnitude(), axis=0))
            if point_check:
                def evaluate_residual(coords):
                    states, observed = observe_block(model, chart.decode(coords[:, 0], coords[:, 1:]), forcing)
                    phase, _ = chart.encode(states)
                    harmonics = np.arange(1, measurement['highest']+1)
                    coeff = np.asarray(measurement['potential'])
                    return observed-measurement['mean']-basis(coords[:, 0], harmonics)@coeff+basis(phase, harmonics)@coeff
                second = np.empty_like(residual_hessian.lo)
                step = 1e-4
                for j in range(4):
                    for k in range(4):
                        dj, dk = np.eye(4)[j]*step, np.eye(4)[k]*step
                        second[..., j, k] = (evaluate_residual(coordinates+dj+dk)-evaluate_residual(coordinates+dj-dk)-
                                             evaluate_residual(coordinates-dj+dk)+evaluate_residual(coordinates-dj-dk))/(4*step**2)
                residual_error = float(np.max(abs(second-(residual_hessian.lo+residual_hessian.hi)/2)))
                assert residual_error < 3e-5, ('measurement residual Hessian mismatch', residual_error)
                check_result['measurementResidualHessianError'] = residual_error
                return check_result
        defect = center_normal.magnitude()
        remainder = 0.5*(final_hessian[..., 1:, :, :]*delta[..., :, None]*delta[..., None, :]).sum(-1).sum(-1).magnitude()
        image = defect+(center_jac_box[..., 1:, :]*delta).sum(-1).magnitude()+remainder
        maxima['a'] = min(maxima['a'], float(np.min(final_jac.lo[..., 0, 0])))
        maxima['b'] = max(maxima['b'], float(np.max(final_jac[..., 0, 1:].magnitude().sum(axis=-1))))
        maxima['c'] = max(maxima['c'], float(np.max(final_jac[..., 1:, 0].magnitude())))
        maxima['d'] = max(maxima['d'], float(np.max(final_jac[..., 1:, 1:].magnitude().sum(axis=-1))))
        maxima['image'] = max(maxima['image'], float(np.max(image)))
        maxima['defect'] = max(maxima['defect'], float(np.max(defect)))
    trials = []
    for L in [0.01, 0.02, 0.04, 0.08, 0.16]:
        A = maxima['a']-maxima['b']*L
        if A <= 0:
            continue
        cone = (maxima['c']+maxima['d']*L)/A
        rate = maxima['d']+(maxima['c']+maxima['d']*L)*maxima['b']/A
        trials.append({'L':L, 'A':A, 'cone':cone, 'rate':rate,
                       'passes': cone <= L and rate < 1 and maxima['image'] < radius and maxima['alignment'] > 0})
    result = {'cells': cells, 'radius': radius, 'intervalProposals': maxima, 'trials': trials}
    if measurement is not None:
        phasor_error = float(np.hypot(*residual_bound[:2]))
        phasor_size = float(np.hypot(*measurement['mean'][:2]))
        result['measurementProposals'] = {
            'mean': measurement['mean'], 'residualBounds': residual_bound.tolist(),
            'potentialBounds': measurement['fourierPotentialBound'],
            'coherentPower': phasor_size**2/2,
            'asymptoticCoherentPowerError': phasor_error*(phasor_size+phasor_error/2),
            'asymptoticTotalPowerError': float(residual_bound[2]),
            'status': 'bounds for eventual averages if replayed; no exact convergence claimed'}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cells', type=int, nargs='+', default=[256, 1024, 4096])
    parser.add_argument('--radius', type=float, default=0.0001)
    parser.add_argument('--check-hessian', action='store_true')
    parser.add_argument('--measurements', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    proposals = json.loads((root/'build/results/ladder-drift-certificate/averages.json').read_text())
    data = proposals['chart']
    chart = Chart(np.array(data['transform']), np.array(data['coefficientsReal'])+1j*np.array(data['coefficientsImag']))
    model = Model('tanh', 1, .75)
    measurement = next(row for row in proposals['results'] if row['highest']==64) if args.measurements else None
    if args.check_hessian:
        print(json.dumps(probe(chart, model, 4, 1e-9, point_check=True, measurement=measurement)))
        return
    results = []
    for count in args.cells:
        result = probe(chart, model, count, args.radius, measurement=measurement)
        results.append(result)
        print(json.dumps(result), flush=True)
    (root/f'build/results/ladder-drift-certificate/tube-{args.radius}.json').write_text(json.dumps({
        'status':'untrusted interval proposals; transcendental values require verified replay', 'results':results},indent=2)+'\n')


if __name__ == '__main__':
    main()

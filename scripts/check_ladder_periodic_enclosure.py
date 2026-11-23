#!/usr/bin/env python3
"""External interval check for an attracting, forced periodic ladder orbit.

This is NOT a Lean-kernel certificate. Decimal arithmetic rounds outwards;
trigonometric/exponential bounds use explicit Taylor tails. The resulting
uniform derivative and residual bounds are the hypotheses of AttractingBlock.
No harmonic truncation or continuous-time phase model is used in this check.
"""

from dataclasses import dataclass
from decimal import Decimal as D, localcontext, getcontext, ROUND_FLOOR, ROUND_CEILING
from fractions import Fraction
import json
import random
from pathlib import Path

import numpy as np

getcontext().prec = 70


def rounded(fn, mode):
    with localcontext() as ctx:
        ctx.rounding = mode
        return fn()


@dataclass(frozen=True)
class Interval:
    lo: D
    hi: D

    def __post_init__(self):
        assert self.lo <= self.hi

    @staticmethod
    def of(x):
        if isinstance(x, Interval):
            return x
        if isinstance(x, Fraction):
            return Interval.of(x.numerator)/x.denominator
        value = D(int(x)) if isinstance(x, bool) else D(str(x))
        return Interval(value, value)

    def __add__(self, other):
        other = self.of(other)
        return Interval(rounded(lambda: self.lo+other.lo, ROUND_FLOOR),
                        rounded(lambda: self.hi+other.hi, ROUND_CEILING))

    __radd__ = __add__

    def __neg__(self):
        return Interval(self.hi.copy_negate(), self.lo.copy_negate())

    def __sub__(self, other):
        return self+-self.of(other)

    def __rsub__(self, other):
        return self.of(other)+-self

    def __mul__(self, other):
        other = self.of(other)
        pairs = [(a, b) for a in (self.lo, self.hi) for b in (other.lo, other.hi)]
        return Interval(min(rounded(lambda: a*b, ROUND_FLOOR) for a, b in pairs),
                        max(rounded(lambda: a*b, ROUND_CEILING) for a, b in pairs))

    __rmul__ = __mul__

    def __truediv__(self, other):
        other = self.of(other)
        assert other.lo*other.hi > 0, ('division across zero', other)
        reciprocal = Interval(rounded(lambda: D(1)/other.hi, ROUND_FLOOR),
                              rounded(lambda: D(1)/other.lo, ROUND_CEILING))
        return self*reciprocal

    def __rtruediv__(self, other):
        return self.of(other)/self

    def square(self):
        high = max(abs(self.lo), abs(self.hi))
        low = D(0) if self.lo <= 0 <= self.hi else min(abs(self.lo), abs(self.hi))
        return Interval(rounded(lambda: low*low, ROUND_FLOOR), rounded(lambda: high*high, ROUND_CEILING))

    def sqrt(self):
        assert self.lo >= 0
        # Decimal.sqrt is correctly rounded. One adjacent representable value
        # on each side encloses the exact root, independently of tie direction.
        return Interval(max(D(0), self.lo.sqrt().next_minus()), self.hi.sqrt().next_plus())

    def mid(self):
        return (self.lo+self.hi)/2

    def magnitude(self):
        return max(abs(self.lo), abs(self.hi))

    def record(self):
        return [str(self.lo), str(self.hi)]


I = Interval.of


def atan_reciprocal(q):
    value = I(0)
    for n in range(100):
        value += I((-1)**n)/((2*n+1)*q**(2*n+1))
    tail = (I(1)/(201*q**201)).hi
    return value+Interval(-tail, tail)


def sine_cosine(x, sine):
    term, value = (x, x) if sine else (I(1), I(1))
    for n in range(1, 100):
        term = -term*x.square()/((2*n)*(2*n+1) if sine else (2*n-1)*(2*n))
        value += term
    # Taylor's theorem: every derivative of sin/cos is bounded by one.
    degree = 199 if sine else 198
    tail = I(1)
    for n in range(1, degree+2):
        tail *= I(x.magnitude())/n
    return value+Interval(-tail.hi, tail.hi)


def exp_endpoint(x):
    if x < 0:
        return 1/exp_endpoint(-x)
    term, value = I(1), I(1)
    for n in range(1, 161):
        term *= I(x)/n
        value += term
    # Every subsequent positive term ratio is at most x/162.
    assert x < 162
    tail = term*I(x)/161/(1-I(x)/162)
    return value+Interval(D(0), tail.hi)


def tanh(x):
    def endpoint(t):
        scaled = 2*I(t)
        e = Interval(exp_endpoint(scaled.lo).lo, exp_endpoint(scaled.hi).hi)
        return (e-1)/(e+1)
    return Interval(endpoint(x.lo).lo, endpoint(x.hi).hi)


def mm(a, b):
    return [[sum((a[i][k]*b[k][j] for k in range(len(b))), I(0))
             for j in range(len(b[0]))] for i in range(len(a))]


def rational_inverse(a):
    # Invert the rational, decimal-valued change of coordinates exactly.
    n = len(a)
    b = [[Fraction(a[i][j]) for j in range(n)]+[Fraction(i == j) for j in range(n)] for i in range(n)]
    for k in range(n):
        pivot = max(range(k, n), key=lambda i: abs(b[i][k]))
        b[k], b[pivot] = b[pivot], b[k]
        divisor = b[k][k]
        assert divisor
        b[k] = [v/divisor for v in b[k]]
        for i in range(n):
            if i != k:
                factor = b[i][k]
                b[i] = [x-factor*y for x, y in zip(b[i], b[k])]
    return [row[n:] for row in b]


def exact_inverse(a):
    return [[I(v) for v in row] for row in rational_inverse(a)]


def norm_bound(values):
    return sum((v.square() for v in values), I(0)).sqrt().hi


def check_fixture(fixture):
    assert fixture['drive'] == 1 and fixture['resonance'] == 0.75 and fixture['model'] == 'tanh'
    p = fixture['periodic']
    pi = 16*atan_reciprocal(5)-4*atan_reciprocal(239)
    sine = sine_cosine(pi/192, True)
    alpha = 2*sine*((1+sine.square()).sqrt()-sine)
    beta = 1-alpha
    gain = I(5)
    feedback = I(39)/10*(7*I(7).sqrt()-1)/48
    normalization = tanh(gain)
    count = p['samplePeriod']
    forcing = [sine_cosine(2*pi*p['inputCycles']*n/count, False)/4 for n in range(count)]
    def period(state, outputs=None):
        derivative = [[I(i == j) for j in range(4)] for i in range(4)]
        for u in forcing:
            next_state = [beta*state[0]+alpha*(u-feedback*state[3])]
            jac = [[I(0) for _ in range(4)] for _ in range(4)]
            jac[0][0], jac[0][3] = beta, -alpha*feedback
            for j in range(1, 4):
                saturation = tanh(gain*next_state[j-1])
                slope = gain*(1-saturation.square())/normalization
                next_state.append(beta*state[j]+alpha*saturation/normalization)
                jac[j] = [alpha*slope*v for v in jac[j-1]]
                jac[j][j] += beta
            state = next_state
            derivative = mm(jac, derivative)
            if outputs is not None:
                outputs.append((1+feedback)*tanh(gain*state[3])/normalization)
        return state, derivative
    center = [D(str(x)) for x in p['state']]
    for _ in range(4):
        value, jac = period([I(x) for x in center])
        residual = np.array([float(v.mid()-x) for v, x in zip(value, center)])
        matrix = np.array([[float(v.mid()) for v in row] for row in jac])-np.eye(4)
        correction = np.linalg.solve(matrix, residual)
        center = [x-D(str(dx)) for x, dx in zip(center, correction)]
    transform_decimal = [[D(str(x)) for x in row] for row in p['adaptedNorm']]
    transform = [[I(x) for x in row] for row in transform_decimal]
    inverse = exact_inverse(transform_decimal)
    radius = D('0.001')
    widths = [sum((I(v.magnitude()) for v in row), I(0))*radius for row in inverse]
    box = [I(x)+Interval(-w.hi, w.hi) for x, w in zip(center, widths)]
    _, jacobian_box = period(box)
    transformed = mm(mm(transform, jacobian_box), inverse)
    derivative_bound = norm_bound([v for row in transformed for v in row])
    value, _ = period([I(x) for x in center])
    residual = mm(transform, [[v-I(x)] for v, x in zip(value, center)])
    residual_bound = norm_bound([v[0] for v in residual])
    image_radius = (I(residual_bound)+I(derivative_bound)*radius).hi
    assert derivative_bound < 1 and image_radius < radius, (derivative_bound, image_radius)
    distance_bound = (I(residual_bound)/(1-I(derivative_bound))).hi
    assert derivative_bound < D('0.9') and residual_bound < D('1e-58')
    # Enclose the actual periodic orbit using its a-posteriori distance from
    # the candidate. Replay the recurrence, including the output saturator.
    orbit_widths = [sum((I(v.magnitude()) for v in row), I(0))*distance_bound for row in inverse]
    orbit_box = [I(x)+Interval(-w.hi, w.hi) for x, w in zip(center, orbit_widths)]
    outputs = []
    period(orbit_box, outputs)
    carrier_cos = [4*u for u in forcing]
    carrier_sin = [sine_cosine(2*pi*p['inputCycles']*n/count, True) for n in range(count)]
    coherent_real = 2*sum((y*c for y, c in zip(outputs, carrier_cos)), I(0))/count
    coherent_imag = -2*sum((y*s for y, s in zip(outputs, carrier_sin)), I(0))/count
    coherent_power = (coherent_real.square()+coherent_imag.square())/2
    total_power = sum((y.square() for y in outputs), I(0))/count
    return {'status': 'external interval check; not Lean-kernel checked',
            'exactParameters': {'gain': '5', 'alpha': '2*sin(pi/192)*(sqrt(1+sin(pi/192)^2)-sin(pi/192))',
                                'feedback': '39/10*(7*sqrt(7)-1)/48', 'tanhGain': 'tanh(5)',
                                'input': f'cos(2*pi*{p["inputCycles"]}*n/{count})/4', 'detector': 'disabled'},
            'periodSamples': count, 'coordinateCenter': list(map(str, center)),
            'roundedCertificateBounds': {'contraction': '0.9', 'residual': '1e-58'},
            'coordinateTransform': [[str(x) for x in row] for row in transform_decimal],
            'ballRadius': str(radius), 'derivativeNormUpperBound': str(derivative_bound),
            'residualNormUpperBound': str(residual_bound), 'imageRadiusUpperBound': str(image_radius),
            'fixedPointDistanceUpperBound': str(distance_bound),
            'responseEnclosures': {'coherentReal': coherent_real.record(),
                                   'coherentImag': coherent_imag.record(),
                                   'coherentPower': coherent_power.record(),
                                   'totalPower': total_power.record(),
                                   'normalizedCoherentPower': (32*coherent_power).record(),
                                   'normalizedTotalPower': (32*total_power).record()},
            'alphaEnclosure': alpha.record(), 'feedbackEnclosure': feedback.record(),
            'piEnclosure': pi.record(), 'arithmetic': '70-digit outward-rounded Decimal intervals, explicit series tails'}


def check_arithmetic():
    # Independent exact-rational checks exercise rounding and signs. These
    # tests help review the checker; they are not a proof of its correctness.
    rng = random.Random(724)
    for _ in range(200):
        a = Fraction(rng.randint(-1000, 1000), rng.randint(1, 1000))
        b = Fraction(rng.randint(1, 1000), rng.randint(1, 1000))
        for enclosure, exact in [(I(a)+I(b), a+b), (I(a)-I(b), a-b),
                                 (I(a)*I(b), a*b), (I(a)/I(b), a/b)]:
            assert Fraction(enclosure.lo) <= exact <= Fraction(enclosure.hi)
        square_root = I(b).sqrt()
        assert Fraction(square_root.lo)**2 <= b <= Fraction(square_root.hi)**2
    for value in ['-20', '-5', '-0.001', '0', '0.001', '5', '20']:
        x = D(value)
        exponential, saturation = exp_endpoint(x), tanh(I(x))
        with localcontext() as context:
            context.prec = 110
            exp_reference = x.exp()
            tanh_reference = ((2*x).exp()-1)/((2*x).exp()+1)
        assert exponential.lo <= exp_reference <= exponential.hi
        assert saturation.lo <= tanh_reference <= saturation.hi
    return {'exactRationalOperations': 800, 'squareRootBounds': 200,
            'independentExponentialTanhChecks': 14}


def main():
    root_dir = Path(__file__).resolve().parents[1]
    data = json.loads((root_dir/'build/results/ladder-attraction/diagnostics.json').read_text())
    fixture = next(r for r in data if r['model'] == 'tanh' and r['drive'] == 1)
    arithmetic_checks = check_arithmetic()
    result = check_fixture(fixture)
    result['arithmeticChecks'] = arithmetic_checks
    from analyze_ladder_driven_response import Model, evaluate_second
    prepared_rows = json.loads((root_dir/'build/results/ladder-driven-response/response.json').read_text())
    prepared = next(r['prepared'] for r in prepared_rows if r['model'] == 'tanh' and r['drive'] == 1)
    model = Model('tanh', 1, 0.75)
    frequency = model.fs*fixture['periodic']['inputCycles']/result['periodSamples']
    result['approximationComparison'] = {}
    for name, moving in [('frozenQuadratic', False), ('movingQuadratic', True)]:
        approximation = evaluate_second(model, prepared, 3, 0.25, frequency, moving=moving)
        errors = {}
        for key in ['coherentPower', 'totalPower']:
            enclosure = Interval(*map(D, result['responseEnclosures'][key]))
            errors[key] = str((enclosure-I(str(approximation[key]))).magnitude())
        result['approximationComparison'][name] = {
            'frequencyHz': frequency, 'computedApproximation': approximation,
            'absolutePowerErrorUpperBounds': errors,
            'scope': 'error of these rounded output numbers at this one fixture; external checker only'}
    out = root_dir/'build/results/ladder-attraction/periodic-enclosure.json'
    out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

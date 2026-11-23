"""Certify a reset-reachable wet detector cap for the fixed stock challenge."""

import argparse
import ctypes as ct
from fractions import Fraction as Q
import json
from pathlib import Path

import numpy as np

from finite_stock import check as check_stock, rounded_error, tiny, u


RHO = Q('0.996')
SAMPLES = 4096
POWER_CAP = Q('2.078')


def impulse_bounds(a1, a2, a3):
    # Exact-real SVF from the stored binary32 words:
    # x' = A*x + B*input, bandpass = C*x + d*input.
    A = ((2*a1-1, -2*a2), (2*a2, 1-2*a3))
    B = (2*a2, 2*a3)
    C = (a1, -a2)
    d = a2

    def p_inner(x, y):
        return x[0]*y[0] + x[1]*y[1] + (x[0]*y[1] + x[1]*y[0])/2

    col1, col2 = zip(*A)
    h11 = RHO**2 - p_inner(col1, col1)
    h12 = RHO**2/2 - p_inner(col1, col2)
    h22 = RHO**2 - p_inner(col2, col2)
    assert h11 > 0 and h11*h22 > h12**2
    # ||v||_P <= ||v||_1 and |v_j| <= (2/sqrt(3)) ||v||_P.
    c_norm = Q(5, 4)*sum(map(abs, C))
    b_norm = sum(map(abs, B))

    # All coefficients have power-of-two denominators. Carry the exact dyadic
    # impulse response with a common denominator per sample. In the cascade,
    # y and its input t1 have one additional factor of scale in the denominator.
    scale = max(q.denominator for q in (*A[0], *A[1], *B, *C, d))

    def scaled(q):
        n = q*scale
        assert n.denominator == 1
        return n.numerator

    ((a, b), (c, e)) = tuple(tuple(map(scaled, row)) for row in A)
    b1, b2 = map(scaled, B)
    c1, c2 = map(scaled, C)
    tap = scaled(d)

    x1 = x2 = y1 = y2 = sum1 = sum2 = 0
    for j in range(SAMPLES):
        impulse = int(j == 0)
        t1 = c1*x1 + c2*x2 + tap*impulse
        t2 = c1*y1 + c2*y2 + tap*t1
        sum1 = scale*sum1 + abs(t1)
        sum2 = scale*sum2 + abs(t2)
        x1, x2 = a*x1 + b*x2 + b1*impulse, c*x1 + e*x2 + b2*impulse
        y1, y2 = a*y1 + b*y2 + b1*t1, c*y1 + e*y2 + b2*t1

    denom = scale**SAMPLES
    xt = Q(abs(x1)+abs(x2), denom)
    yt = Q(abs(y1)+abs(y2), denom*scale)
    tail1 = c_norm*xt/(1-RHO)
    single = Q(sum1, denom) + tail1
    # Future stage-two output is the homogeneous response to y plus the
    # convolution of the remaining stage-one impulse with the full SVF.
    cascade = Q(sum2, denom*scale) + c_norm*yt/(1-RHO) + single*tail1
    assert single < Q('1.306') and cascade < Q('1.341')
    return single, cascade, c_norm, b_norm


def svf_error(a1, a2, a3, c_norm, b_norm, input_cap, radius):
    # The generated SVF has three FMA nodes for the two tap calculations,
    # and one FMA for each integrator update. Bound local errors with the
    # actual binary32 state and input as exact operands.
    state = Q(5, 4)*radius
    diff = input_cap + state
    diff_error = rounded_error(diff)
    bandpass = a1*state + a2*diff
    bp_error = rounded_error(bandpass, rounded_error(a1*state) + a2*diff_error)
    partial = (1+a2)*state
    lowpass = partial + a3*diff
    lp_error = rounded_error(lowpass, rounded_error(partial) + a3*diff_error)
    dx = rounded_error(2*bandpass+state, 2*bp_error)
    dy = rounded_error(2*lowpass+state, 2*lp_error)
    injection = dx+dy
    assert RHO*radius + b_norm*input_cap + injection < radius
    # Unroll the state error recursion from the zero reset, driven by these
    # local injections, while keeping the same actual input sequence.
    return bp_error + c_norm*injection/(1-RHO)


def feedback_lower(dry, base, reference, damping, floor):
    # Every operation is monotone in its indicated positive operand.
    # Given actual dry power >= dry and wet power <= POWER_CAP, replace
    # each rounded operand by an outward endpoint before the next operation.
    ref_lower = reference*dry*(1-u)-tiny
    excess_upper = max(0, (POWER_CAP-ref_lower)*(1+u)+tiny)
    denom_lower = (dry+floor)*(1-u)-tiny
    assert denom_lower > 0
    excess_normalized = excess_upper/denom_lower
    excess_normalized += rounded_error(excess_normalized)
    scaled_excess = damping*excess_normalized
    divisor = 1 + scaled_excess
    divisor += rounded_error(divisor, rounded_error(scaled_excess))
    exact_gain_lower = base/divisor
    return exact_gain_lower*(1-u)-tiny


def outward(q, places=10):
    scale = 10**places
    scaled = q*scale
    whole, remainder = divmod(-(-scaled.numerator//scaled.denominator), scale)
    return f'{whole}.{remainder:0{places}d}'


def report(knobs, words):
    check_stock()
    alpha, base, gain, tanh_gain, a1, a2, a3, beta, reference, damping, floor = map(
        Q, map(float, words))
    assert 0 < alpha < 1 and 0 < beta < 1 and 0 < tanh_gain <= 1
    assert 0 < a1 < 1 and 0 < a2 < 1 and 0 < a3 < 1
    assert base > 0 and gain > 0 and reference > 0 and damping > 0 and floor > 0

    # The clamped saturator output before normalization has magnitude <=1.
    # Account for the rounded division, complement, product and final FMA.
    stage4 = Q('1.073')
    normalized = (1+u)/tanh_gain+tiny
    assert normalized < stage4
    old_product = rounded_error((1-alpha)*stage4, stage4*rounded_error(1-alpha))
    assert stage4-alpha*(stage4-normalized)+rounded_error(stage4, old_product) < stage4

    single, cascade, c_norm, b_norm = impulse_bounds(a1, a2, a3)
    err1 = svf_error(a1, a2, a3, c_norm, b_norm, stage4, Q(3))
    src2 = stage4*single+err1
    err2 = svf_error(a1, a2, a3, c_norm, b_norm, src2, Q(4))
    tap = stage4*cascade + single*err1 + err2
    squared = tap**2 + rounded_error(tap**2)

    # For 0 <= e <= cap and 0 <= v <= squared, the exact EMA update is a
    # convex combination. Bound the three roundings in e + beta*(v-e).
    # For the lower bound, monotonicity in v reduces to v=0, at which the
    # subtraction is exact and RN(beta*e) <= e because e is representable.
    assert squared < POWER_CAP
    diff_error = rounded_error(POWER_CAP)
    product_error = rounded_error(beta*POWER_CAP, beta*diff_error)
    assert POWER_CAP-beta*(POWER_CAP-squared)+rounded_error(POWER_CAP, product_error) < POWER_CAP

    critical = (2+alpha/(1-alpha))**2/(gain/tanh_gain)**3
    thresholds = (('0.000321', Q('0.925669')), ('0.000357', Q(1)),
                  ('0.001168', Q(2)), ('0.004820', Q(3)), ('0.04545', Q('3.5')))
    assert critical < thresholds[0][1]
    for dry, target in thresholds:
        assert feedback_lower(Q(dry), base, reference, damping, floor) > target

    # When the rounded reference product already exceeds every possible wet
    # power, the rectifier returns zero and the final division by 1 is exact.
    dry_full_feedback = Q('0.458')
    assert reference*dry_full_feedback*(1-u)-tiny > POWER_CAP
    return dict(knobs=list(map(float, knobs)), words=[float(word).hex() for word in words],
                impulse_samples=SAMPLES, p_norm_contraction=str(RHO),
                single_bandpass_l1_upper=outward(single), cascade_l1_upper=outward(cascade),
                stage4_upper=str(stage4), svf1_output_roundoff_upper=outward(err1),
                svf2_output_roundoff_upper=outward(err2), wet_bandpass_upper=outward(tap),
                wet_square_upper=outward(squared), wet_ema_upper=str(POWER_CAP),
                dry_power_for_exact_base_feedback=outward(dry_full_feedback, 3),
                exact_base_feedback_word=float(base).hex(),
                tangent_critical_feedback_upper=outward(critical),
                dry_thresholds=[dict(power=dry, feedback_at_least=str(target))
                                for dry, target in thresholds])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('confirmation', type=Path)
    parser.add_argument('--library', type=Path,
                        default=Path('build/tools/ladder-noise-grid/cycle.so'))
    args = parser.parse_args()
    with np.load(args.confirmation) as data:
        knobs = data['knobs']
    library = ct.CDLL(str(args.library.resolve()))
    coefficients = library.Coefficients
    coefficients.argtypes = [ct.c_double]*3 + [ct.POINTER(ct.c_float)]
    coefficients.restype = None
    words = (ct.c_float*11)()
    coefficients(*knobs, words)
    print(json.dumps(report(knobs, words), indent=2))

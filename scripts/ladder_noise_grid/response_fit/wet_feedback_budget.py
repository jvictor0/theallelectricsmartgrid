"""Check exact-real and binary32 detector budgets at the fixed stock setting.

The binary32 bounds assume round-to-nearest-even, including subnormals,
and the source update order. They apply to detector bandpass tap energies.
They use reset-reachable states for dry input magnitude at most 241.
"""
import argparse
import json
from fractions import Fraction
from pathlib import Path

import numpy as np

from detector_memory import check_tube
from finite_stock import check as check_stock, rounded_error, u
from record_bank import bind
from survey import EPOCH


ETA = Fraction(1, 2**150)


def detector_caps(coefficients, input_cap, radii):
    """Check two successive SVF P-norm radii and return the second tap cap."""
    a, b, c = coefficients
    cap = input_cap
    for radius in radii:
        X = Y = Fraction(7, 6)*radius
        assert Fraction(7, 6)**2 > Fraction(4, 3)
        diff = cap+Y
        bp = a*X+b*diff
        lp = Y+b*X+c*diff
        diff_error = rounded_error(diff)
        bp_error = rounded_error(bp, rounded_error(a*X)+b*diff_error)
        lp_error = rounded_error(lp, rounded_error(Y+b*X)+c*diff_error)
        x_error = rounded_error(2*bp+X, 2*bp_error)
        y_error = rounded_error(2*lp+Y, 2*lp_error)
        # The affine input column has P-norm at most the sum of its entries.
        assert Fraction('.996')*radius+2*(b+c)*cap+x_error+y_error < radius
        cap = bp+bp_error
    return cap


def ema_error_parts(a, p, v, eta):
    """Bound errors starting from exact binary32 p and exact square v."""
    multiply_error = u*v+eta
    subtract_error = (1+u)*multiply_error+u*(p+v)+eta
    scale_error = (1+u)*a*subtract_error+u*a*(p+v)+eta
    return (1+u)*scale_error+u*((1-a)*p+a*v)+eta


def check_rounding(words):
    """Check exact-rational error majorants for stock binary32 operations."""
    check_stock()
    check_tube(words)
    alpha, K, _, tanh_gain, a, b, c, rate, R, D, F = map(Fraction, map(float, words))
    eta = ETA
    assert 0 < a < 1 and 0 < b < 1 and 0 < c < 1
    radius = Fraction('1.073')
    saturated = (1+u)/tanh_gain+eta
    stage_error = rounded_error(radius, rounded_error(radius, radius*rounded_error(1)))
    assert radius-alpha*(radius-saturated)+stage_error < radius
    # |stage4| is initially zero and each input is the bounded stage-3 saturator.
    dry_tap = detector_caps((a,b,c), 241, (500,1200))
    wet_tap = detector_caps((a,b,c), radius, (3,8))
    dry_power, wet_power = 3_000_000, 100
    for power, tap in ((dry_power, dry_tap), (wet_power, wet_tap)):
        error = ema_error_parts(rate, power, tap*tap, eta)
        assert (1-rate)*power+rate*tap*tap+error < power
        # Rounded squared-input, relaxed update remains nonnegative. If
        # the rounded difference is negative, its magnitude is at most p
        # by monotonicity of rounding, and its scaled magnitude is at most p.
        assert 0 < rate < 1
    for p, v, pad in ((1,0,0), (0,1,0), (0,0,1)):
        assert ema_error_parts(rate,p,v,pad) <= u*p+5*u*rate*(p+v)+4*pad

    # The rounded K/(1+d) equals K exactly when its rounded denominator is 1.
    # Consecutive binary32 values in the binade containing K differ by 2^-22.
    assert 2 < K < 4 and K/(1+2*u) < K-2*u
    # Round-to-nearest-even sends 1+u to 1. With FMA, omit product rounding.
    # With no FMA, its effect is the third relative factor in the bounds below.
    H = u/D
    distortion = 6*u*H
    assert (1+u)/(1-u)**3-1 < 6*u
    assert 1-(1-u)/(1+u)**3 < 6*u
    assert (1-u)*F-eta > F/2
    # The exact target is H*(p+F). The three intermediate roundings
    # following reference multiplication have ideal nonnegative argument
    # x=max(0,q-rounded(R*p)). Division uses rounded(p+F)>=F/2.
    intermediate_pad = eta*((2*D/F)*(1+u)**2+D*(1+u)+1)
    assert intermediate_pad < u
    # Enclose the guard's absolute remainder in solving d <= u or d > u
    # for max(0,q-rounded(R*p)). Afterward add the reference-product pad.
    power_plus_floor = dry_power+F
    guard_pad = Fraction(1,10**26)
    assert (u*eta + intermediate_pad*((1+u)*power_plus_floor+eta))/(D*(1-u)**3)+eta < guard_pad
    assert (u*eta + intermediate_pad*((1+u)*power_plus_floor+eta))/(D*(1+u)**3)+eta < guard_pad

    # For the selected samples the full-feedback guard bounds q' itself.
    # The EMA lower inequality relates the prior q to q' plus its tiny pad.
    T = R+H
    guard_p = u*R+distortion
    prior_leak = 1-rate-u-5*u*rate
    assert prior_leak > 0
    q_on_gate = ((T+guard_p), (H+distortion)*F+guard_pad+4*eta)
    return dict(rate=rate, T=T, H=H, guard_p=guard_p, distortion=distortion,
                guard_pad=guard_pad, q_on_gate=q_on_gate, prior_leak=prior_leak,
                dry_tap=dry_tap, wet_tap=wet_tap, dry_power=dry_power, wet_power=wet_power)

def report(grid, library):
    reference = grid/'screen-v3/survivor-replay-250/fresh-parents.npz'
    with np.load(reference) as data:
        settings = data['settings'][0]
    words = np.empty(11, 'f4')
    bind(library).Coefficients(*settings, words)
    alpha, K, gain, tanh_gain, _, _, _, rate, R, D, F = map(Fraction, map(float, words))
    assert 0 < alpha < Fraction(2, 3) and gain > 0 and tanh_gain > 0
    assert K > 0 and 0 < rate <= 1 and R >= 0 and D > 0 and F > 0

    def upper(value, places):
        scale = 10**places
        scaled = value*scale
        numerator = -(-scaled.numerator//scaled.denominator)
        assert Fraction(numerator, scale) >= value
        whole, remainder = divmod(numerator, scale)
        return f'{whole}.{remainder:0{places}d}'

    rows = []
    for relative in ('1', '.9999', '.999', '.99', '.95', '.9', '.75', '.5'):
        fraction = Fraction(relative)
        cutoff = K*fraction
        T = R+(K/cutoff-1)/D
        floor_cost = (T-R)*F
        # From reset, deficit(e_n) <= 0 and deficit(e_0) = -floor_cost.
        rows.append(dict(fraction_of_base=relative, dry_energy_multiplier_upper=upper(T, 6),
                         selected_sample_floor_upper=upper(floor_cost, 15),
                         reset_endpoint_per_sample_four_records_upper=upper(
                             floor_cost/(rate*4*14*EPOCH), 18)))
    # Only three tangent saturators lie inside the delayed feedback loop.
    critical = (2+alpha/(1-alpha))**2/(gain/tanh_gain)**3
    assert 0 < critical < K
    unstable_threshold = R+(K/critical-1)/D
    rounded = check_rounding(words)
    guard_p, distortion = rounded['guard_p'], rounded['distortion']
    guard_pad = rounded['guard_pad']
    prior_q_p, prior_q_const = rounded['q_on_gate']
    p_coef = (u/rate+5*u)*rounded['T']
    q_coef = u/rate+5*u
    # Absorb the selected wet square from the q update error on the left.
    absorption = 1-5*u
    post_coef = guard_p/rate+q_coef*prior_q_p/rounded['prior_leak']
    # Summing the dry EMA gives mean(p) <= this factor times mean(dry^2),
    # plus bounded endpoints divided by elapsed samples and a subnormal pad.
    dry_leak = rate-u-5*u*rate
    assert dry_leak > 0
    dry_mean_factor = rate*(1+5*u)/dry_leak
    steady_roundoff = ((p_coef+post_coef)*dry_mean_factor+5*u*rounded['T'])/absorption
    return dict(settings=settings.tolist(),
                coefficient_words=dict(base=str(K), smoothing=str(rate),
                                       reference=str(R), damping=str(D), floor=str(F)),
                rows=rows,
                binary32_full_feedback=dict(
                    threshold_dry_multiplier_upper=upper(rounded['T'],12),
                    old_dry_power_error_coefficient_upper=upper(p_coef/absorption,12),
                    post_dry_power_error_coefficient_upper=upper(
                        post_coef/absorption,12),
                    dry_square_roundoff_coefficient_upper=upper(5*u*rounded['T']/absorption,12),
                    selected_dry_square_main_coefficient_upper=upper(rounded['T']/absorption,12),
                    long_run_roundoff_times_full_dry_square_upper=upper(steady_roundoff,12),
                    physical_floor_charge_upper=upper(rounded['H']*F/absorption,20),
                    absolute_roundoff_per_sample_upper=upper(
                        ((distortion*F+guard_pad+4*ETA*(rounded['T']+1))/rate+
                         q_coef*prior_q_const/rounded['prior_leak'])/absorption,24),
                    dry_tap_cap_upper=upper(rounded['dry_tap'],6),
                    wet_tap_cap_upper=upper(rounded['wet_tap'],6)),
                frozen_zero_amplitude_tangent=dict(
                    critical_feedback_upper=upper(critical, 9),
                    fraction_of_base_upper=upper(critical/K, 9),
                    dry_energy_multiplier_upper=upper(unstable_threshold, 6)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('grid', type=Path)
    parser.add_argument('--library', type=Path,
                        default=Path('build/tools/ladder-noise-grid/cycle.so'))
    args = parser.parse_args()
    print(json.dumps(report(args.grid, args.library), indent=2))

"""Check the per-sample invariant using the coefficient inequalities in WetPowerBox --whole-survey."""
from fractions import Fraction as Q


u = Q(1, 2**24)
tiny = Q(1, 2**80)


def rounded_error(ideal_bound, inherited=0):
    # Round-to-nearest of an expression already within inherited of its ideal value.
    assert ideal_bound >= 0 and inherited >= 0 and ideal_bound + inherited < 2**126
    return inherited + (ideal_bound + inherited)*u + tiny


def detector(radius, input_bound, tap_bound):
    # Each coordinate has magnitude <= 2/sqrt(3) times the P-norm.
    coord = Q(5, 4)*radius
    diff = input_bound + coord
    diff_error = rounded_error(diff)
    first_error = rounded_error(coord)
    bp = coord + diff
    bp_error = rounded_error(bp, first_error + diff_error)
    partial_error = rounded_error(2*coord)
    lp = 2*coord + diff
    lp_error = rounded_error(lp, partial_error + diff_error)
    x_error = rounded_error(2*bp + coord, 2*bp_error)
    y_error = rounded_error(2*lp + coord, 2*lp_error)
    # The input column has P-norm <= sqrt(12); coordinate errors add in P-norm.
    assert Q(9999, 10000)*radius + 4*input_bound + x_error + y_error < radius
    assert bp + bp_error < tap_bound


def audio_stage(radius, input_bound):
    # The stock expression rounds (1-alpha), multiplies the old state, then does one FMA.
    complement_error = rounded_error(1)
    product_error = rounded_error(radius, radius*complement_error)
    final_error = rounded_error(radius + input_bound, product_error)
    assert radius - Q(6, 10000)*(radius-input_bound) + final_error < radius


def check():
    assert 1835008 < 32*240**2 and 240 + rounded_error(240) < 241
    # Positive normalization, clamped rational numerator/denominator division.
    normalized = Q(1)/Q('0.24')
    assert normalized + rounded_error(normalized) < Q('4.17')
    feedback = Q('3.901')
    feedback_sample = feedback*5
    dry_sum = 241 + feedback_sample
    assert dry_sum + rounded_error(dry_sum, rounded_error(feedback_sample)) < 262
    audio_stage(500, 262)
    audio_stage(5, Q('4.17'))
    # The largest rational-tanh argument is 5*500. Even its cubic numerator is safe.
    arg = 2501
    assert arg*(arg*arg+28) < 2**35
    assert Q('4.17')*(1+feedback)*(1+u)**2 < 21

    detector(2*10**7, 241, 6*10**7)
    detector(3*10**12, 6*10**7, 10**13)
    power = 2*10**26
    squared = 10**26 + rounded_error(10**26)
    diff = power + squared
    product_error = rounded_error(diff/ Q(10), rounded_error(diff)/10)
    total_error = rounded_error(power + diff/10, product_error)
    assert power - Q(2, 100000)*(power-squared) + total_error < power
    # critical >= 4, base <= 3.901, positive 1e-8 floor and damping < 1.
    assert (1+u)**4 / ((4-feedback)*(1-u))**2 < 103
    assert Q('5.01')**3 * Q('0.000101') * (1+u)**3 < 1
    assert 103*power*(1+u) < 2**100
    assert (power+tiny)*(1+u)**3 / Q('0.000000009') < 2**116


if __name__ == '__main__':
    check()
    print('Verified audio, both bandpass cascades, power smoothing and feedback arithmetic.')

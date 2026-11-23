"""Check positive-probability rounded slices of a projected Gaussian record.

For the continuous Gaussian input law, a strict certificate gives an open
neighborhood of the specified host vector. Projection, interpolation and
normalization are continuous there because the projected energy is positive.
The Gaussian density assigns positive probability to that neighborhood.
"""

import argparse
from fractions import Fraction as Q
import json
from math import factorial, isqrt
from pathlib import Path

import numpy as np


HOST = 14*32768
S = 1 << 128
U = Q(1, 1 << 53)
TRIG_ERROR = 256
PI = 1069028584064966747859680373161870783300


def _atan(q):
    return sum(Q((-1)**j, (2*j+1)*q**(2*j+1)) for j in range(64))


# Machin's formula checked with alternating-series remainders.
_machin = 16*_atan(5)-4*_atan(239)
assert Q(PI, S) <= _machin-Q(4, 129*239**129)
assert _machin+Q(16, 129*5**129) <= Q(PI+1, S)
assert Q(11, 7)**50*S < factorial(50)


def _quadrant(a, b):
    """Sine and cosine of pi*a/b in [0, pi/2], within TRIG_ERROR fixed units."""
    assert 0 <= 2*a <= b
    x = PI*a//b
    x2 = x*x//S
    ts = sine = x
    tc = cosine = S
    for j in range(1, 25):
        ts = ts*x2//(S*(2*j)*(2*j+1))
        tc = tc*x2//(S*(2*j-1)*(2*j))
        sign = 1-2*(j % 2)
        sine += sign*ts
        cosine += sign*tc
    # Angle error <2/S, square error <1/S relative to (x/S)^2.
    # The term-recurrence error is <=8/S per term, since terms are <4
    # and after j=1 the recurrence multiplier is <=2.5/12.
    return sine, cosine


ROOT_HALF = isqrt(S*S//2)
SIN8 = (0, ROOT_HALF, S, ROOT_HALF, 0, -ROOT_HALF-1, -S, -ROOT_HALF-1)
COS8 = (S, ROOT_HALF, 0, -ROOT_HALF-1, -S, -ROOT_HALF-1, 0, ROOT_HALF)


def _kernel(d, host):
    """Return lower and upper fixed-point kernel endpoints."""
    native = 4*host
    d %= native
    d = min(d, native-d)
    if d % 4 == 0:
        n = (host if d == 0 else 0) - (1+(-1)**(d//4))
        return n*S//host, -(-n*S//host)
    sine, cosine = _quadrant(d, native)
    # sin((M-1)t)/sin(t) = sin(Mt)cot(t) - cos(Mt), with Mt=pi*d/4.
    a = SIN8[d % 8]
    b = COS8[d % 8]
    products = [x*y for x in (a, a+1) for y in (cosine-TRIG_ERROR, cosine+TRIG_ERROR)]
    p, q = min(products), max(products)
    assert sine > TRIG_ERROR
    lo = p//(sine+(TRIG_ERROR if p >= 0 else -TRIG_ERROR))
    hi = -(-q//(sine+(-TRIG_ERROR if q >= 0 else TRIG_ERROR)))
    return (lo-b-1-S)//host, -(-(hi-b-S)//host)


def _sqrt(q):
    a = isqrt(q.numerator*S*S//q.denominator)
    return Q(a, S), Q(a+1, S)


def _product(x, y):
    return (x.real*y.real-x.imag*y.imag) + 1j*(x.real*y.imag+x.imag*y.real)


def _fft_setup(length):
    indices = np.arange(length, dtype=np.intp)
    reverse = np.zeros(length, dtype=np.intp)
    for bit in range(length.bit_length()-1):
        reverse = 2*reverse + ((indices >> bit) & 1)
    twiddles = np.empty(length//2, dtype=np.complex128)
    for j in range(length//2):
        # Evaluate the upper half-circle by reflection into the first quadrant.
        reflected = 4*j > length
        sine, cosine = _quadrant(length-2*j if reflected else 2*j, length)
        twiddles[j] = complex((-cosine if reflected else cosine)/S, -sine/S)
    return reverse, twiddles


def _fft(data, reverse, twiddles, inverse=False):
    length = len(data)
    out = data[reverse].astype(np.complex128)
    size = 2
    while size <= length:
        stage = out.reshape(-1, size)
        half = size//2
        first = stage[:, :half].copy()
        twiddle = twiddles[::length//size]
        second = _product(stage[:, half:], twiddle.conj() if inverse else twiddle)
        stage[:, :half] = first + second
        stage[:, half:] = first - second
        size *= 2
    return out/length if inverse else out


def _convolution_error(length, x_norm, x_error, kernel_error):
    # The stored twiddle differs in complex norm by <4u. Its complex
    # multiply uses four products and two sums. Including the last sum
    # or difference, the butterfly relative induced 2-norm error is <32u.
    # The same factor bounds each path's contribution to the inverse
    # transform in absolute-sum norm. Use Cauchy-Schwarz on the product
    # of the two forward spectra to bound that sum by L*||x||_2*||k||_2.
    # Parseval bounds ||k||_2 by sqrt(8) <3 on any slice of length <2N.
    stages = length.bit_length()-1
    arithmetic = ((1+32*U)**(3*stages+1)-1)*(x_norm+x_error)*(3+kernel_error)
    representation = 3*x_error + (x_norm+x_error)*kernel_error
    # Absolute IEEE underflow contributions are bounded by the number
    # of paths times their amplification. L^10 covers this graph.
    assert length <= 2**26 and x_norm < 2**20 and kernel_error < 1
    return arithmetic+representation+Q(1, 2**700)


def _fraction(value):
    if isinstance(value, Q):
        return value
    if isinstance(value, (int, np.integer)):
        return Q(int(value))
    return Q(float(value))


def _word(value):
    return f'0x{int(np.asarray(value, dtype=np.float32).view(np.uint32)):08x}'


def _probability(host, start, margin, cell_lo, cell_hi):
    dimension = host-2
    exponent = 0
    decimal_margin = Q(1)
    while decimal_margin > margin:
        decimal_margin /= 10
        exponent += 1

    # The native evaluation row has norm sqrt((M-2)/M) by Parseval.
    # On the projected sphere of radius R=sqrt(M/32), an Euclidean ball
    # of radius decimal_margin about the witness stays inside the cells.
    # Align independent standard Gaussian coordinates with this witness.
    # If Z1 in [1,2] and every other |Zi| < a <= margin/(2M), then the
    # normalized direction moves by <=2R*sqrt(d-1)*a <= margin.
    # P(Z1 in [1,2]) > 1/100 and P(|Zi| < a) > a/4 for a <= 1.
    # Use pi < 4 and e < 3 to bound the Gaussian density.
    coordinate_exponent = exponent+len(str(2*host))
    lower_exponent = 2+(dimension-1)*(coordinate_exponent+1)

    # Native indices on the host grid are the projected host coordinates.
    # Requiring squared energy >= q*R^2 on k of them requires that much
    # energy in the subspace spanned by their projected coordinate axes.
    # This is stochastically bounded above by the energy in a k-plane.
    # For a uniform direction, that fraction has law X/(X+Y), with
    # independent chi-squares of dimensions k and d-k. Markov's
    # inequality at t=1/(4(1-q)) gives
    # 2^(k/2)*(1+q/(2(1-q)))^(-(d-k)/2).
    # ln(2) < 7/10, ln(1+x) >= x/(1+x), and ln(10) < 3.
    first = (-start) % 4
    powers = sorted((max(Q(0), Q(lo), -Q(hi))**2 for lo, hi in
                     zip(cell_lo[first::4], cell_hi[first::4])), reverse=True)
    budget = Q(0)
    best = Q(0)
    used = 0
    for k, power in enumerate(powers[:dimension-1], start=1):
        budget += power
        q = 32*budget/host
        assert q < 1
        negative_log = ((dimension-k)*q/(2-q)-Q(7, 10)*k)/2
        if negative_log > best:
            best, used = negative_log, k
    return dict(projected_dimension=dimension, native_cell_margin_lower=f'1e-{exponent}',
                probability_lower=f'1e-{lower_exponent}',
                probability_upper=f'1e-{best//3}', upper_bound_host_samples=used)


def certify(indices, values, start, expected, *, host=HOST):
    """Confirm strict interior membership in all requested binary32 cells.

    An unresolved sample can touch a rounding boundary. Its alternatives
    do not certify any particular joint rounded word.
    """
    assert 0 < host <= 2**23 and host % 2 == 0
    assert len(indices) == len(values) and isinstance(start, (int, np.integer))
    samples = np.asarray(expected)
    assert samples.ndim == 1 and samples.dtype == np.float32
    assert np.all(np.isfinite(samples))
    assert len(samples) and 0 <= start <= 4*host-len(samples)
    support = {}
    for j, v in zip(indices, values):
        assert isinstance(j, (int, np.integer)) and 0 <= j < host
        exact = _fraction(v)
        assert exact.denominator & (exact.denominator-1) == 0
        support[int(j)] = support.get(int(j), Q(0))+exact
    support = {j: v for j, v in support.items() if v}
    assert support
    size = max(map(abs, support.values()))
    scale = Q(2)**(size.numerator.bit_length()-size.denominator.bit_length())
    support = {j: v/scale for j, v in support.items()}
    energy = sum(v*v for v in support.values())
    dc = sum(support.values())
    nyquist = sum((-1)**(j % 2)*v for j, v in support.items())
    projected = energy-(dc**2+nyquist**2)/host
    assert projected > 0
    norm_lo, norm_hi = _sqrt(Q(host, 32)/projected)
    norm = float((norm_lo+norm_hi)/2)
    norm_err = max(abs(Q(norm)-norm_lo), abs(Q(norm)-norm_hi))

    # Split remote clusters before convolution so a distant energy filler
    # costs one local FFT instead of extending the convolution by its offset.
    groups = []
    for j, v in sorted(support.items()):
        if not groups or j-groups[-1][-1][0] > max(512, len(samples)//4):
            groups.append([])
        groups[-1].append((j, v))
    denominator = max(v.denominator for v in support.values())
    blocks = []
    setup = {}
    raw = np.zeros(len(samples))
    error = Q(0)
    partial_magnitudes = Q(0)
    x_norm = _sqrt(energy)[1]
    for group in groups:
        first, last = group[0][0], group[-1][0]
        span = 4*(last-first)
        origin = int(start)-4*last
        kernel_size = span+len(samples)
        length = 1 << (span+kernel_size-1).bit_length()
        x = np.zeros(length)
        x_error2 = Q(0)
        for j, v in group:
            stored = float(v)
            x[4*(j-first)] = stored
            x_error2 += (Q(stored)-v)**2
        x_error = _sqrt(x_error2)[1]
        group_norm = _sqrt(sum(v*v for _, v in group))[1]

        kernel = np.zeros(length)
        endpoints = [_kernel(origin+j, host) for j in range(kernel_size)]
        width = max(hi-lo for lo, hi in endpoints)
        assert width < S//2**90
        kernel[:kernel_size] = [(lo+hi)/(2*S) for lo, hi in endpoints]
        kernel_error = (3*U + _sqrt(Q(kernel_size))[1]*Q(width, S))/(1-U)
        if length not in setup:
            setup[length] = _fft_setup(length)
        reverse, twiddles = setup[length]
        raw += _fft(_product(_fft(x, reverse, twiddles), _fft(kernel, reverse, twiddles)),
                    reverse, twiddles, inverse=True).real[span:span+len(samples)]
        block_error = _convolution_error(length, group_norm, x_error, kernel_error)
        error += block_error
        partial_magnitudes += 3*group_norm+block_error
        weights = [(4*(last-j), v.numerator*(denominator//v.denominator)) for j, v in group]
        blocks.append((endpoints, weights))
    error += ((1+U)**len(groups)-1)*partial_magnitudes+Q(1, 2**700)
    error = error*abs(Q(norm)) + 3*x_norm*norm_err
    error += U*(3*x_norm*norm_hi+error) + Q(1, 2**1074)
    radius = np.nextafter(float(error), np.inf)
    center = raw*norm
    low = np.nextafter(center-radius, -np.inf)
    high = np.nextafter(center+radius, np.inf)

    # The cell midpoints are exact in binary64, including those for
    # subnormals. A zero of either sign has a distinct signed cell.
    with np.errstate(over='ignore'):
        before = np.nextafter(samples, np.float32(-np.inf)).astype(np.float64)
        after = np.nextafter(samples, np.float32(np.inf)).astype(np.float64)
    middle = samples.astype(np.float64)
    cell_lo, cell_hi = (before+middle)/2, (middle+after)/2
    cell_lo[(samples == 0) & ~np.signbit(samples)] = 0
    cell_hi[(samples == 0) & np.signbit(samples)] = 0
    cell_lo[np.isneginf(before)] = -float(2**128-2**103)
    cell_hi[np.isposinf(after)] = float(2**128-2**103)

    strict = (low > cell_lo) & (high < cell_hi)
    margins = np.nextafter(np.minimum(low-cell_lo, cell_hi-high), -np.inf)
    margin = Q(float(min(margins[strict]))) if np.any(strict) else None
    mismatches, unresolved = [], []
    direct = np.flatnonzero(~strict)
    for offset in direct:
        lower, upper = 0, 0
        offset_int = int(offset)
        for endpoints, weights in blocks:
            for lag, v in weights:
                lo, hi = endpoints[lag+offset_int]
                if v < 0:
                    lo, hi = hi, lo
                lower += v*lo
                upper += v*hi
        corners = [a*b/(S*denominator) for a in (lower, upper) for b in (norm_lo, norm_hi)]
        lo, hi = min(corners), max(corners)
        gap = min(lo-Q(cell_lo[offset]), Q(cell_hi[offset])-hi)
        if gap > 0:
            strict[offset] = True
            margin = gap if margin is None else min(margin, gap)
            continue
        row = dict(native_index=int(start+offset), expected=_word(samples[offset]),
                   lower=str(lo), upper=str(hi))
        if hi < Q(cell_lo[offset]) or lo > Q(cell_hi[offset]):
            row['suggested'] = _word(np.float32(float((lo+hi)/2)))
            mismatches.append(row)
        else:
            # Outward-expand the two endpoint words for any double rounding.
            with np.errstate(over='ignore'):
                lo_word = np.nextafter(np.float32(float(lo)), np.float32(-np.inf))
                hi_word = np.nextafter(np.float32(float(hi)), np.float32(np.inf))
            row['enclosing_alternative_words'] = [_word(lo_word), _word(hi_word)]
            unresolved.append(row)
    result = dict(strict_support=bool(np.all(strict)), checked_samples=len(samples),
                  strictly_inside=int(strict.sum()), fallback_samples=len(direct),
                  fft_sample_radius=float(radius), mismatches=mismatches, unresolved=unresolved)
    if result['strict_support']:
        assert margin is not None and margin > 0
        result.update(_probability(host, int(start), margin, cell_lo, cell_hi))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('word', type=Path, help='NPZ with indices, values, start, expected and optional host')
    args = parser.parse_args()
    with np.load(args.word) as data:
        result = certify(data['indices'], data['values'], int(data['start']), data['expected'],
                         host=int(data['host']) if 'host' in data else HOST)
    print(json.dumps(result, indent=2))

"""Enclose the stationary dry band powers of the randomly phased fresh-record law."""
import argparse
from fractions import Fraction as Q
from hashlib import sha256
from pathlib import Path

import numpy as np

from record_bank import EDGES
from survey import atomic_json
from wet_power_certificate import FFT, HOST, NATIVE, U, TINY, DOWN, UP


def point(x):
    return x, x


def rational(x):
    f = float(x)
    return DOWN(f), UP(f)


def add(a, b):
    return DOWN(a[0]+b[0]), UP(a[1]+b[1])


def neg(a):
    return -a[1], -a[0]


def mul(a, b):
    ll, lh, hl, hh = a[0]*b[0], a[0]*b[1], a[1]*b[0], a[1]*b[1]
    return DOWN(np.minimum(np.minimum(ll, lh), np.minimum(hl, hh))), UP(np.maximum(np.maximum(ll, lh), np.maximum(hl, hh)))


def inverse_positive(a):
    assert np.all(a[0] > 0)
    return DOWN(1/a[1]), UP(1/a[0])


def dot_rows(a, b):
    lo, hi = mul(a, b)
    # Directed pairwise sum, including cancellation in tapered Fourier bins.
    while lo.shape[-1] > 1:
        n = lo.shape[-1]//2
        if lo.shape[-1] % 2:
            last = lo[..., -1:], hi[..., -1:]
        else:
            last = None
        lo, hi = add((lo[..., :n], hi[..., :n]), (lo[..., n:2*n], hi[..., n:2*n]))
        if last is not None:
            lo, hi = np.concatenate((lo, last[0]), -1), np.concatenate((hi, last[1]), -1)
    return lo[..., 0], hi[..., 0]


def sin_pi(n, period):
    # Exact integer reduction before evaluating sin(pi*n/period) in the first quadrant.
    residue = n % (2*period)
    sign = np.where(residue < period, 1., -1.)
    distance = np.minimum(residue % period, period-residue % period)
    pi = DOWN(3.141592653589793), UP(3.141592653589794)
    angle = mul(mul(pi, point(distance)), rational(Q(1, period)))
    factor = neg(mul(angle, angle))
    total = term = angle
    for k in range(1, 14):
        term = mul(mul(term, factor), rational(Q(1, 2*k*(2*k+1))))
        total = add(total, term)
    # On [0, pi/2] the next term is bounded by 1.571^29/29! < 1e-24.
    return mul(add(total, (-1e-24, 1e-24)), point(sign))


def record_covariance(length):
    lag = np.arange(1, length, dtype=np.int64)
    assert length < NATIVE
    numerator = sin_pi((HOST-1)*lag, NATIVE)
    periodic = add(mul(numerator, inverse_positive(sin_pi(lag, NATIVE))), point(-1.))
    overlap = mul(point(NATIVE-lag), rational(Q(1, NATIVE*32*(HOST-2))))
    lo, hi = mul(periodic, overlap)
    return np.r_[1/32, lo], np.r_[1/32, hi]


def dyadic(values):
    values = [Q(float(x)) for x in values]
    scale = max(x.denominator for x in values)
    return np.array([int(x*scale) for x in values], object), scale


def autocorrelation(taps, scale):
    # Python-integer convolution includes every product exactly. Scaling is dyadic.
    exact = np.correlate(taps, taps, 'full')
    assert scale & (scale-1) == 0
    center = np.ldexp(exact.astype(float), -2*(scale.bit_length()-1))
    return DOWN(center), UP(center)


def take(interval, index):
    return interval[0][index], interval[1][index]


def bin_powers(taps, scale, stride, window, cosines):
    cov = record_covariance(stride*(FFT-1)+len(taps))
    fir = autocorrelation(taps, scale)
    sample = np.arange(FFT)
    offsets = np.arange(1-len(taps), len(taps))
    filtered = [dot_rows(take(cov, abs(stride*j[:,None]+offsets)), fir)
                for j in np.array_split(sample, 32)]
    weighted = mul(window, tuple(map(np.concatenate, zip(*filtered))))
    weighted = mul(weighted, point(np.r_[1., np.full(FFT-1, 2.)]))
    bins = np.arange(FFT//2+1)
    full = [dot_rows(take(cosines, (k[:,None]*sample) % FFT), weighted)
            for k in np.array_split(bins, 32)]
    return tuple(map(np.concatenate, zip(*full)))


def main(coefficients, output):
    digest = sha256(coefficients.read_bytes()).hexdigest()
    assert digest == '2beacba39454322758b8c2a2fa37150c989bacab21d44ee81fa71d7b2082caa0'
    with np.load(coefficients) as z:
        first, fscale = dyadic(z['first'])
        second, sscale = dyadic(z['second'])
        hann, wscale = dyadic(z['window'])
        np.testing.assert_array_equal(z['edges'], EDGES)
    two = np.zeros(4*(len(second)-1)+1, object)
    two[::4] = second
    bass = np.convolve(first, two)
    window = take(autocorrelation(hann, wscale), slice(FFT-1,None))
    assert Q(sum(hann*hann), wscale**2*1536) < Q(1000001,1000000)**2
    cosines = sin_pi(2*np.arange(FFT)+FFT//2, FFT)
    results = []
    for taps, scale, stride, start in [(bass, fscale*sscale, 32, 0), (first, fscale, 4, 60)]:
        psd = bin_powers(taps, scale, stride, window, cosines)
        rate = 192000//stride
        bw = Q(rate, FFT)
        # Parseval, the FIR L1 norm, and pointwise binary32 rounding in ensemble RMS.
        gain = Q(sum(abs(taps)),scale)*Q(1000001,1000000)
        error = rational(gain*(2*U*Q(177,1000)+TINY))[1]
        for i in range(start, start+60):
            low, high = map(Q, map(float, EDGES[i:i+2]))
            band = point(0.)
            for k in range((low/bw+Q(1,2)).__floor__(), (high/bw+Q(1,2)).__ceil__()):
                assert 0 < k < FFT//2
                width = min(high,(k+Q(1,2))*bw)-max(low,(k-Q(1,2))*bw)
                if width > 0:
                    band = add(band, mul(take(psd,k), rational(2*width/(rate*1536))))
            assert band[0] > 0
            lower = DOWN(DOWN(np.sqrt(band[0]))-error)
            upper = UP(UP(np.sqrt(band[1]))+error)
            assert lower > 0
            results.append((DOWN(lower*lower), UP(upper*upper)))
    lo, hi = np.asarray(results).T
    atomic_json(output, dict(coefficients_sha256=digest, dry_power_lower=lo.tolist(),
                dry_power_upper=hi.tolist(), max_band_width_db=float(max(10*np.log10(hi/lo)))))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('coefficients', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    main(args.coefficients, args.output)

"""Certify support and dry-state erasure for a frozen periodic noise word."""
import argparse
import ctypes as ct
from fractions import Fraction as Q
from hashlib import sha256
from math import isqrt
from pathlib import Path

import numpy as np

from detector_memory import check_tube
from finite_stock import check as check_stock
from fresh_dry_power import HOST, NATIVE, DOWN, add, mul, point, rational, sin_pi
from record_bank import bind
from survey import F32, atomic_json


def sqrt_bracket(num):
    scale = 2**96
    lower = isqrt(num*scale*scale)
    return Q(lower, scale), Q(lower+1, scale)


def certify_word(period, modes, weights, phases):
    assert NATIVE%period == 0 and period%2 == 0
    assert len(modes) == len(weights) == len(phases)
    assert len(set(modes)) == len(modes) and all(w > 0 for w in weights)
    assert all(0 < mode*NATIVE//period < HOST//2 for mode in modes)
    d = 21*period
    # cos(2*pi*m*(n+1/3)/period+pi*phase/7) represented as a sine.
    def phase_num(n, m, phase):
        return 42*m*n + 14*m + 3*period*phase + d//2
    n = np.arange(period, dtype='i8')
    signal = point(np.zeros(period))
    for m,w,p in zip(modes,weights,phases):
        signal = add(signal,mul(point(w),sin_pi(phase_num(n,m,p),d)))
    root = sqrt_bracket(sum(w*w for w in weights))
    scale = rational(Q(1,4*root[1]))[0], rational(Q(1,4*root[0]))[1]
    lo,hi = mul(signal,scale)
    center = ((lo+hi)*.5).astype('f4')

    def bounds(samples):
        a = np.nextafter(samples,np.float32(-np.inf)).astype('f8')
        b = np.nextafter(samples,np.float32(np.inf)).astype('f8')
        return (a+samples)*.5,(b+samples)*.5

    left,right = bounds(center)
    assert np.all(left < lo) and np.all(hi < right)
    margin = float(np.min(np.minimum(DOWN(lo-left),DOWN(right-hi))))
    assert margin > 0
    # Orthogonality on this native period gives exact mean square 1/32.
    # Frequencies lie in the admitted nonconstant, non-Nyquist host subspace.
    # Positive distance to every midpoint makes its normalized preimage open.
    return np.tile(center,NATIVE//period), margin


def probability_exponent(margin, weights):
    # Let h* be the witness sampled at native indices 4j. At h*, the host
    # projection and fixed-RMS normalization are identities. Projection is
    # nonexpansive, normalization inflates distance to h* by at most two,
    # and zero-padding interpolation has Euclidean gain two.
    assert sum(weights)**2 < 16*sum(w*w for w in weights)  # |h*_j| < 1
    b = 1
    while 8*HOST*Q(1,2**b) >= Q(margin):
        b += 1
    # A full-dimensional input cube of radius 2^-b then maps strictly
    # into the rounding cells. Its coordinates lie within [-2,2], where
    # the standard Gaussian density exceeds 1/32. Independence gives
    # probability greater than 2^(-exponent).
    return (b+4)*HOST


def certify_erase(path, knobs, word):
    library = ct.CDLL(str(path.resolve()))
    erase = library.DetectorMemory
    erase.argtypes = [ct.c_double]*3+[ct.c_int,F32]+[
        np.ctypeslib.ndpointer(t) for t in ('i4','u8','u4')]
    erase.restype = ct.c_int
    steps = np.zeros(3,'i4')
    sizes = np.zeros(3,'u8')
    terminal = np.zeros(1,'u4')
    code = erase(*knobs,len(word),word,steps,sizes,terminal)
    return dict(code=code, steps=steps.tolist(), sizes=sizes.tolist(), terminal=int(terminal[0]))


def certify(grid, detector, stock, output, period, modes, weights, phases):
    with np.load(grid/'screen-v3/survivor-replay-250/fresh-parents.npz') as data:
        knobs = data['settings'][0]
    words = np.empty(11,'f4')
    bind(stock).Coefficients(*knobs,words)
    check_stock()
    check_tube(words)
    word,margin = certify_word(period,modes,weights,phases)
    assert np.max(np.abs(word)) < 241
    result = certify_erase(detector,knobs,word)
    assert result['code'] == 0, result
    report = dict(settings=knobs.tolist(), period=period, modes=modes, weights=weights, phases=phases,
                  minimum_rounding_margin_lower=margin,
                  gaussian_probability_lower_negative_power_of_two=probability_exponent(margin,weights),
                  word_sha256=sha256(word.astype('<f4',copy=False).tobytes()).hexdigest(),
                  detector=result)
    atomic_json(output,report)
    print(report)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('grid',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('--detector',type=Path,
                        default=Path('build/tools/ladder-noise-grid/detector-memory.so'))
    parser.add_argument('--stock',type=Path,
                        default=Path('build/tools/ladder-noise-grid/cycle.so'))
    args = parser.parse_args()
    certify(args.grid,args.detector,args.stock,args.output,7168,
            [3,5,8,9,11],[78,75,52,53,98],[2,5,3,9,10])

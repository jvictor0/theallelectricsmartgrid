"""Check the fixed-stock dry detector tube and replay its lower enclosure."""
import argparse
import ctypes as ct
from fractions import Fraction as Q
from pathlib import Path

import numpy as np

from detector_memory import check_tube
from finite_stock import u
from record_bank import bind
from survey import F32
from wet_feedback_budget import ETA, detector_caps, ema_error_parts


def check_constants(words):
    check_tube(words)
    a, b, c, beta = map(Q, map(float, words[4:8]))
    assert list(words[4:8]) == [
        float.fromhex(value) for value in (
            '0x1.fde64cp-1', '0x1.0bc136p-8', '0x1.1933eap-16', '0x1.289674p-12')
    ]
    tap_cap = detector_caps((a,b,c), 241, (500,1200))
    assert tap_cap < 1410

    def errors(X, Y, I, pad):
        def rnd(v, e=0):
            return u*v+(1+u)*e+pad
        delta = I+Y
        bp = a*X+b*delta
        lp = Y+b*X+c*delta
        eb = rnd(bp,rnd(a*X)+b*rnd(delta))
        el = rnd(lp,rnd(Y+b*X)+c*rnd(delta))
        return eb, rnd(2*bp+X,2*eb)+rnd(2*lp+Y,2*el)

    # Bound each coefficient in these positive linear error models. The
    # exact linear SVF contracts in x²+xy+y² by .996. Each coordinate of a
    # perturbation in this norm is at most 1.16 times its norm.
    n = Q('1.16')
    for j in range(6):
        X,Y,I,r,s,t = (Q(int(i == j)) for i in range(6))
        e1, f1 = errors(X,Y,I,t*ETA)
        e2, f2 = errors(X+n*r,Y+n*r,I+s,t*ETA)
        local = X+Y+Q('.02')*I
        radius = Q('.996')*r+2*(b+c)*s+f1+f2
        tap = n*(a+b)*r+b*s+e1+e2
        radius_impl = Q('.99601')*r+Q('.02')*s+17*u*local+Q('1e-18')*t
        tap_impl = Q('1.17')*r+Q('.005')*s+10*u*local+Q('1e-18')*t
        assert radius <= radius_impl*(1-Q('1e-13'))
        assert tap <= tap_impl*(1-Q('1e-13'))
    for p,v,pad in ((1,0,0), (0,1,0), (0,0,1)):
        assert ema_error_parts(beta,p,v,pad) <= u*p+5*u*beta*(p+v)+4*pad
    assert 1-beta-u-5*u*beta > 0 and 1-5*u > 0

    # Fewer than 30 binary64 operations in each outer recurrence. Relative
    # padding dominates all ordinary rounding in their nonnegative sums and
    # products. The additive pad dominates both subnormal roundoff and the
    # four half-subnormal errors from the binary32 EMA.
    eps = Q(1,2**53)
    assert (1+eps)**30 < 1+Q('1e-12')
    assert (1-eps)**30 > 1-Q('1e-12')
    assert 4*ETA < Q('1e-30')


def bind_lower(path):
    library = ct.CDLL(str(path.resolve()))
    lower = library.DryPowerLower
    lower.argtypes = [ct.c_double]*3+[ct.c_int,F32,F32]
    lower.restype = ct.c_int
    return lower


def check_replay(stock, lower, knobs):
    t = np.arange(50000)
    wave = np.sin(2*np.pi*250*t/192000).astype('f4')
    common = np.concatenate((np.zeros(18000,'f4'), 2*wave))
    lower_power = np.empty_like(common)
    assert lower(*knobs,len(common),common,lower_power) == 0
    assert lower_power[0] == 0 and np.all(lower_power >= 0)
    assert np.max(lower_power) > .458
    state = np.empty(14,'u4')
    for prefix in (np.empty(0,'f4'), 241*wave, -241*wave):
        cycle = stock.CreateCycleFilter(*knobs)
        try:
            if len(prefix):
                stock.RunCycleRecord(cycle,len(prefix),prefix,np.empty_like(prefix))
            scratch = np.empty(31,'f4')
            for i in range(0,len(common)-31,31):
                stock.ReadCycleState(cycle,state)
                assert lower_power[i] <= state.view('f4')[8]
                stock.RunCycleRecord(cycle,31,common[i:i+31],scratch)
        finally:
            stock.DestroyCycleFilter(cycle)
    return float(np.max(lower_power))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('grid',type=Path)
    parser.add_argument('--library',type=Path,
                        default=Path('build/tools/ladder-noise-grid/dry-power-lower.so'))
    parser.add_argument('--stock',type=Path,
                        default=Path('build/tools/ladder-noise-grid/cycle.so'))
    args = parser.parse_args()
    stock = bind(args.stock)
    lower = bind_lower(args.library)
    with np.load(args.grid/'screen-v3/survivor-replay-250/fresh-parents.npz') as data:
        knobs = data['settings'][0]
    words = np.empty(11,'f4')
    stock.Coefficients(*knobs,words)
    check_constants(words)
    print('Maximum certified dry power in replay',check_replay(stock,lower,knobs))

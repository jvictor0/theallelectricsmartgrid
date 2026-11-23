"""Check the wet-detector forcing enclosure constants and bind the C interface."""
import argparse
import ctypes as ct
from fractions import Fraction as Q
from pathlib import Path

import numpy as np

from detector_memory import check_tube
from wet_detector_cap import POWER_CAP, report
from record_bank import bind
from finite_stock import tiny, u


def check(knobs, words):
    report(knobs, words)
    check_tube(words)
    a, b, c, beta = map(Q, map(float, words[4:8]))
    assert tuple(words[4:8].view(np.uint32)) == (0x3f7ef326, 0x3b85e09b, 0x378c99f5, 0x39944b3a)
    assert Q(float(np.float32(2.0780001))) >= POWER_CAP

    def errors(x, y, inp, subnormal):
        def rnd(value, inherited=0):
            return u*value+(1+u)*inherited+subnormal*tiny

        diff = inp+y
        bp, lp = a*x+b*diff, y+b*x+c*diff
        e_bp = rnd(bp, rnd(a*x)+b*rnd(diff))
        e_lp = rnd(lp, rnd(y+b*x)+c*rnd(diff))
        next_x = abs(2*a-1)*x+2*b*(inp+y)
        next_y = 2*b*x+abs(1-2*c)*y+2*c*inp
        return e_bp, rnd(next_x,2*e_bp)+rnd(next_y,2*e_lp)

    # Linear expressions in nonnegative basis coordinates. The reference
    # trajectory and perturbed trajectory have coordinate deviations at
    # most 1.16R. Bound their pair of local rounding perturbations.
    precision_pad = Q(1,10**13)
    for j in range(6):
        x,y,inp,r,e,t = [Q(i == j) for i in range(6)]
        nominal, actual = errors(x,y,inp,t), errors(x+Q('1.16')*r,y+Q('1.16')*r,inp+e,t)
        common = x+y+Q('.02')*inp
        tap = Q('1.16')*(a+b)*r+b*e+nominal[0]+actual[0]
        state = Q('.996')*r+2*(b+c)*e+nominal[1]+actual[1]
        tap_bound = Q('1.17')*r+Q('.0041')*e+10*u*common+Q('1e-18')*t
        state_bound = Q('.99601')*r+Q('.009')*e+Q('10.2')*u*common+Q('1e-18')*t
        assert tap < tap_bound*(1-precision_pad)
        assert state < state_bound*(1-precision_pad)
        if e == 0:
            # The real-centered update is simpler than the binary32 stock
            # step. Budget its binary64 error by a loose gamma(12) times
            # the sum of the term magnitudes (including diff).
            f64 = Q(1,2**53)
            reference_error = 13*f64*(2*x+2*y+Q('.02')*inp)+t*Q('1e-250')
            ref_state = Q('.996')*r+actual[1]+reference_error
            ref_bound = Q('.99601')*r+Q('5.2')*u*common+Q('1e-18')*t
            assert ref_state < ref_bound*(1-precision_pad)

    # EMA uncertainty budget with upper operands p and v. StepPowerBox
    # evaluates two affine endpoints and their common error budget in
    # binary64. Here |vlo| <= vhi, including the negative square padding.
    # Each expression has fewer than 12 arithmetic or constant-rounding
    # nodes. The sum of absolute terms in either endpoint is at most twice
    # p+beta*v+1e-18. Charge 32 binary64 unit roundoffs against the variable
    # terms and a separate allowance for the constant and tiny operations.
    f64 = Q(1,2**53)
    for j in range(3):
        p,v,t = [Q(i == j) for i in range(3)]
        ed = u*(p+v)+t*tiny
        ep = u*beta*(p+v)+(1+u)*beta*ed+t*tiny
        ee = u*(p+beta*v)+(1+u)*ep+t*tiny
        f64_error = 32*f64*(p+beta*v) + (32*f64*Q('1e-18')+Q('1e-250'))*t
        assert ee + f64_error < u*(Q('1.01')*p+Q('3.01')*beta*v)+Q('1e-18')*t


def read(library, knobs, pre):
    ff = np.ctypeslib.ndpointer(dtype=np.float32, flags='C_CONTIGUOUS')
    fn = ct.CDLL(str(library.resolve())).WetDetectorForcing
    fn.argtypes = [ct.c_double]*3+[ct.c_int,ff,ff,ct.POINTER(ct.c_int)]
    fn.restype = ct.c_int
    pre = np.ascontiguousarray(pre, dtype=np.float32)
    assert pre.ndim == 1
    upper = np.empty_like(pre)
    erasure = ct.c_int()
    code = fn(*knobs,len(pre),pre,upper,ct.byref(erasure))
    if code:
        raise ArithmeticError(f'WetDetectorForcing failed with code {code}')
    return upper, erasure.value


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('confirmation', type=Path)
    args = parser.parse_args()
    with np.load(args.confirmation) as archive:
        knobs = archive['knobs']
    stock = bind(Path('build/tools/ladder-noise-grid/cycle.so'))
    words = np.empty(11,'f4')
    stock.Coefficients(*knobs, words)
    check(knobs, words)
    print('Checked the wet-detector forcing arithmetic.')

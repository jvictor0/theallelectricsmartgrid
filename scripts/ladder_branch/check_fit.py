"""Replay saved boxes, identify the sinusoid, and bound a piecewise-linear power fit."""
import argparse
from fractions import Fraction as F
import json
import math
from pathlib import Path

import numpy as np

from enclose import I, ONE, ZERO, UNIT, Tone, padded, solve, verify


def require(condition, message):
    if not condition:
        raise ValueError(message)


def atan_inverse(n):
    # The alternating series brackets its sum after any two successive partial sums.
    partial = sum((F((-1)**i, (2*i+1)*n**(2*i+1)) for i in range(48)), F(0))
    tail = F(1, 97*n**97)
    return I(I.point(partial).l, I.point(partial+tail).h)


def check_tone(tone):
    pi = I.point(16)*atan_inverse(5) - I.point(4)*atan_inverse(239)
    amp = padded(10**(-18/20))
    require(10**9 * amp.l**10 < UNIT**10 < 10**9 * amp.h**10, "amplitude bracket failed")
    # Tighten A for the per-sample checks by exact monotone integer bisection.
    lo, hi = amp.l, amp.h
    while hi-lo > 1:
        mid = (hi+lo)//2
        if 10**9 * mid**10 < UNIT**10:
            lo = mid
        else:
            hi = mid
    amp = I(lo, hi)
    for n in range(tone.q):
        index = n if n <= tone.q//2 else n-tone.q
        angle = pi*I.point(F(-2*index, tone.q))
        cosine, sine = ONE, angle
        tc, ts = ONE, angle
        for j in range(1, 25):
            tc = -(tc*angle*angle)/I.point((2*j-1)*(2*j))
            ts = -(ts*angle*angle)/I.point((2*j)*(2*j+1))
            cosine, sine = cosine+tc, sine+ts
        # Taylor remainder through degree 48 (cos) and degree 49 (sin), |angle| < 4.
        err = I.point(F(4**49, math.factorial(49))).h
        extra = I(-err, err)
        cosine, sine = cosine+extra, sine+extra
        for enclosure, stored in [(cosine, tone.phase[n].real), (sine, tone.phase[n].imag),
                                  (amp*cosine, tone.input[n])]:
            outer = padded(stored)
            require(outer.l <= enclosure.l <= enclosure.h <= outer.h, "sine/quadrature left its enclosure")


def check_anchor(coeff):
    alpha, _, _, _, a1, a2, a3, ema = map(padded, coeff[:8])
    require(0 < alpha.l <= alpha.h < UNIT and 0 < ema.l <= ema.h < UNIT, "one-pole stability")
    two = I.point(2)
    matrix = [[two*a1-ONE, -(two*a2)], [two*a2, ONE-two*a3]]
    for _ in range(8):
        matrix = [[sum((matrix[i][k]*matrix[k][j] for k in range(2)), ZERO)
                   for j in range(2)] for i in range(2)]
    require(max(sum(a.size() for a in row) for row in matrix) < UNIT, "SVF stability")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("witness", type=Path)
    ap.add_argument("out", type=Path)
    args = ap.parse_args()
    witness = json.loads(args.witness.read_text())
    rows = witness["boxes"]
    tone = Tone(18, 20*1000**(56/99), 90/99)
    require(tone.input.tolist() == witness['input'], "forcing mismatch")
    check_tone(tone)
    tone.coeffs = np.asarray(witness["coefficients"])
    check_anchor(tone.coeffs)
    # For each knot, solve the double processor close to a saved branch center.
    which = [i*len(rows)//8 for i in range(8)] + [len(rows)-1]
    knots = []
    for j,i in enumerate(which):
        k = rows[i]["base_feedback"][int(j == 8)]
        tone.set_resonance(np.log1p(48*k/3.9)/np.log(49))
        root = solve(tone, np.array(rows[i]["center"]), None)
        require(root is not None, "knot proposal failed")
        tone.period(root[0])
        fundamental = abs(2*np.mean(tone.output*tone.phase))**2/2
        knots.append((F(k), F(float(fundamental)), F(float(np.mean(tone.output**2)))))
    tone.coeffs = np.asarray(witness["coefficients"])
    worst = [F(1), F(1)]
    previous = None
    idx = 0
    for row in rows:
        lo, hi = row["base_feedback"]
        center, scale = np.array(row["center"]), np.array(row["scales"])
        require(hi > lo and row["radius"] > 0 and np.all(scale > 0), "invalid box")
        inv = [[I.point(v) for v in line] for line in row["inverse"]]
        _, _, radius, ends, checked = verify(tone, lo, hi, center, scale, row['radius'], inv)
        if previous:
            k, old_center, old_scale, endpoint = previous
            require(k == lo, "missing parameter coverage")
            gap = max((((I.point(a)-I.point(b))/I.point(s)).size() +
                       (endpoint*I.point(t)/I.point(s)).h)
                      for a,b,s,t in zip(old_center,center,scale,old_scale,strict=True))
            require(gap <= radius.l, "shared endpoint root outside the next box")
        else:
            require(lo == 0, "missing zero-feedback anchor")
        previous = hi, center, scale, ends[1]
        if F(lo) >= knots[idx+1][0]:
            idx += 1
        left, right = knots[idx:idx+2]
        require(left[0] <= F(lo) < F(hi) <= right[0], "fit knot falls inside a cell")
        for j, field in enumerate(["fundamental_power_dyadic", "total_power_dyadic"]):
            lower, upper = [F(x, UNIT) for x in checked[field]]
            require(lower > 0, "cannot bound a power ratio at zero")
            fits = [left[j+1] + (F(x)-left[0])/(right[0]-left[0])*(right[j+1]-left[j+1]) for x in [lo,hi]]
            require(min(fits) > 0, "fit is not positive")
            worst[j] = max(worst[j], max(fits)/lower, upper/min(fits))
    summary = dict(cells=len(rows), knots=[[float(x) for x in knot] for knot in knots],
                   max_power_ratio_rational=[str(r) for r in worst],
                   max_abs_error_db=[10*math.log10(float(r)) for r in worst])
    with args.out.open("x") as f:
        json.dump(summary, f, indent=2)
    print(summary['max_abs_error_db'])


if __name__ == "__main__":
    main()

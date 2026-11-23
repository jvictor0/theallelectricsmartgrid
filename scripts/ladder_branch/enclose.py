"""Replay a parameter cell of the quiet-detector branch with outward dyadic intervals.

All enclosure arithmetic below uses Python integers on the 2^-120 lattice.
NumPy only proposes a center and a preconditioner. The return map is the exact
real recurrence on the stored coefficient and tone values, with the production
clamped rational saturator. Cells touching the controller knee are declined.
"""
import argparse
from fractions import Fraction
import json
from pathlib import Path

import numpy as np

from pilot import Tone, solve

UNIT = 1 << 120


class I:
    __slots__ = ("l", "h")

    def __init__(self, low, high):
        self.l, self.h = low, high

    @classmethod
    def point(cls, x):
        frac = Fraction(x) * UNIT
        return cls(frac.numerator // frac.denominator, -(-frac.numerator // frac.denominator))

    def __add__(a, b):
        return I(a.l+b.l, a.h+b.h)

    def __neg__(a):
        return I(-a.h, -a.l)

    def __sub__(a, b):
        return a + (-b)

    def __mul__(a, b):
        products = (a.l*b.l, a.h*b.h, a.l*b.h, a.h*b.l)
        return I(min(products)//UNIT, -(-max(products)//UNIT))

    def reciprocal(a):
        if a.l <= 0 <= a.h:
            raise ArithmeticError("division interval contains zero")
        return I(UNIT**2 // a.h, -(-UNIT**2 // a.l))

    def size(a):
        return max(abs(a.l), abs(a.h))

    def __truediv__(a, b):
        return a * b.reciprocal()


ZERO, ONE, TWO, THREE, NINE, TWENTY_SEVEN = map(I.point, (0, 1, 2, 3, 9, 27))


class Jet:
    def __init__(self, val, grad=()):
        self.val = val
        self.grad = grad

    def times(self, k):
        return Jet(self.val*k, [d*k for d in self.grad])

    def __add__(self, other):
        return Jet(self.val+other.val, [a+b for a, b in zip(self.grad, other.grad, strict=True)])

    def __sub__(self, other):
        return Jet(self.val-other.val, [a-b for a, b in zip(self.grad, other.grad, strict=True)])

    def square(self):
        return Jet(self.val*self.val, [d*(TWO*self.val) for d in self.grad])


def sat(x, gain, norm):
    u = x.val * gain
    if u.h < -3*UNIT or u.l > 3*UNIT:
        return Jet((ONE if u.l > 0 else -ONE)/norm, [ZERO]*len(x.grad))
    if u.l <= -3*UNIT or u.h >= 3*UNIT:
        raise ArithmeticError("cell crosses saturation clamp")
    xx = u*u
    den = TWENTY_SEVEN + NINE*xx
    d = THREE*(xx-NINE)/den
    return Jet(u*(TWENTY_SEVEN+xx)/(den*norm), [v*(d*d*gain/norm) for v in x.grad])


def band(signal, z, i, coeff):
    a1, a2, a3 = coeff
    ic1, ic2 = z[i:i+2]
    delta = signal-ic2
    bp = ic1.times(a1)+delta.times(a2)
    z[i] = ic1.times(TWO*a1-ONE)+delta.times(TWO*a2)
    z[i+1] = ic2.times(ONE-TWO*a3)+ic1.times(TWO*a2)+signal.times(TWO*a3)
    return bp


def padded(x):
    pad = UNIT >> 48
    return I.point(x)+I(-pad, pad)


def period(tone, center, scales, radius, feedback, with_derivative, output_bounds=None, feedback_derivative=False):
    alpha, _, gain, norm, a1, a2, a3, ema = map(padded, tone.coeffs[:8])
    z = []
    for i, (x, scale) in enumerate(zip(center, scales, strict=True)):
        width = (I.point(scale)*radius).size()
        val = I.point(x) + I(-width, width)
        grad = [I.point(scales[j]) if i == j else ZERO for j in range(14)] if with_derivative else []
        z.append(Jet(val, [ZERO] if feedback_derivative else grad))
    gains = a1, a2, a3
    for n, x in enumerate(tone.input):
        # A padded dry sample also covers the rounding of the stored digital tone.
        # The theorem here is stated directly for any repeated buffer in these intervals.
        u = Jet(padded(x), [ZERO]*len(z[0].grad))
        if z[8].val.l <= 0 or z[13].val.h >= ((ONE/I.point(25))*z[8].val).l:
            raise ArithmeticError("cannot isolate the inactive detector")
        stage = u-z[3].times(feedback)
        if feedback_derivative:
            stage.grad[0] = stage.grad[0] - z[3].val
        for i in range(4):
            z[i] = z[i].times(ONE-alpha)+stage.times(alpha)
            if i < 3:
                stage = sat(z[i], gain, norm)
        if output_bounds is not None:
            output = sat(Jet(z[3].val), gain, norm).val*(ONE+feedback)
            output_bounds.append((output, padded(tone.phase[n].real), padded(tone.phase[n].imag)))
        for i, v in [(4, u), (9, z[3])]:
            bp = band(band(v, z, i, gains), z, i+2, gains)
            z[i+4] = z[i+4].times(ONE-ema)+bp.square().times(ema)
    return z


def inverse_proposal(tone, center, scales):
    mat = np.eye(14)
    for j in range(14):
        dz = np.zeros(14)
        dz[j] = scales[j]*2e-5
        mat[:, j] -= (tone.period(center+dz)-tone.period(center-dz))/(4e-5*scales)
    return [[I.point(x) for x in row] for row in np.linalg.inv(mat)]


def bounds(tone, center, scales, radius, feedback, inv):
    midpoint = (feedback.l+feedback.h)//2
    delta = I(feedback.l-midpoint, feedback.h-midpoint)
    point = period(tone, center, scales, ZERO, I(midpoint, midpoint), False)
    sensitivity = period(tone, center, scales, ZERO, feedback, False, feedback_derivative=True)
    residual = [(I.point(v)-x.val)/I.point(s) for x, v, s in zip(point, center, scales, strict=True)]
    deriv = [-x.grad[0]/I.point(s) for x, s in zip(sensitivity, scales, strict=True)]
    box = period(tone, center, scales, radius, feedback, True)
    jac = [[(ONE if i == j else ZERO) - d/I.point(scales[i]) for j, d in enumerate(row.grad)] for i, row in enumerate(box)]
    defect = max((sum((b*r for b,r in zip(row, residual, strict=True)), ZERO) +
                  delta*sum((b*d for b,d in zip(row, deriv, strict=True)), ZERO)).size() for row in inv)
    lip = max(sum(((ONE if i == j else ZERO) - sum((b*jac[k][j] for k, b in enumerate(row)), ZERO)).size()
                  for j in range(14)) for i, row in enumerate(inv))
    return defect, lip


def certify(tone, lo, hi, seed, radius):
    middle = (lo+hi)/2
    # The solver takes knob positions. The replay parameter is unclipped base feedback.
    tone.set_resonance(np.log1p(48*middle/3.9)/np.log(49))
    if abs(tone.coeffs[1]-middle) > 1e-10:
        raise ValueError("feedback is clipped")
    root = solve(tone, seed, None)
    if root is None:
        raise ArithmeticError("center solve failed")
    center = root[0]
    scales = np.array([.015625]*4 + [.00390625]*4 + [2**-20] + [.0001220703125]*4 + [2**-20])
    inv = inverse_proposal(tone, center, scales)
    return verify(tone, lo, hi, center, scales, radius, inv)


def verify(tone, lo, hi, center, scales, radius, inv):
    feedback = I(I.point(lo).l, I.point(hi).h)
    rad = I.point(radius)
    defect, lip = bounds(tone, center, scales, rad, feedback, inv)
    if lip >= UNIT or defect*UNIT+lip*rad.h > rad.l*UNIT:
        raise ArithmeticError(f"cell does not close: defect={defect/UNIT:.2e} lip={lip/UNIT:.4g} r={radius:g}")
    # The same contraction gives a smaller a posteriori root tube for measuring power.
    tight = I(defect, defect)/I(UNIT-lip, UNIT-lip)
    phases = []
    period(tone, center, scales, tight, feedback, False, phases)
    count = I.point(len(phases))
    total = sum((v*v for v, _, _ in phases), ZERO)/count
    real = TWO*sum((v*c for v,c,_ in phases), ZERO)/count
    imag = TWO*sum((v*s for v,_,s in phases), ZERO)/count
    fundamental = (real*real+imag*imag)/TWO
    # Enclose the endpoint roots tightly to glue consecutive isolating boxes.
    ends = []
    for k in [lo, hi]:
        endpoint = period(tone, center, scales, ZERO, I.point(k), False)
        residual = [(I.point(v)-x.val)/I.point(s) for x, v, s in zip(endpoint, center, scales, strict=True)]
        eta = max(sum((b*r for b, r in zip(row, residual, strict=True)), ZERO).size() for row in inv)
        ends.append(I(eta, eta)/I(UNIT-lip, UNIT-lip))
    return center, scales, rad, ends, dict(base_feedback=[lo, hi], center=center.tolist(), scales=scales.tolist(),
            radius=radius, inverse=[[v.l/UNIT for v in row] for row in inv],
            fundamental_power_dyadic=[max(0, fundamental.l), fundamental.h],
            total_power_dyadic=[max(0, total.l), total.h], contraction_dyadic_upper=lip,
            contraction_upper=lip/UNIT, defect_upper=defect/UNIT, endpoint_radii=[v.h/UNIT for v in ends])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("--lo", type=float, required=True)
    ap.add_argument("--hi", type=float, required=True)
    ap.add_argument("--cells", type=int, default=1)
    ap.add_argument("--radius", type=float, default=.001)
    args = ap.parse_args()
    if not (0 <= args.lo < args.hi and args.radius > 0 and args.cells > 0):
        ap.error("expected an increasing nonnegative interval, positive cell count and radius")
    tone = Tone(18, 20*1000**(56/99), 90/99)
    seed = tone.anchor
    edges = np.linspace(args.lo, args.hi, args.cells+1)
    previous = None
    rows = []
    for lo, hi in zip(edges, edges[1:]):
        center, scales, radius, ends, row = certify(tone, lo, hi, seed, args.radius)
        if previous:
            old_center, old_end = previous
            # Root of the previous box at the shared endpoint lies in this box.
            gap = max(((I.point(a)-I.point(b))/I.point(s)).size() for a,b,s in zip(old_center, center, scales, strict=True))
            if gap + old_end.h > radius.l:
                raise ArithmeticError("branch boxes do not glue")
        rows.append(row)
        previous = center, ends[1]
        seed = center
    with args.out.open("x") as output:
        json.dump(dict(dyadic_bits=120, input=tone.input.tolist(), input_radius=2**-48, coefficient_radius=2**-48, quadrature_radius=2**-48,
                       dry_reference_lower=.04,
                       coefficients=tone.coeffs.tolist(), boxes=rows), output)
    print(f"{len(rows)} glued cells, K in [{edges[0]}, {edges[-1]}], worst solver contraction {max(r['contraction_upper'] for r in rows):.4g}")


if __name__ == "__main__":
    main()

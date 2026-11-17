import DSP.ResponseFrequency
import DSP.FMA
import DSP.IfLess

namespace DSP.Response

/-- Squared complex modulus without constructing an executable complex number. -/
def normSquared {R : Type} [Mul R] [FMA R] (real imag : R) : R :=
  FMA.fma real real (imag * imag)

/-- Avoid 0/0 for a zero numerator. A nonzero numerator at a pole still divides
by zero: callers must exclude poles, rather than silently hiding them. -/
def ratio {R : Type} [OfNat R 0] [OfNat R 1] [Div R] [IfLess R]
    (numerator denominator : R) : R :=
  numerator / IfLess.ifLess 0 numerator denominator 1

/-- Homogeneous one-pole transfer. Use (a,b)=(sigma,1) in stream proofs and
(a,b)=(i sin(pi f),cos(pi f)) for frequency evaluation. -/
def onePole {R : Type} [Add R] [Mul R] [Div R] (g a b : R) : R :=
  (g * b) / (a + g * b)

def highPole {R : Type} [Add R] [Mul R] [Div R] (g a b : R) : R :=
  a / (a + g * b)

def svf {R : Type} [Add R] [Mul R] [Div R]
    (g k low band high a b : R) : R :=
  (low * (g * b * (g * b)) + band * (g * b) * a + high * (a * a)) /
    (a * a + k * (g * b) * a + g * b * (g * b))

def ladder {R : Type} [Add R] [Mul R] [Div R]
    (g1 g2 g3 g4 k a b : R) : R :=
  let n := (g1 * b) * (g2 * b) * (g3 * b) * (g4 * b)
  n / ((a + g1 * b) * (a + g2 * b) * (a + g3 * b) * (a + g4 * b) + k * n)

end DSP.Response

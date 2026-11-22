import DSP.FMA

namespace DSP.SmoothRandomLFO

structure Cubic (R : Type) where
  a : R
  b : R
  c : R
  d : R

/-- Four consecutive control values and the five knot spans supporting them.
`width` is the span whose cubic is evaluated, in samples. -/
structure Segment (R : Type) where
  p0 : R
  p1 : R
  p2 : R
  p3 : R
  beforePrevious : R
  previous : R
  width : R
  next : R
  afterNext : R

variable {R : Type}

def Segment.advance (s : Segment R) (point span : R) : Segment R :=
  ⟨s.p1, s.p2, s.p3, point, s.previous, s.width, s.next, s.afterNext, span⟩

section Arithmetic
variable [Add R] [Sub R] [Div R]

def Segment.leftSlope (s : Segment R) : R :=
  (s.p1 - s.p0) / (s.beforePrevious + s.previous + s.width)

def Segment.slope (s : Segment R) : R :=
  (s.p2 - s.p1) / (s.previous + s.width + s.next)

def Segment.rightSlope (s : Segment R) : R :=
  (s.p3 - s.p2) / (s.width + s.next + s.afterNext)

def Segment.leftCurve (s : Segment R) : R :=
  (s.slope - s.leftSlope) / (s.previous + s.width)

def Segment.rightCurve (s : Segment R) : R :=
  (s.rightSlope - s.slope) / (s.width + s.next)

/-- A cubic B-spline segment in the power basis, using local phase in [0, 1].
The three slopes are the first derivative control values divided by three.
The two differences of slopes give the endpoint curvatures divided by six. -/
def prepare [Mul R] [OfNat R 3] [FMA R] (s : Segment R) : Cubic R :=
  let bThird := s.width * (s.width * s.leftCurve)
  let cThird := s.width * s.slope - bThird
  ⟨s.width * (s.width * s.rightCurve) - bThird, 3 * bThird, 3 * cThird,
    FMA.fma s.previous s.slope (s.p1 - cThird)⟩

end Arithmetic

def evaluate [FMA R] (p : Cubic R) (phase : R) : R :=
  FMA.fma (FMA.fma (FMA.fma p.a phase p.b) phase p.c) phase p.d

/-- One frame of the smoothed phase clock. Both speeds are in knots per sample. -/
def smoothVelocity [OfNat R 1] [Sub R] [Mul R] [FMA R] (alpha speed draw : R) : R :=
  FMA.fma alpha speed ((1 - alpha) * draw)

end DSP.SmoothRandomLFO

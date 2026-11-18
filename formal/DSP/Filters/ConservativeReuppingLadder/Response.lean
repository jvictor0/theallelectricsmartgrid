import DSP.Filters.ConservativeReuppingLadder
import DSP.Response

namespace DSP.ConservativeReuppingLadder

/-- Coefficients of the small-signal transfer, captured with audio tuning.
Dividing out alpha^4 keeps low-cutoff power computations away from underflow. -/
structure Snapshot (R : Type) where
  memoryRatio : R
  slope : R
  k : R

instance : Functor Snapshot where
  map f s := ⟨f s.memoryRatio, f s.slope, f s.k⟩

def snapshot {R : Type} [OfNat R 1] [Sub R] [Div R]
    (state : ConservativeReuppingLadder R) : Snapshot R :=
  ⟨(1-state.alpha)/state.alpha, NormalizedTanh.slopeZero state.saturation, state.k⟩

def prepareCoefficients {R : Type} [OfNat R 1] [OfNat R 2] [OfScientific R]
    [Add R] [Sub R] [Mul R] [Div R] [IfLess R] [Trig R] [SquareRoot R] [Tanh R]
    (spec : LogicalSpec R) (state : ConservativeReuppingLadder R) :
    ConservativeReuppingLadder R × Snapshot R :=
  let next := computeCoefficients spec state
  (next, snapshot next)

/-- The exact delayed small-signal transfer's power. Half-angle preparation
avoids subtracting a rounded cos(omega) from one near DC. No square root or
executable complex response is needed. -/
def Snapshot.magnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Neg R] [Mul R] [Div R] [FMA R] [IfLess R]
    (s : Snapshot R) (frequency : ResponseFrequency R) : R :=
  let re := FMA.fma (2*s.memoryRatio) (frequency.sinHalf*frequency.sinHalf) 1
  let im := -(s.memoryRatio*frequency.zInverse.imag)
  let re2 := FMA.fma re re (-(im*im))
  let im2 := 2*re*im
  let slope2 := s.slope*s.slope
  let feedback := s.k*slope2*s.slope
  let denominatorRe := FMA.fma feedback frequency.zInverse.real (FMA.fma re2 re2 (-(im2*im2)))
  let denominatorIm := FMA.fma feedback frequency.zInverse.imag (2*re2*im2)
  let numerator := (1+s.k)*slope2*slope2
  Response.ratio (numerator*numerator) (Response.normSquared denominatorRe denominatorIm)

def prepareCoefficientsFloat := prepareCoefficients (R := Float)
def Snapshot.magnitudeSquaredFloat := Snapshot.magnitudeSquared (R := Float)

end DSP.ConservativeReuppingLadder

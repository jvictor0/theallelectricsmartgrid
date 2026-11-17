import DSP.Filters.OnePoleZOHLowPass
import DSP.IfLess
import DSP.ResponseFrequency

namespace DSP.OnePoleZOHLowPass

/-- The digital transfer, with `q` denoting one sample of delay.
Only arithmetic operations are required; their algebraic laws belong to proofs. -/
def transfer {R : Type} [OfNat R 1] [Sub R] [Mul R] [Div R]
    (alpha q : R) : R :=
  alpha / (1 - (1 - alpha) * q)

/-- Squared magnitude for a prepared `sinHalf = sin (π * f)`, where `f` is in
cycles per sample. The supported coefficient range is `0 ≤ alpha ≤ 1`.
At zero alpha the response from rest is zero. Select a safe denominator before
dividing, so floating-point execution avoids `0 / 0` even at DC. -/
def Snapshot.magnitudeSquared {R : Type} [OfNat R 0] [OfNat R 1] [OfNat R 4]
    [Sub R] [Mul R] [Div R] [FMA R] [IfLess R]
    (s : Snapshot R) (frequency : ResponseFrequency R) : R :=
  let alphaSquared := s.alpha * s.alpha
  let sinHalf := frequency.sinHalf
  let denominator := FMA.fma (4 * (1 - s.alpha)) (sinHalf * sinHalf) alphaSquared
  alphaSquared / IfLess.ifLess 0 s.alpha denominator 1

def Snapshot.magnitudeSquaredFloat := Snapshot.magnitudeSquared (R := Float)

end DSP.OnePoleZOHLowPass

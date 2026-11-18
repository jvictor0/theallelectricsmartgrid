import DSP.Nonlinear
import DSP.Transcendental

namespace DSP.ReuppingLadder

/-- Euler-stage corner and delayed unit-slope loop boundary.
The pole ratio is computed directly from the sine to avoid subtracting a
rounded alpha from one. No detector is part of these coefficients. -/
structure Tuning (R : Type) where
  cutoff : R
  alpha : R
  poleRatio : R
  criticalFeedback : R

def criticalForRatio {R : Type} [OfNat R 1] [OfNat R 2]
    [Add R] [Mul R] [Div R] [IfLess R] (poleRatio : R) : R :=
  let crossingGain := poleRatio + 2
  let nyquistGain := 1 + 2 / maxValue poleRatio 2
  let nyquistSquared := nyquistGain * nyquistGain
  IfLess.ifLess poleRatio 2
    (crossingGain * crossingGain) (nyquistSquared * nyquistSquared)

def prepareFromSine {R : Type} [OfNat R 1] [OfNat R 2]
    [Add R] [Sub R] [Mul R] [Div R] [IfLess R] [SquareRoot R]
    (cutoff sine : R) : Tuning R :=
  let root := SquareRoot.sqrt (1 + sine * sine)
  let alpha := sine * 2 * (root - sine)
  let poleRatio := sine * 2 * (root + sine)
  ⟨cutoff, alpha, poleRatio, criticalForRatio poleRatio⟩

def prepareTuning {R : Type} [OfNat R 1] [OfNat R 2] [OfScientific R]
    [Add R] [Sub R] [Mul R] [Div R] [IfLess R] [Trig R] [SquareRoot R]
    (requestedCutoff : R) : Tuning R :=
  let cutoff := minValue 0.5 requestedCutoff
  let sine := Trig.sin2pi (cutoff * 0.5)
  prepareFromSine cutoff sine

end DSP.ReuppingLadder

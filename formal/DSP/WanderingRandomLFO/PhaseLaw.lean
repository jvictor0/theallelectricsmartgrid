import DSP.WanderingRandomLFO.ProbabilityModel
import Mathlib.MeasureTheory.Group.Convolution

namespace DSP.WanderingRandomLFO

open MeasureTheory ProbabilityTheory
open scoped ENNReal

noncomputable section

def phaseStepLaw (lambda coefficient : ℝ) : Measure Phase :=
  (expMeasure (max 1 lambda)).map (fun x : ℝ => (coefficient * min 1 x : Phase))

instance (lambda coefficient : ℝ) : IsProbabilityMeasure (phaseStepLaw lambda coefficient) := by
  letI : IsProbabilityMeasure (expMeasure (max 1 lambda)) :=
    isProbabilityMeasure_expMeasure (zero_lt_one.trans_le (le_max_left _ _))
  apply Measure.isProbabilityMeasure_map
  exact (AddCircle.measurable_mk'.comp (by fun_prop : Measurable
    (fun x : ℝ => coefficient * min 1 x))).aemeasurable

def convolvedPhase (steps : ℕ → Measure Phase) : ℕ → Measure Phase
  | 0 => Measure.dirac 0
  | n+1 => convolvedPhase steps n ∗ steps n

def clockCoefficient (alpha : ℝ) (n : ℕ) (i : ℝ) (j : ℕ) : ℝ :=
  (frameSamples : ℝ) - ((frameSamples : ℝ) - i * (1 - alpha)) * alpha ^ (n - j)

def clockConstant (lambda alpha : ℝ) (n : ℕ) (i : ℝ) : ℝ :=
  let beta := (frameSamples : ℝ) * alpha / (1 - alpha)
  (beta + (i * alpha - beta) * alpha ^ n) / max 1 lambda

/-- A positive fraction of each sufficiently old angular increment is Haar.
The cutoff of 1 knot per sample leaves the entire first angular turn intact. -/
def uniformFraction (lambda : ℝ) : ℝ :=
  min (1 / 2) (max 1 lambda / (frameSamples : ℝ) * Real.exp (-max 1 lambda))

theorem uniformFraction_pos (lambda : ℝ) : 0 < uniformFraction lambda := by
  have h : 0 < max 1 lambda := zero_lt_one.trans_le (le_max_left _ _)
  have hf : 0 < (frameSamples : ℝ) := by norm_num [frameSamples]
  exact lt_min (by norm_num) (mul_pos (div_pos h hf) (Real.exp_pos _))

theorem uniformFraction_lt_one (lambda : ℝ) : uniformFraction lambda < 1 :=
  (min_le_left _ _).trans_lt (by norm_num)

end

end DSP.WanderingRandomLFO

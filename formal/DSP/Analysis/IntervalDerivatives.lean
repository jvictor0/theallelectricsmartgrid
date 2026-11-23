import DSP.Analysis.IntervalArithmetic

/-! Verified second-derivative boxes for the real saturator. This extends the
same rational/tanh witness engine used by the locked-orbit certificate. -/

namespace DSP.IntervalArithmetic
open LeanCert.Core

def curvature (gain inverseNormalization saturation : IntervalRat) : IntervalRat :=
  (((((IntervalRat.singleton (-2)).mul (gain.mul gain)).mul saturation).mul
    ((IntervalRat.singleton 1).sub (saturation.mul saturation)))).mul inverseNormalization

theorem mem_curvature (c : NormalizedTanh.Coefficients ℝ)
    (gain inverseNormalization input saturation : IntervalRat) (depth : ℕ)
    (hg : c.gain ∈ gain) (hi : c.tanhGain⁻¹ ∈ inverseNormalization)
    (hc : IntervalTanh.check (input.mul gain) saturation depth = true)
    (x : ℝ) (hx : x ∈ input) :
    NormalizedTanh.curvature c x ∈ curvature gain inverseNormalization saturation := by
  have hs := IntervalTanh.check_sound _ _ depth hc (x*c.gain) (IntervalRat.mem_mul hx hg)
  have h := IntervalRat.mem_mul (IntervalRat.mem_mul
    (IntervalRat.mem_mul (IntervalRat.mem_mul (IntervalRat.mem_singleton (-2)) (IntervalRat.mem_mul hg hg)) hs)
    (IntervalRat.mem_sub (IntervalRat.mem_singleton 1) (IntervalRat.mem_mul hs hs))) hi
  simpa only [NormalizedTanh.curvature, curvature, div_eq_mul_inv, pow_two,
    Rat.cast_neg, Rat.cast_ofNat, Rat.cast_one] using h

end DSP.IntervalArithmetic

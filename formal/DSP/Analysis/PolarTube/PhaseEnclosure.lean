import DSP.Analysis.PolarTube.Lift
import DSP.Analysis.IntervalArithmetic
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Bounds

/-! Certified output-phase windows without evaluating atan numerically in
the proof. Positive alignment and a bounded cross component place the exact
argument near a proposed real anchor. The anchor's sine and cosine and the
two rational inequalities are independently checked. -/

noncomputable section
namespace DSP.PolarTube
open LeanCert.Core DSP.IntervalArithmetic

theorem abs_arg_le_ratio (z : ℂ) (hz : 0 < z.re) : |z.arg| ≤ |z.im/z.re| := by
  have hπ : |z.arg| < Real.pi/2 := Complex.abs_arg_lt_pi_div_two_iff.mpr (Or.inl hz)
  by_cases ha : 0 ≤ z.arg
  · rw [abs_of_nonneg ha]
    exact (Real.le_tan ha ((le_abs_self _).trans_lt hπ)).trans
      (by rw [Complex.tan_arg]; exact le_abs_self _)
  · have han : z.arg < 0 := lt_of_not_ge ha
    rw [abs_of_neg han]
    have ht := Real.le_tan (neg_nonneg.mpr han.le) (by simpa [abs_of_neg han] using hπ)
    rw [Real.tan_neg,Complex.tan_arg] at ht
    exact ht.trans (neg_le_abs _)

theorem relative_imaginary (z : ℂ) (θ : Real.Angle) :
    (z/unitPhase θ).im = z.im*θ.cos-z.re*θ.sin := by
  have hn : Complex.normSq (unitPhase θ) = 1 := by
    rw [Complex.normSq_eq_norm_sq,unitPhase,norm_unit]
    norm_num
  rw [Complex.div_im,hn]
  simp [unitPhase]

def anchoredPhase (anchor : ℝ) (z : ℂ) : ℝ := anchor+(z/unitPhase (anchor:Real.Angle)).arg

theorem anchoredPhase_angle (anchor : ℝ) (z : ℂ) (hz : z ≠ 0) :
    (anchoredPhase anchor z:Real.Angle) = (z.arg:Real.Angle) := by
  rw [anchoredPhase,Real.Angle.coe_add,Complex.arg_div_coe_angle hz (unitPhase_ne_zero _)]
  rw [show ((unitPhase (anchor:Real.Angle)).arg:Real.Angle) = anchor from
    Complex.arg_cos_add_sin_mul_I_coe_angle _]
  abel

theorem anchoredPhase_window (anchor radius : ℝ) (z : ℂ)
    (ha : 0 < z.re*Real.cos anchor+z.im*Real.sin anchor)
    (hc : |z.im*Real.cos anchor-z.re*Real.sin anchor| ≤
      radius*(z.re*Real.cos anchor+z.im*Real.sin anchor)) :
    |anchoredPhase anchor z-anchor| ≤ radius := by
  have hr : 0 < (z/unitPhase (anchor:Real.Angle)).re := by simpa [relative_real] using ha
  have hb := abs_arg_le_ratio (z/unitPhase (anchor:Real.Angle)) hr
  rw [abs_div,abs_of_pos hr] at hb
  have hdiv : |(z/unitPhase (anchor:Real.Angle)).im|/(z/unitPhase (anchor:Real.Angle)).re ≤ radius :=
    (div_le_iff₀ hr).mpr (by simpa only [relative_imaginary,relative_real,Real.Angle.cos_coe,Real.Angle.sin_coe] using hc)
  simpa only [anchoredPhase,add_sub_cancel_left] using hb.trans hdiv

def phaseWindowCheck (x y cosine sine : IntervalRat) (radius : ℚ) : Bool :=
  let alignment := (x.mul cosine).add (y.mul sine)
  let cross := (y.mul cosine).sub (x.mul sine)
  decide (0 < alignment.lo ∧ 0 ≤ radius ∧ absoluteBound cross ≤ radius*alignment.lo)

theorem phaseWindowCheck_sound (x y cosine sine : IntervalRat) (radius : ℚ)
    (hc : phaseWindowCheck x y cosine sine radius = true) (anchor : ℝ) (z : ℂ)
    (hx : z.re ∈ x) (hy : z.im ∈ y)
    (hcos : Real.cos anchor ∈ cosine) (hsin : Real.sin anchor ∈ sine) :
    |anchoredPhase anchor z-anchor| ≤ (radius:ℝ) := by
  simp only [phaseWindowCheck,decide_eq_true_eq] at hc
  have hal := IntervalRat.mem_add (IntervalRat.mem_mul hx hcos) (IntervalRat.mem_mul hy hsin)
  have hcr := IntervalRat.mem_sub (IntervalRat.mem_mul hy hcos) (IntervalRat.mem_mul hx hsin)
  have ha : (0:ℝ) < (((x.mul cosine).add (y.mul sine)).lo:ℝ) := by exact_mod_cast hc.1
  have hr : (0:ℝ) ≤ (radius:ℝ) := by exact_mod_cast hc.2.1
  have hb : (absoluteBound ((y.mul cosine).sub (x.mul sine)):ℝ) ≤
      (radius:ℝ)*(((x.mul cosine).add (y.mul sine)).lo:ℝ) := by exact_mod_cast hc.2.2
  apply anchoredPhase_window anchor (radius:ℝ) z (ha.trans_le hal.1)
  exact (norm_le_absoluteBound hcr).trans (hb.trans (mul_le_mul_of_nonneg_left hal.1 hr))

end DSP.PolarTube

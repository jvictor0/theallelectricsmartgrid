import DSP.Analysis.IntervalExp
import Mathlib.Analysis.SpecialFunctions.Trigonometric.DerivHyp

/-! A small certificate interface for tight real-tanh bounds. The external
solver proposes rational endpoints; the certificate checks them against a
proved exponential Taylor enclosure. No native reduction is used. -/

namespace DSP.IntervalTanh
open LeanCert.Core

theorem tanh_exp_double (x : ℝ) :
    Real.tanh x = (Real.exp (2*x)-1)/(Real.exp (2*x)+1) := by
  rw [Real.tanh_eq, show 2*x = x+x by ring, Real.exp_add, Real.exp_neg]
  have hp := Real.exp_pos x
  field_simp

def check (input output : IntervalRat) (depth : ℕ) : Bool :=
  let lower := IntervalExp.point (2*input.lo) depth
  let upper := IntervalExp.point (2*input.hi) depth
  decide (|2*input.lo| ≤ 64 ∧ |2*input.hi| ≤ 64 ∧ output.lo ≤ 1 ∧ output.hi ≤ 1 ∧
    output.lo+1 ≤ (1-output.lo)*lower.lo ∧
    (1-output.hi)*upper.hi ≤ output.hi+1)

/-- The only computational premise is a decidable rational check, suitable
for `decide +kernel`. A proposed interval may be rejected; soundness does not
depend on the quality of the solver or its choice of Taylor depth. -/
theorem check_sound (input output : IntervalRat) (depth : ℕ)
    (hc : check input output depth = true) (x : ℝ) (hx : x ∈ input) :
    Real.tanh x ∈ output := by
  simp only [check, decide_eq_true_eq] at hc
  rcases hc with ⟨hql, hqu, ha, hb, hlo, hhi⟩
  have hl := (IntervalExp.mem_point (2*input.lo) depth hql).1
  have hu := (IntervalExp.mem_point (2*input.hi) depth hqu).2
  push_cast at hl hu
  have hel : ((IntervalExp.point (2*input.lo) depth).lo : ℝ) ≤ Real.exp (2*x) :=
    hl.trans (Real.exp_le_exp.mpr (by nlinarith [hx.1]))
  have heu : Real.exp (2*x) ≤ ((IntervalExp.point (2*input.hi) depth).hi : ℝ) :=
    (Real.exp_le_exp.mpr (by nlinarith [hx.2])).trans hu
  have ha' : (output.lo : ℝ) ≤ 1 := by exact_mod_cast ha
  have hb' : (output.hi : ℝ) ≤ 1 := by exact_mod_cast hb
  have hlo' : (output.lo : ℝ)+1 ≤ (1-output.lo)*
      (IntervalExp.point (2*input.lo) depth).lo := by exact_mod_cast hlo
  have hhi' : (1-(output.hi : ℝ))*
      (IntervalExp.point (2*input.hi) depth).hi ≤ output.hi+1 := by exact_mod_cast hhi
  rw [IntervalRat.mem_def, tanh_exp_double]
  have hp : 0 < Real.exp (2*x)+1 := by positivity
  constructor
  · apply (le_div_iff₀ hp).mpr
    nlinarith [mul_le_mul_of_nonneg_left hel (sub_nonneg.mpr ha')]
  · apply (div_le_iff₀ hp).mpr
    nlinarith [mul_le_mul_of_nonneg_left heu (sub_nonneg.mpr hb')]

end DSP.IntervalTanh

import DSP.Filters.NormalizedTanh
import DSP.Nonlinear.Real
import Mathlib.Analysis.SpecialFunctions.Trigonometric.DerivHyp
import Mathlib.Analysis.Calculus.Taylor
import Mathlib.Analysis.Calculus.Deriv.MeanValue

/-! Derivatives and a uniform local Taylor bound for the actual real saturator.
These are proof-side operations; no runtime or snapshot representation changes.
The rational runtime approximation is not identified with real tanh here. -/

noncomputable section
namespace DSP.NormalizedTanh

def slope (c : Coefficients ℝ) (x : ℝ) : ℝ :=
  c.gain * (1-Real.tanh (x*c.gain)^2) / c.tanhGain

def curvature (c : Coefficients ℝ) (x : ℝ) : ℝ :=
  -2*c.gain^2 * Real.tanh (x*c.gain) * (1-Real.tanh (x*c.gain)^2) / c.tanhGain

def thirdDerivative (c : Coefficients ℝ) (x : ℝ) : ℝ :=
  2*c.gain^3 * (1-Real.tanh (x*c.gain)^2) * (3*Real.tanh (x*c.gain)^2-1) / c.tanhGain

theorem tanh_hasDerivAt (x : ℝ) : HasDerivAt Real.tanh (1-Real.tanh x^2) x := by
  have h := (Real.hasDerivAt_sinh x).div (Real.hasDerivAt_cosh x) (ne_of_gt (Real.cosh_pos x))
  have he : Real.tanh = Real.sinh / Real.cosh := funext Real.tanh_eq_sinh_div_cosh
  rw [he]
  exact h.congr_deriv (by
    dsimp only [Pi.div_apply]
    field_simp [ne_of_gt (Real.cosh_pos x)])

theorem hasDerivAt_apply (c : Coefficients ℝ) (x : ℝ) :
    HasDerivAt (apply c) (slope c x) x := by
  have h := (tanh_hasDerivAt (x*c.gain)).comp x ((hasDerivAt_id x).mul_const c.gain)
  exact (h.div_const c.tanhGain).congr_deriv (by dsimp [slope]; ring)

theorem hasDerivAt_slope (c : Coefficients ℝ) (x : ℝ) :
    HasDerivAt (slope c) (curvature c x) x := by
  have h := (tanh_hasDerivAt (x*c.gain)).comp x ((hasDerivAt_id x).mul_const c.gain)
  have hd := ((hasDerivAt_const x (1 : ℝ)).sub (h.pow 2)).const_mul c.gain
  exact (hd.div_const c.tanhGain).congr_deriv (by dsimp [curvature]; ring)

theorem hasDerivAt_curvature (c : Coefficients ℝ) (x : ℝ) :
    HasDerivAt (curvature c) (thirdDerivative c x) x := by
  have h := (tanh_hasDerivAt (x*c.gain)).comp x ((hasDerivAt_id x).mul_const c.gain)
  have hd := (h.mul ((hasDerivAt_const x (1 : ℝ)).sub (h.pow 2))).const_mul (-2*c.gain^2)
  have he : curvature c = fun y => (-2*c.gain^2) *
      (Real.tanh (y*c.gain)*(1-Real.tanh (y*c.gain)^2))/c.tanhGain := by
    funext y; unfold curvature; ring
  rw [he]
  exact (hd.div_const c.tanhGain).congr_deriv (by dsimp [thirdDerivative]; ring)

theorem contDiff_apply (c : Coefficients ℝ) : ContDiff ℝ ⊤ (apply c) := by
  change ContDiff ℝ ⊤ (fun x : ℝ => Real.tanh (x*c.gain)/c.tanhGain)
  simp_rw [Real.tanh_eq_sinh_div_cosh]
  have hm : ContDiff ℝ ⊤ (fun x : ℝ => x*c.gain) := by fun_prop
  exact ((Real.contDiff_sinh.comp hm).div
    (Real.contDiff_cosh.comp hm)
    (fun x => ne_of_gt (Real.cosh_pos _))).div_const _

theorem deriv_apply (c : Coefficients ℝ) : deriv (apply c) = slope c := by
  funext x; exact (hasDerivAt_apply c x).deriv

theorem deriv_slope (c : Coefficients ℝ) : deriv (slope c) = curvature c := by
  funext x; exact (hasDerivAt_slope c x).deriv

theorem deriv_curvature (c : Coefficients ℝ) : deriv (curvature c) = thirdDerivative c := by
  funext x; exact (hasDerivAt_curvature c x).deriv

/-- An exact secant coefficient lies among the slopes between the two input
values. This lets interval derivative bounds control finite differences, not
just infinitesimal perturbations of a candidate trajectory. -/
theorem exists_secant (c : Coefficients ℝ) (lo hi x y : ℝ)
    (hx : x ∈ Set.Icc lo hi) (hy : y ∈ Set.Icc lo hi) :
    ∃ z ∈ Set.Icc lo hi, apply c x-apply c y = slope c z*(x-y) := by
  have hforward (a b : ℝ) (ha : a ∈ Set.Icc lo hi) (hb : b ∈ Set.Icc lo hi) (hab : a < b) :
      ∃ z ∈ Set.Icc lo hi, apply c b-apply c a = slope c z*(b-a) := by
    obtain ⟨z, hz, hs⟩ := exists_hasDerivAt_eq_slope (apply c) (slope c) hab
      (contDiff_apply c).continuous.continuousOn (fun t _ => hasDerivAt_apply c t)
    refine ⟨z, ⟨ha.1.trans hz.1.le, hz.2.le.trans hb.2⟩, ?_⟩
    exact ((eq_div_iff (sub_ne_zero.mpr (ne_of_gt hab))).mp hs).symm
  rcases lt_trichotomy x y with h | h | h
  · obtain ⟨z, hz, hs⟩ := hforward x y hx hy h
    exact ⟨z, hz, by nlinarith⟩
  · exact ⟨x, hx, by rw [h]; simp⟩
  · exact hforward y x hy hx h

theorem thirdDerivative_bound (c : Coefficients ℝ) (x : ℝ) :
    |thirdDerivative c x| ≤ 2*|c.gain|^3/|c.tanhGain| := by
  have ht := (Real.tanh_sq_lt_one (x*c.gain)).le
  have ht0 := sq_nonneg (Real.tanh (x*c.gain))
  have hp : |(1-Real.tanh (x*c.gain)^2)*(3*Real.tanh (x*c.gain)^2-1)| ≤ 1 := by
    rw [abs_le]
    constructor <;> nlinarith [sq_nonneg (3*Real.tanh (x*c.gain)^2-2)]
  unfold thirdDerivative
  rw [show 2*c.gain^3*(1-Real.tanh (x*c.gain)^2)*(3*Real.tanh (x*c.gain)^2-1) =
    (2*c.gain^3)*((1-Real.tanh (x*c.gain)^2)*(3*Real.tanh (x*c.gain)^2-1)) by ring]
  rw [abs_div, abs_mul, abs_mul, abs_pow, abs_of_pos (by norm_num : (0:ℝ)<2)]
  exact div_le_div_of_nonneg_right (by nlinarith [abs_nonneg c.gain, pow_nonneg (abs_nonneg c.gain) 3])
    (abs_nonneg c.tanhGain)

/-- Quadratic local approximation of the exact real saturation. The bound is
uniform in the expansion point and does not assume small gain or small x. -/
theorem quadratic_remainder (c : Coefficients ℝ) (x h : ℝ) :
    |apply c (x+h) - (apply c x + slope c x*h + curvature c x*h^2/2)| ≤
      |c.gain|^3/(3*|c.tanhGain|)*|h|^3 := by
  by_cases hh : h = 0
  · subst h; simp
  have hx : x ≠ x+h := by intro he; apply hh; linarith
  have hu : UniqueDiffOn ℝ (Set.uIcc x (x+h)) := uniqueDiffOn_Icc (min_lt_max.mpr hx)
  have hi (n : ℕ) : iteratedDerivWithin n (apply c) (Set.uIcc x (x+h)) x =
      iteratedDeriv n (apply c) x := by
    apply iteratedDerivWithin_eq_iteratedDeriv hu
    · exact (contDiff_apply c).contDiffAt.of_le (by simp)
    · exact Set.left_mem_uIcc
  obtain ⟨y, _, hy⟩ := taylor_mean_remainder_lagrange_iteratedDeriv (n := 2) hx
    ((contDiff_apply c).of_le (by simp)).contDiffOn
  have hp : taylorWithinEval (apply c) 2 (Set.uIcc x (x+h)) x (x+h) =
      apply c x + slope c x*h + curvature c x*h^2/2 := by
    rw [taylorWithinEval_succ (apply c) 1, taylorWithinEval_succ (apply c) 0,
      taylor_within_zero_eval]
    rw [hi, hi]
    simp [iteratedDeriv_succ, iteratedDeriv_zero, deriv_apply, deriv_slope]
    ring
  rw [hp] at hy
  have h3 : iteratedDeriv 3 (apply c) y = thirdDerivative c y := by
    simp [iteratedDeriv_succ, iteratedDeriv_zero, deriv_apply, deriv_slope, deriv_curvature]
  norm_num [h3, show x+h-x = h by ring] at hy
  rw [hy, abs_div, abs_mul, abs_pow, abs_of_pos (by norm_num : (0:ℝ)<6)]
  calc
    _ ≤ (2*|c.gain|^3/|c.tanhGain|)*|h|^3/6 := by
      gcongr
      exact thirdDerivative_bound c y
    _ = _ := by ring

end DSP.NormalizedTanh

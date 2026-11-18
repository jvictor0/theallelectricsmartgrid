import DSP.Filters.TPTSVF.Convergence

/-! At the Q=1 SVF center, BP and LP approach equal-amplitude quadratures.
The center condition is the exact bilinear relation (1-i*g)*z = 1+i*g.
All trajectories use the existing SVF step and its proved contracting mode. -/

noncomputable section
namespace DSP.TPTSVF.Quadrature
open Filter Convergence
open scoped Topology

def carrier (g : ℝ) : ℂ := (1 + (g : ℂ) * Complex.I) / (1 - (g : ℂ) * Complex.I)

theorem center_denominator_ne_zero (g : ℝ) : (1 - (g : ℂ) * Complex.I) ≠ 0 := by
  intro h
  have hr := congrArg Complex.re h
  simp at hr

theorem carrier_center (g : ℝ) :
    (1 - (g : ℂ) * Complex.I) * carrier g = 1 + (g : ℂ) * Complex.I := by
  unfold carrier
  field_simp [center_denominator_ne_zero g]

theorem center_norm_one (g : ℝ) (z : ℂ)
    (hcenter : (1 - (g : ℂ) * Complex.I) * z = 1 + (g : ℂ) * Complex.I) : ‖z‖ = 1 := by
  have hs : star (1 - (g : ℂ) * Complex.I) = 1 + (g : ℂ) * Complex.I := by simp
  have h := congrArg norm hcenter
  rw [norm_mul, ← hs, norm_star] at h
  exact mul_left_cancel₀ (norm_ne_zero_iff.mpr (center_denominator_ne_zero g))
    (by simpa only [mul_one] using h)

def reference (g : ℝ) (w : ℂ) : TPTSVF ℝ :=
  (prepareFromSnapshot ⟨g, 1⟩
    ⟨0,0,0,0,w.re + g*w.im,w.im - g*w.re,0,0,0,0,0⟩).1

theorem reference_outputs (g : ℝ) (w : ℂ) :
    (step (reference g w) ⟨w.re⟩).bandPass = w.re ∧
      (step (reference g w) ⟨w.re⟩).lowPass = w.im := by
  have hd : g * (g + 1) + 1 ≠ 0 := by nlinarith [sq_nonneg (g + 1 / 2)]
  constructor <;> simp only [reference, step, prepareFromSnapshot, fma_eq] <;> field_simp <;> ring

theorem step_ic1 (s : TPTSVF ℝ) (input : ℝ) :
    (step s ⟨input⟩).ic1 = 2 * (step s ⟨input⟩).bandPass - s.ic1 := by
  simp only [step, fma_eq]
  ring

theorem step_ic2 (s : TPTSVF ℝ) (input : ℝ) :
    (step s ⟨input⟩).ic2 = 2 * (step s ⟨input⟩).lowPass - s.ic2 := by
  simp only [step, fma_eq]
  ring

theorem reference_next_coordinate (g : ℝ) (w z : ℂ)
    (hcenter : (1 - (g : ℂ) * Complex.I) * z = 1 + (g : ℂ) * Complex.I) :
    coordinate (step (reference g w) ⟨w.re⟩) = coordinate (reference g (w * z)) := by
  have hw : (1 - (g : ℂ) * Complex.I) * (w * z) = (1 + (g : ℂ) * Complex.I) * w := by
    linear_combination w * hcenter
  have hr := congrArg Complex.re hw
  have hi := congrArg Complex.im hw
  simp only [Complex.mul_re, Complex.mul_im, Complex.sub_re, Complex.sub_im,
    Complex.add_re, Complex.add_im, Complex.ofReal_re, Complex.ofReal_im,
    Complex.one_re, Complex.one_im, Complex.I_re, Complex.I_im,
    mul_zero, mul_one, one_mul, sub_zero, add_zero, zero_sub] at hr hi
  have h1 : (step (reference g w) ⟨w.re⟩).ic1 = w.re - g * w.im := by
    rw [step_ic1, (reference_outputs g w).1]
    simp only [reference, prepareFromSnapshot]
    ring
  have h2 : (step (reference g w) ⟨w.re⟩).ic2 = w.im + g * w.re := by
    rw [step_ic2, (reference_outputs g w).2]
    simp only [reference, prepareFromSnapshot]
    ring
  apply Complex.ext
  · change (step (reference g w) ⟨w.re⟩).ic1 + (step (reference g w) ⟨w.re⟩).ic2 / 2 = _
    rw [h1, h2]
    simp only [coordinate, reference, prepareFromSnapshot, Complex.mul_re, Complex.mul_im]
    linear_combination -hr - hi / 2
  · change Real.sqrt 3 / 2 * (step (reference g w) ⟨w.re⟩).ic2 = _
    rw [h2]
    simp only [coordinate, reference, prepareFromSnapshot, Complex.mul_re, Complex.mul_im]
    linear_combination -(Real.sqrt 3 / 2) * hi

theorem reference_step (g : ℝ) (hg : 0 < g) (w z : ℂ)
    (hcenter : (1 - (g : ℂ) * Complex.I) * z = 1 + (g : ℂ) * Complex.I) :
    coordinate (reference g (w * z)) = pole g * coordinate (reference g w) + drive g * (w.re : ℂ) := by
  rw [← reference_next_coordinate g w z hcenter]
  exact step_coordinate g _
    (prepareFromSnapshot_tuned ⟨g,1⟩ _ hg.le (by norm_num)) rfl w.re

def secondState (z : ℂ) : ℝ := 2 * z.im / Real.sqrt 3

theorem secondState_coordinate (s : TPTSVF ℝ) : secondState (coordinate s) = s.ic2 := by
  have hs : Real.sqrt 3 ≠ 0 := ne_of_gt (Real.sqrt_pos.mpr (by norm_num))
  simp only [secondState, coordinate]
  field_simp

theorem secondState_sub (z w : ℂ) : secondState (z - w) = secondState z - secondState w := by
  simp only [secondState, Complex.sub_im]
  ring

theorem secondState_tendsto {z : ℕ → ℂ} (h : Tendsto z atTop (𝓝 0)) :
    Tendsto (fun n => secondState (z n)) atTop (𝓝 0) := by
  have him := (Complex.continuous_im.tendsto 0).comp h
  simpa [secondState] using (him.const_mul 2).div_const (Real.sqrt 3)

theorem coordinate_tendsto (g : ℝ) (hg : 0 < g) (s : TPTSVF ℝ) (ht : s.Tuned g) (hk : s.k = 1)
    (input : ℕ → ℝ) (A z : ℂ)
    (hcenter : (1 - (g : ℂ) * Complex.I) * z = 1 + (g : ℂ) * Complex.I)
    (hu : Tendsto (fun n => input n - (A * z ^ n).re) atTop (𝓝 0)) :
    Tendsto (fun n => coordinate (trajectory s input n) - coordinate (reference g (A * z ^ n)))
      atTop (𝓝 0) := by
  apply DSP.Asymptotic.stable_recurrence (pole g) (pole_norm_lt_one g hg) _
    (fun n => drive g * ((input n - (A * z ^ n).re : ℝ) : ℂ))
  · intro n
    change coordinate (step (trajectory s input n) ⟨input n⟩) - _ = _
    rw [step_coordinate g _ (trajectory_tuned g s ht input n)
      (by rw [(states_coefficients s input n).1, hk]), pow_succ, ← mul_assoc,
      reference_step g hg _ z hcenter]
    push_cast
    ring
  · have h := (Complex.continuous_ofReal.tendsto 0).comp hu
    simpa using h.const_mul (drive g)

theorem outputs_tendsto (g : ℝ) (hg : 0 < g) (s : TPTSVF ℝ) (ht : s.Tuned g) (hk : s.k = 1)
    (input : ℕ → ℝ) (A z : ℂ)
    (hcenter : (1 - (g : ℂ) * Complex.I) * z = 1 + (g : ℂ) * Complex.I)
    (hu : Tendsto (fun n => input n - (A * z ^ n).re) atTop (𝓝 0)) :
    Tendsto (fun n => (trajectory s input (n + 1)).bandPass - (A * z ^ n).re) atTop (𝓝 0) ∧
    Tendsto (fun n => (trajectory s input (n + 1)).lowPass - (A * z ^ n).im) atTop (𝓝 0) := by
  have hc := coordinate_tendsto g hg s ht hk input A z hcenter hu
  have h1 := firstState_tendsto hc
  simp only [firstState_sub, firstState_coordinate] at h1
  have h2 := secondState_tendsto hc
  simp only [secondState_sub, secondState_coordinate] at h2
  have href (n : ℕ) : coordinate (step (reference g (A * z ^ n)) ⟨(A * z ^ n).re⟩) =
      coordinate (reference g (A * z ^ (n + 1))) := by
    rw [pow_succ, ← mul_assoc]
    exact reference_next_coordinate g _ z hcenter
  have href1 (n : ℕ) := congrArg firstState (href n)
  have href2 (n : ℕ) := congrArg secondState (href n)
  simp only [firstState_coordinate, step_ic1, (reference_outputs g _).1] at href1
  simp only [secondState_coordinate, step_ic2, (reference_outputs g _).2] at href2
  constructor
  · have h := ((h1.comp (tendsto_add_atTop_nat 1)).add h1).div_const 2
    simp only [zero_add, zero_div] at h
    apply h.congr
    intro n
    have ha := step_ic1 (trajectory s input n) (input n)
    change (trajectory s input (n + 1)).ic1 = 2 * (trajectory s input (n + 1)).bandPass - _ at ha
    dsimp only [Function.comp_def]
    linarith [href1 n]
  · have h := ((h2.comp (tendsto_add_atTop_nat 1)).add h2).div_const 2
    simp only [zero_add, zero_div] at h
    apply h.congr
    intro n
    have ha := step_ic2 (trajectory s input n) (input n)
    change (trajectory s input (n + 1)).ic2 = 2 * (trajectory s input (n + 1)).lowPass - _ at ha
    dsimp only [Function.comp_def]
    linarith [href2 n]

end DSP.TPTSVF.Quadrature

import DSP.Filters.NaiveReuppingLadder.HarmonicBalance
import DSP.Analysis.NormalizedTanhDerivatives

/-! Exact first variation of the existing real sample step at ANY history.
This includes variation around a nonzero oscillating trajectory. It is not a
zero-signal transfer or a theorem that the trajectory exists or attracts.
Only the detector-disabled case is used in this exploration. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance

/-- Replace audio histories, retaining all coefficients and detector fields. -/
def withHistory (s : NaiveReuppingLadder ℝ) (x : Fin 4 → ℝ) : NaiveReuppingLadder ℝ :=
  {s with stage1 := x 0, stage2 := x 1, stage3 := x 2, stage4 := x 3}

structure Variation where
  stage1 : ℝ
  stage2 : ℝ
  stage3 : ℝ
  stage4 : ℝ
  output : ℝ

/-- Derivative of a sample update. Slopes are evaluated at the NEW stage
values, while feedback uses the OLD fourth-stage perturbation. -/
def firstVariation (s next : NaiveReuppingLadder ℝ) (v : Fin 4 → ℝ) (du : ℝ) : Variation :=
  let v1 := s.alpha*(du-s.baseFeedback*v 3)+(1-s.alpha)*v 0
  let v2 := s.alpha*NormalizedTanh.slope s.saturation next.stage1*v1+(1-s.alpha)*v 1
  let v3 := s.alpha*NormalizedTanh.slope s.saturation next.stage2*v2+(1-s.alpha)*v 2
  let v4 := s.alpha*NormalizedTanh.slope s.saturation next.stage3*v3+(1-s.alpha)*v 3
  ⟨v1, v2, v3, v4, (s.baseFeedback+1)*NormalizedTanh.slope s.saturation next.stage4*v4⟩

/-- Chain rule through the actual sample step, retaining its one-sample
feedback delay. Histories and input may follow arbitrary differentiable curves. -/
theorem step_firstVariation (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (x : ℝ → Fin 4 → ℝ) (u : ℝ → ℝ) (t : ℝ) (v : Fin 4 → ℝ) (du : ℝ)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (v j) t) (hu : HasDerivAt u du t) :
    let next := fun r => step (withHistory s (x r)) ⟨u r⟩
    let dv := firstVariation s (next t) v du
    HasDerivAt (fun r => (next r).stage1) dv.stage1 t ∧
    HasDerivAt (fun r => (next r).stage2) dv.stage2 t ∧
    HasDerivAt (fun r => (next r).stage3) dv.stage3 t ∧
    HasDerivAt (fun r => (next r).stage4) dv.stage4 t ∧
    HasDerivAt (fun r => (next r).output) dv.output t := by
  dsimp only
  let next := fun r => step (withHistory s (x r)) ⟨u r⟩
  have hk (r : ℝ) : effectiveFeedback (withHistory s (x r)) = s.baseFeedback := by
    simp [effectiveFeedback, withHistory, hd]
  have e1 (r : ℝ) : (next r).stage1 =
      s.alpha*(u r-s.baseFeedback*x r 3)+(1-s.alpha)*x r 0 := by
    simp only [next, step]
    rw [hk]
    simp only [withHistory, OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  have e2 (r : ℝ) : (next r).stage2 =
      s.alpha*NormalizedTanh.apply s.saturation (next r).stage1+(1-s.alpha)*x r 1 := by
    simp only [next, step, withHistory, OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  have e3 (r : ℝ) : (next r).stage3 =
      s.alpha*NormalizedTanh.apply s.saturation (next r).stage2+(1-s.alpha)*x r 2 := by
    simp only [next, step, withHistory, OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  have e4 (r : ℝ) : (next r).stage4 =
      s.alpha*NormalizedTanh.apply s.saturation (next r).stage3+(1-s.alpha)*x r 3 := by
    simp only [next, step, withHistory, OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  have eo (r : ℝ) : (next r).output =
      NormalizedTanh.apply s.saturation (next r).stage4*(s.baseFeedback+1) := by
    simp only [next, step]
    rw [hk]
    rfl
  let dv := firstVariation s (next t) v du
  have h1 : HasDerivAt (fun r => (next r).stage1) dv.stage1 t := by
    simp_rw [e1]
    exact ((hu.sub ((hx 3).const_mul _)).const_mul _).add ((hx 0).const_mul _)
  have h2 : HasDerivAt (fun r => (next r).stage2) dv.stage2 t := by
    simp_rw [e2]
    exact ((((NormalizedTanh.hasDerivAt_apply _ _).comp t h1).const_mul _).add
      ((hx 1).const_mul _)).congr_deriv (by dsimp [dv, firstVariation]; ring)
  have h3 : HasDerivAt (fun r => (next r).stage3) dv.stage3 t := by
    simp_rw [e3]
    exact ((((NormalizedTanh.hasDerivAt_apply _ _).comp t h2).const_mul _).add
      ((hx 2).const_mul _)).congr_deriv (by dsimp [dv, firstVariation]; ring)
  have h4 : HasDerivAt (fun r => (next r).stage4) dv.stage4 t := by
    simp_rw [e4]
    exact ((((NormalizedTanh.hasDerivAt_apply _ _).comp t h3).const_mul _).add
      ((hx 3).const_mul _)).congr_deriv (by dsimp [dv, firstVariation]; ring)
  have ho : HasDerivAt (fun r => (next r).output) dv.output t := by
    simp_rw [eo]
    exact (((NormalizedTanh.hasDerivAt_apply _ _).comp t h4).mul_const _).congr_deriv
      (by dsimp [dv, firstVariation]; ring)
  exact ⟨h1, h2, h3, h4, ho⟩

end DSP.NaiveReuppingLadder.HarmonicBalance

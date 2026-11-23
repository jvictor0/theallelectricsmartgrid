import DSP.Analysis.AttractingBlock

/-! A posteriori error bounds for equations in a Banach space. The fixed-point
iteration here is a preconditioned EQUATION SOLVER. Its contraction is not a
claim about the physical system's time evolution or its basin of attraction. -/

noncomputable section
namespace DSP.ResidualValidation
open Metric
open scoped NNReal
variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

def correction (residual : E → E) (inverse : E →L[ℝ] E) (x : E) : E :=
  x-inverse (residual x)

theorem fixed_iff (residual : E → E) (inverse : E →L[ℝ] E)
    (hi : Function.Injective inverse) (x : E) :
    correction residual inverse x = x ↔ residual x = 0 := by
  rw [correction,sub_eq_self]
  exact ⟨fun h => hi (by simpa using h),fun h => by rw [h,map_zero]⟩

/-- The residual is in the full equation space. In a Fourier application,
retaining only the solved modes here would omit the truncation error. -/
theorem exists_solution [CompleteSpace E] (residual : E → E) (inverse : E →L[ℝ] E)
    (hi : Function.Injective inverse) (candidate : E) (radius defect : ℝ) (q : ℝ≥0)
    (hr : 0 ≤ radius) (hq : q < 1)
    (hc : LipschitzOnWith q (correction residual inverse) (closedBall candidate radius))
    (hd : ‖inverse (residual candidate)‖ ≤ defect)
    (htrap : defect+q*radius ≤ radius) :
    ∃ exact ∈ closedBall candidate radius, residual exact = 0 ∧
      (∀ other ∈ closedBall candidate radius, residual other = 0 → other = exact) ∧
      dist candidate exact ≤ defect/(1-q) := by
  have he : dist (correction residual inverse candidate) candidate = ‖inverse (residual candidate)‖ := by
    rw [dist_eq_norm,correction]
    simp
  obtain ⟨solution,hs,hfix,hunique,_,herr⟩ :=
    AttractingBlock.exists_attracting_fixedPoint (correction residual inverse) candidate radius q hr hq hc
      (by rw [he]; linarith)
  refine ⟨solution,hs,(fixed_iff residual inverse hi solution).mp hfix,?_,?_⟩
  · intro other ho heq
    exact hunique other ho ((fixed_iff residual inverse hi other).mpr heq)
  · have hden : 0 < (1:ℝ)-q := by exact sub_pos.mpr hq
    apply herr.trans
    rw [dist_comm,he]
    exact div_le_div_of_nonneg_right hd hden.le

/-- Uniform derivative bounds supply the solver contraction. The condition
is on `I - inverse * D residual`, not on the physical sample Jacobian. -/
theorem correction_lipschitz (residual : E → E) (inverse : E →L[ℝ] E)
    (derivative : E → E →L[ℝ] E) (candidate : E) (radius : ℝ) (q : ℝ≥0)
    (hd : ∀ x ∈ closedBall candidate radius, HasFDerivAt residual (derivative x) x)
    (hb : ∀ x ∈ closedBall candidate radius,
      ‖ContinuousLinearMap.id ℝ E-inverse.comp (derivative x)‖₊ ≤ q) :
    LipschitzOnWith q (correction residual inverse) (closedBall candidate radius) := by
  apply AttractingBlock.lipschitz_of_derivative _ _ candidate radius q
  · intro x hx
    exact (hasFDerivAt_id x).sub (inverse.hasFDerivAt.comp x (hd x hx))
  · exact hb

end DSP.ResidualValidation

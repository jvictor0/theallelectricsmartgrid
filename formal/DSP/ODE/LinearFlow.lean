import Mathlib.Analysis.SpecialFunctions.Exponential
import Mathlib.Analysis.Calculus.Deriv.Mul
import Mathlib.MeasureTheory.Integral.IntervalIntegral.FundThmCalculus

namespace DSP.LinearFlow

open NormedSpace intervalIntegral

variable {E : Type} [NormedAddCommGroup E] [NormedSpace ℝ E] [CompleteSpace E]

noncomputable def flow (A : E →L[ℝ] E) (t : ℝ) : E →L[ℝ] E := exp (t • A)

theorem flow_deriv (A : E →L[ℝ] E) (t : ℝ) :
    HasDerivAt (flow A) (A * flow A t) t := hasDerivAt_exp_smul_const' A t

theorem flow_continuous (A : E →L[ℝ] E) : Continuous (flow A) :=
  Differentiable.continuous (fun t => (flow_deriv A t).differentiableAt)

omit [CompleteSpace E] in
@[simp] theorem flow_zero (A : E →L[ℝ] E) : flow A 0 = 1 := by simp [flow]

theorem flow_apply_deriv (A : E →L[ℝ] E) (v : E) (t : ℝ) :
    HasDerivAt (fun r => flow A r v) (A (flow A t v)) t := by
  simpa using (flow_deriv A t).clm_apply (hasDerivAt_const t v)

theorem flow_apply_continuous (A : E →L[ℝ] E) (v : E) : Continuous (fun t => flow A t v) :=
  Differentiable.continuous (fun t => (flow_apply_deriv A v t).differentiableAt)

theorem flow_neg_cancel (A : E →L[ℝ] E) (t : ℝ) (v : E) :
    flow A (-t) (flow A t v) = v := by
  have h : exp (-(t • A)) * exp (t • A) = 1 := by
    have hball (x : E →L[ℝ] E) : x ∈ Metric.eball 0 (expSeries ℝ (E →L[ℝ] E)).radius := by
      rw [expSeries_radius_eq_top ℝ]; exact edist_lt_top _ _
    rw [← exp_add_of_commute_of_mem_ball (𝕂 := ℝ) (Commute.refl (t • A)).neg_left
      (hball _) (hball _), neg_add_cancel, exp_zero]
  simpa [flow, neg_smul] using congrArg (fun L : E →L[ℝ] E => L v) h

noncomputable def inputGain (A : E →L[ℝ] E) (B : E) (t : ℝ) : E :=
  ∫ r in 0..t, flow A r B

theorem inputGain_deriv (A : E →L[ℝ] E) (B : E) (t : ℝ) :
    HasDerivAt (inputGain A B) (flow A t B) t := by
  have hc := flow_apply_continuous A B
  exact integral_hasDerivAt_right (hc.intervalIntegrable _ _)
    hc.aestronglyMeasurable.stronglyMeasurableAtFilter hc.continuousAt

theorem inputGain_equation (A : E →L[ℝ] E) (B : E) (t : ℝ) :
    A (inputGain A B t) = flow A t B - B := by
  rw [inputGain, ← A.intervalIntegral_comp_comm ((flow_apply_continuous A B).intervalIntegrable _ _)]
  have h := integral_eq_sub_of_hasDerivAt (fun r _ => flow_apply_deriv A B r)
    ((A.continuous.comp (flow_apply_continuous A B)).intervalIntegrable 0 t)
  simpa using h

noncomputable def heldTrajectory (A : E →L[ℝ] E) (B initial : E) (u t : ℝ) : E :=
  flow A t initial + u • inputGain A B t

omit [CompleteSpace E] in
@[simp] theorem heldTrajectory_zero (A : E →L[ℝ] E) (B initial : E) (u : ℝ) :
    heldTrajectory A B initial u 0 = initial := by simp [heldTrajectory, inputGain]

theorem heldTrajectory_deriv (A : E →L[ℝ] E) (B initial : E) (u t : ℝ) :
    HasDerivAt (heldTrajectory A B initial u)
      (A (heldTrajectory A B initial u t) + u • B) t := by
  apply ((flow_apply_deriv A initial t).add ((inputGain_deriv A B t).const_smul u)).congr_deriv
  simp [heldTrajectory, inputGain_equation, smul_sub, add_assoc]

theorem heldTrajectory_continuous (A : E →L[ℝ] E) (B initial : E) (u : ℝ) :
    Continuous (heldTrajectory A B initial u) :=
  Differentiable.continuous (fun t => (heldTrajectory_deriv A B initial u t).differentiableAt)

/-- Uniqueness on a closed interval needs derivatives only in its interior. -/
theorem interval_unique (A : E →L[ℝ] E) (f g : ℝ → E) (b : ℝ → E) (T : ℝ)
    (hT : 0 ≤ T) (hf : ContinuousOn f (Set.Icc 0 T)) (hg : ContinuousOn g (Set.Icc 0 T))
    (hfd : ∀ t ∈ Set.Ioo 0 T, HasDerivAt f (A (f t) + b t) t)
    (hgd : ∀ t ∈ Set.Ioo 0 T, HasDerivAt g (A (g t) + b t) t)
    (hinit : f 0 = g 0) : f T = g T := by
  let residual := fun t => flow (-A) t (f t - g t)
  have hc : ContinuousOn residual (Set.Icc 0 T) :=
    (flow_continuous (-A)).continuousOn.clm_apply (hf.sub hg)
  have hd : ∀ t ∈ Set.Ioo 0 T, HasDerivAt residual 0 t := by
    intro t ht
    have he := hasDerivAt_exp_smul_const (-A) t
    apply (he.clm_apply ((hfd t ht).sub (hgd t ht))).congr_deriv
    simp [mul_apply_eq_comp, map_sub]
  have hz := integral_eq_sub_of_hasDerivAt_of_le hT hc hd (intervalIntegrable_const)
  have hr0 : residual 0 = 0 := by simp [residual, hinit]
  have hr : residual T = 0 := by simpa only [integral_zero, hr0, sub_zero] using hz.symm
  have hcancel (v : E) : flow A T (flow (-A) T v) = v := by
    simpa [flow, smul_neg] using flow_neg_cancel A (-T) v
  have h := congrArg (flow A T) hr
  change flow A T (flow (-A) T (f T - g T)) = flow A T 0 at h
  simpa only [hcancel, map_zero, sub_eq_zero] using h

theorem held_interval (A : E →L[ℝ] E) (B initial : E) (u T : ℝ) (hT : 0 ≤ T)
    (q : ℝ → E) (hc : ContinuousOn q (Set.Icc 0 T)) (hi : q 0 = initial)
    (hd : ∀ t ∈ Set.Ioo 0 T, HasDerivAt q (A (q t) + u • B) t) :
    q T = flow A T initial + u • inputGain A B T := by
  exact interval_unique A q (heldTrajectory A B initial u) (fun _ => u • B) T hT hc
    (heldTrajectory_continuous A B initial u).continuousOn hd
    (fun t _ => heldTrajectory_deriv A B initial u t) (by simpa using hi)

end DSP.LinearFlow

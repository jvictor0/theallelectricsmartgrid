import DSP.Filters.Tuning.Composite
import DSP.Stability.Bilinear

noncomputable section
namespace DSP

namespace OnePoleZOHLowPass
/-- The recurrence pole is exp(-2*pi*cutoff), strictly inside the unit disk. -/
theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (state : OnePoleZOHLowPass ℝ)
    (h : spec.Valid) : |1-(computeCoefficients spec state).alpha| < 1 := by
  obtain ⟨ha,hb⟩ := computeCoefficients_bounds spec state h
  rw [abs_of_pos (by linarith)]
  linarith
end OnePoleZOHLowPass

namespace OnePoleTPTLowPass
theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable (fun p => p + (Tuning.warp spec.cutoff : ℂ)) :=
  (Stability.onePole _ (Tuning.warp_pos _ h)).bilinear
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass
theorem computeCoefficients_tuned (spec : LogicalSpec ℝ) (state : OnePoleTPTHighPass ℝ)
    (h : spec.Valid) :
    (computeCoefficients spec state).lowPass.G * (1+Tuning.warp spec.cutoff) = Tuning.warp spec.cutoff :=
  OnePoleTPTLowPass.computeCoefficients_tuned ⟨spec.cutoff⟩ state.lowPass h

theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable (fun p => p + (Tuning.warp spec.cutoff : ℂ)) :=
  (Stability.onePole _ (Tuning.warp_pos _ h)).bilinear
end OnePoleTPTHighPass

namespace TPTSVF
/-- This is the same quadratic denominator used by the snapshot/stream proof. -/
def LogicalSpec.denominator (spec : LogicalSpec ℝ) (p : ℂ) : ℂ :=
  let g := Tuning.warp spec.cutoff
  p*p + (1/spec.q*g : ℝ)*p + (g*g : ℝ)

theorem computeCoefficients_hurwitz (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.Hurwitz spec.denominator :=
  Stability.svf _ _ (Tuning.warp_pos _ h.1) (one_div_pos.mpr h.2)

theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable spec.denominator := (computeCoefficients_hurwitz spec h).bilinear
end TPTSVF

namespace TPTBaseWidth
theorem computeCoefficients_tuned (spec : LogicalSpec ℝ) (state : TPTBaseWidth ℝ) (h : spec.Valid) :
    (computeCoefficients spec state).highPass.lowPass.G*(1+Tuning.warp spec.highPass.cutoff) =
      Tuning.warp spec.highPass.cutoff ∧
    (computeCoefficients spec state).lowPass.G*(1+Tuning.warp spec.lowPass.cutoff) =
      Tuning.warp spec.lowPass.cutoff :=
  ⟨OnePoleTPTHighPass.computeCoefficients_tuned _ _ h.1,
    OnePoleTPTLowPass.computeCoefficients_tuned _ _ h.2⟩

theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable (fun p => (p+(Tuning.warp spec.highPass.cutoff : ℂ))*
      (p+(Tuning.warp spec.lowPass.cutoff : ℂ))) :=
  ((Stability.onePole _ (Tuning.warp_pos _ h.1)).cascade
    (Stability.onePole _ (Tuning.warp_pos _ h.2))).bilinear
end TPTBaseWidth

namespace MorphingTPTSVF
theorem computeCoefficients_tuned (spec : LogicalSpec ℝ) (state : MorphingTPTSVF ℝ) (h : spec.Valid) :
    (computeCoefficients spec state).svf.Tuned (Tuning.warp spec.svf.cutoff) :=
  TPTSVF.computeCoefficients_tuned spec.svf state.svf h.1

theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable spec.svf.denominator := TPTSVF.computeCoefficients_stable _ h.1
end MorphingTPTSVF

namespace TPTSVF4Pole
theorem computeCoefficients_tuned (spec : LogicalSpec ℝ) (state : TPTSVF4Pole ℝ) (h : spec.Valid) :
    (computeCoefficients spec state).stage1.Tuned (Tuning.warp spec.stage1.cutoff) ∧
    (computeCoefficients spec state).stage2.Tuned (Tuning.warp spec.stage2.cutoff) :=
  ⟨TPTSVF.computeCoefficients_tuned _ _ h.1, TPTSVF.computeCoefficients_tuned _ _ h.2⟩

theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable (fun p => spec.stage1.denominator p * spec.stage2.denominator p) :=
  ((TPTSVF.computeCoefficients_hurwitz _ h.1).cascade
    (TPTSVF.computeCoefficients_hurwitz _ h.2)).bilinear
end TPTSVF4Pole

namespace LinkwitzRileyCrossover
theorem computeCoefficients_tuned (spec : LogicalSpec ℝ) (state : LinkwitzRileyCrossover ℝ)
    (h : spec.Valid) :
    (computeCoefficients spec state).first.Tuned (Tuning.warp spec.cutoff) ∧
    (computeCoefficients spec state).lowSecond.Tuned (Tuning.warp spec.cutoff) ∧
    (computeCoefficients spec state).highSecond.Tuned (Tuning.warp spec.cutoff) :=
  ⟨TPTSVF.computeCoefficients_tuned spec.stage _ ⟨h,Tuning.butterworth_q_pos⟩,
   TPTSVF.computeCoefficients_tuned spec.stage _ ⟨h,Tuning.butterworth_q_pos⟩,
   TPTSVF.computeCoefficients_tuned spec.stage _ ⟨h,Tuning.butterworth_q_pos⟩⟩

theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable (fun p => spec.stage.denominator p * spec.stage.denominator p) :=
  let hs := TPTSVF.computeCoefficients_hurwitz spec.stage ⟨h,Tuning.butterworth_q_pos⟩
  (hs.cascade hs).bilinear
end LinkwitzRileyCrossover

namespace TPTLadder
theorem computeCoefficients_ready (spec : LogicalSpec ℝ) (state : TPTLadder ℝ) (h : spec.Valid)
    (h1 : state.stage1.s = 0) (h2 : state.stage2.s = 0)
    (h3 : state.stage3.s = 0) (h4 : state.stage4.s = 0) :
    (computeCoefficients spec state).Ready (Tuning.warp spec.cutoff) (Tuning.warp spec.cutoff)
      (Tuning.warp spec.cutoff) (Tuning.warp spec.cutoff) := by
  refine ⟨OnePoleTPTLowPass.computeCoefficients_tuned ⟨spec.cutoff⟩ _ h.1,
    OnePoleTPTLowPass.computeCoefficients_tuned ⟨spec.cutoff⟩ _ h.1,
    OnePoleTPTLowPass.computeCoefficients_tuned ⟨spec.cutoff⟩ _ h.1,
    OnePoleTPTLowPass.computeCoefficients_tuned ⟨spec.cutoff⟩ _ h.1, h1,h2,h3,h4,?_⟩
  have hg := Tuning.warp_pos spec.cutoff h.1
  have hk := h.2.1
  change 1+spec.feedback*((Tuning.warp spec.cutoff/(1+Tuning.warp spec.cutoff))*
    (Tuning.warp spec.cutoff/(1+Tuning.warp spec.cutoff))*
    (Tuning.warp spec.cutoff/(1+Tuning.warp spec.cutoff))*
    (Tuning.warp spec.cutoff/(1+Tuning.warp spec.cutoff))) ≠ 0
  positivity

theorem computeCoefficients_stable (spec : LogicalSpec ℝ) (h : spec.Valid) :
    Stability.BilinearStable (fun p => (p+(Tuning.warp spec.cutoff : ℂ))^4 +
      (spec.feedback*(Tuning.warp spec.cutoff)^4 : ℝ)) :=
  (Stability.ladder _ _ (Tuning.warp_pos _ h.1) h.2.1 h.2.2).bilinear
end TPTLadder
end DSP

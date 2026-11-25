import DSP.Filters.SquiggleBoyQuadReverb.Frozen.WanderBounds
import Mathlib.MeasureTheory.Integral.Prod

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Matrix MeasureTheory
open scoped Matrix.Norms.L2Operator

noncomputable section

/-- The solved feedback loop depends only on the eight clocks inside the tank. -/
def wanderTankTransfer (s : FrozenState) (q : ℂ) (tank : TankWander) : QuadMatrix :=
  let p := wanderSpec s (0, tank)
  (returnDen p q)⁻¹ * forward p q

/-- Input diffusion depends only on the intro clocks. -/
def wanderIntroTransfer (s : FrozenState) (q : ℂ) (input : IntroWander) : QuadMatrix :=
  diagonal (introResponse q _ (wanderSpec s (input, 0)).intro)

theorem transfer_wanderSpec_eq_mul (s : FrozenState) (q : ℂ) (x : WanderSample) :
    transfer (wanderSpec s x) q =
      wanderTankTransfer s q x.2 * wanderIntroTransfer s q x.1 := by
  have hf : forward (wanderSpec s x) q = forward (wanderSpec s (0, x.2)) q := by
    simp only [forward, dampingResponse, wanderSpec, Spec.complex, prepared, withWander]
  have hb : (wanderSpec s x).feedback = (wanderSpec s (0, x.2)).feedback := by
    simp only [wanderSpec, Spec.complex, prepared, withWander]
  have hi : (wanderSpec s x).intro = (wanderSpec s (x.1, 0)).intro := by
    simp only [wanderSpec, Spec.complex, prepared, withWander]
  unfold transfer wanderTankTransfer wanderIntroTransfer
  rw [mul_assoc, returnDen, returnDen, hf, hb, hi]

private theorem integrable_wanderTankTransfer (s : FrozenState) (q : ℂ)
    (hq : ‖q‖ = 1) (hstable : loopBound s q < 1) (hmain : (prepared s).main.Consecutive) :
    Integrable (wanderTankTransfer s q) tankWanderLaw := by
  let embed (x : TankWander) : WanderSample := (0, x)
  have he : Measurable embed := measurable_const.prodMk measurable_id
  have hb (x : TankWander) := norm_wanderParts_le s q hq hmain (embed x)
  have hi := integrable_resolvedMul_of_contraction (μ := tankWanderLaw)
    (loopBound_nonneg s q) hstable ((measurable_wanderLoopReturn s q).comp he)
    ((measurable_wanderForward s q).comp he) (Filter.Eventually.of_forall fun x => (hb x).2.1)
    (Filter.Eventually.of_forall fun x => (hb x).1)
  change Integrable (fun x : TankWander =>
    (returnDen (wanderSpec s (0, x)) q)⁻¹ * forward (wanderSpec s (0, x)) q) tankWanderLaw
  simpa only [Matrix.nonsing_inv_eq_ringInverse, returnDen, loopReturn, embed, Function.comp_apply] using hi

private theorem integrable_wanderIntroTransfer (s : FrozenState) (q : ℂ) (hq : ‖q‖ = 1) :
    Integrable (wanderIntroTransfer s q) introWanderLaw := by
  refine Integrable.mono' (integrable_const (1 : ℝ))
    (measurable_wanderIntro s q).aestronglyMeasurable ?_
  refine Filter.Eventually.of_forall fun x => ?_
  have htaps := (wanderingTaps_consecutive (withWander s (x, 0))).2.2
  rw [wanderIntroTransfer, Matrix.l2_opNorm_diagonal]
  exact norm_introResponse_le_one q _ _ (fun j i => (htaps j).norm_le_one hq i)

/-- Independence factors the intro out of the whole-reverb expectation.
The tank is solved for each draw before its expectation is taken. -/
theorem expectedWanderTransfer_eq_mul (s : FrozenState) (q : ℂ)
    (hq : ‖q‖ = 1) (hstable : loopBound s q < 1) (hmain : (prepared s).main.Consecutive) :
    expectedWanderTransfer s q =
      (∫ tank, wanderTankTransfer s q tank ∂tankWanderLaw) *
      (∫ input, wanderIntroTransfer s q input ∂introWanderLaw) := by
  simp only [expectedWanderTransfer, expectedTransfer, transfer_wanderSpec_eq_mul, wanderLaw]
  exact integral_prod_bilin (ContinuousLinearMap.mul ℂ QuadMatrix).flip
    (integrable_wanderIntroTransfer s q hq) (integrable_wanderTankTransfer s q hq hstable hmain)

end

end DSP.SquiggleBoyQuadReverb.Frozen

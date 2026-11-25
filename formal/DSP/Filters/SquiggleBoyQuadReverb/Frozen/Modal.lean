import DSP.Filters.SquiggleBoyQuadReverb.Frozen.ModeAllPass
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.ModeDamping

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Matrix

/-- The closed-loop matrix is the wet amplitude of a harmonic solution of
the exact tangent, including all saved histories. -/
theorem transfer_mode (p : Spec ℂ) (q : ℂ) (hq : q ≠ 0)
    (hd : BlockDenominators p q) (hloop : IsUnit (returnDen p q).det) (x : Lanes ℂ) :
    ∃ h : History ℂ, tangent p (h, x) = q⁻¹ • h ∧ h.wet = q • (transfer p q *ᵥ x) := by
  let y := transfer p q *ᵥ x
  let diffused := introResponse q _ p.intro * x
  let write := diffused + q • (p.feedback * y)
  let main := tapeMode squiggleBoyReverbDelayCapacity q write
  let mixed := mix (q⁻¹ • (tapResponse p.main q * write))
  let signal := allPassResponse p.first q * mixed
  let saturated := p.satGain * signal
  let filtered := dampingResponse p q * saturated
  let state : History ℂ := (introMode q _ p.intro x, main, apfMode p.first q mixed,
    dampingMode p q saturated, apfMode p.second q filtered, q • y)
  have hy : allPassResponse p.second q * filtered = y := by
    rw [show y = forward p q *ᵥ write from transfer_solves p q hloop x]
    unfold forward
    rw [← mulVec_mulVec, ← mulVec_mulVec, diagonal_mulLanes, LinearMap.toMatrix'_mulVec,
      diagonal_mulLanes]
    have hmix : (q⁻¹ • tapResponse p.main q) * write = q⁻¹ • (tapResponse p.main q * write) := by
      funext i
      simp only [Pi.smul_apply, Pi.mul_apply, smul_eq_mul]
      ring
    rw [hmix]
    funext i
    simp only [Pi.mul_apply, filtered, saturated, signal, mixed]
    ring
  have hwrite : diffused + laneGain p.feedback state.wet = write := by
    funext i
    simp [write, state, History.wet, laneGain, mul_comm]
  have hread : mix (tapRead p.main (q⁻¹ • main)) = mixed := by
    rw [map_smul, show tapRead p.main main = tapResponse p.main q * write from tapRead_tapeMode p.main q write]
  have hs : laneGain p.satGain signal = saturated := by
    funext i
    rfl
  refine ⟨state, ?_, rfl⟩
  rw [tangent_apply, beforeSAT_apply]
  change afterSAT p _ = _
  have hi := intro_introMode q hq _ p.intro x hd.intro
  dsimp only [state, History.wet]
  rw [hi]
  dsimp only
  rw [show introResponse q _ p.intro * x + laneGain p.feedback (q • y) = write from hwrite]
  rw [show push (main, write) = q⁻¹ • main from push_tapeMode _ q hq write, hread]
  rw [allPass_apfMode p.first q hq mixed hd.first]
  dsimp only
  rw [show allPassResponse p.first q * mixed = signal from rfl, hs, afterSAT_apply]
  dsimp only
  rw [damping_dampingMode p q hq hd]
  dsimp only
  rw [show dampingResponse p q * saturated = filtered from rfl, allPass_apfMode p.second q hq filtered hd.second, hy]
  exact congrArg (fun w => (q⁻¹ • introMode q _ p.intro x, q⁻¹ • main,
    q⁻¹ • apfMode p.first q mixed, q⁻¹ • dampingMode p q saturated, q⁻¹ • apfMode p.second q filtered, w))
    (by rw [smul_smul, inv_mul_cancel₀ hq, one_smul] :
      y = q⁻¹ • (q • y))

end DSP.SquiggleBoyQuadReverb.Frozen

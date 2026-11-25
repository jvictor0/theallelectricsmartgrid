import DSP.Filters.SquiggleBoyQuadReverb.Frozen.ModeParameters

namespace DSP.SquiggleBoyQuadReverb.Frozen

private theorem lowPass_lowMemoryMode (alpha : Lanes ℂ) (q : ℂ) (hq : q ≠ 0)
    (hd : ∀ i, 1 + (2 * alpha i - 1) * q ≠ 0) (x : Lanes ℂ) :
    lowPass alpha (lowMemoryMode alpha q x, x) =
      (q⁻¹ • lowMemoryMode alpha q x, fun i => lowResponse (alpha i) q * x i) := by
  apply Prod.ext
  · funext i
    let m := lowMemoryMode alpha q x i
    change m + alpha i * (x i - m) + alpha i * (x i - m) = q⁻¹ * m
    dsimp [m, lowMemoryMode]
    set denom := 1 + (2 * alpha i - 1) * q
    have hden : denom ≠ 0 := hd i
    field_simp [hq, hden]
    dsimp only [denom]
    ring
  · funext i
    let m := lowMemoryMode alpha q x i
    change m + alpha i * (x i - m) = lowResponse (alpha i) q * x i
    dsimp [m, lowMemoryMode, lowResponse]
    set denom := 1 + (2 * alpha i - 1) * q
    have hden : denom ≠ 0 := hd i
    field_simp [hden]
    dsimp only [denom]
    ring

theorem damping_dampingMode (p : Spec ℂ) (q : ℂ) (hq : q ≠ 0)
    (hd : BlockDenominators p q) (u : Lanes ℂ) :
    damping p.highAlpha p.lowAlpha (dampingMode p q u, u) =
      (q⁻¹ • dampingMode p q u, dampingResponse p q * u) := by
  let high := fun i => highResponse (p.highAlpha i) q * u i
  have hhigh : u - (fun i => lowResponse (p.highAlpha i) q * u i) = high := by
    funext i
    simp only [high, Pi.sub_apply, lowResponse, highResponse]
    set denom := 1 + (2 * p.highAlpha i - 1) * q
    have hden : denom ≠ 0 := hd.high i
    field_simp [hden]
    dsimp only [denom]
    ring
  let first := lowPass p.highAlpha (lowMemoryMode p.highAlpha q u, u)
  change ((first.1, (lowPass p.lowAlpha (lowMemoryMode p.lowAlpha q high, u - first.2)).1),
    (lowPass p.lowAlpha (lowMemoryMode p.lowAlpha q high, u - first.2)).2) = _
  dsimp only [first]
  rw [lowPass_lowMemoryMode p.highAlpha q hq hd.high, hhigh]
  rw [lowPass_lowMemoryMode p.lowAlpha q hq hd.low]
  apply Prod.ext
  · rfl
  · funext i
    simp only [high, dampingResponse, Pi.mul_apply]
    ring

end DSP.SquiggleBoyQuadReverb.Frozen

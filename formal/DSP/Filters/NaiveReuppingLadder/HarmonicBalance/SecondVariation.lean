import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Variation

/-! Exact mixed second variation of the existing detector-disabled sample
step. This is the derivative recurrence used by curved-tube enclosures. The
old fourth-stage feedback and the output saturator are both retained. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance

/-- Differentiate the first variation in another direction. A nonzero mixed
input history represents curvature of the initial coordinate chart. -/
def mixedVariation (s next : NaiveReuppingLadder ℝ) (left right : Variation)
    (mixed : Fin 4 → ℝ) (ddu : ℝ) : Variation :=
  let w1 := s.alpha*(ddu-s.baseFeedback*mixed 3)+(1-s.alpha)*mixed 0
  let w2 := s.alpha*(NormalizedTanh.curvature s.saturation next.stage1*left.stage1*right.stage1+
    NormalizedTanh.slope s.saturation next.stage1*w1)+(1-s.alpha)*mixed 1
  let w3 := s.alpha*(NormalizedTanh.curvature s.saturation next.stage2*left.stage2*right.stage2+
    NormalizedTanh.slope s.saturation next.stage2*w2)+(1-s.alpha)*mixed 2
  let w4 := s.alpha*(NormalizedTanh.curvature s.saturation next.stage3*left.stage3*right.stage3+
    NormalizedTanh.slope s.saturation next.stage3*w3)+(1-s.alpha)*mixed 3
  ⟨w1, w2, w3, w4, (s.baseFeedback+1)*(
    NormalizedTanh.curvature s.saturation next.stage4*left.stage4*right.stage4+
    NormalizedTanh.slope s.saturation next.stage4*w4)⟩

/-- Both first directions and the initial mixed derivative are explicit.
The theorem differentiates `firstVariation`, already proved to differentiate
`step`; it does not introduce another audio implementation. -/
theorem step_mixedVariation (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (x v : ℝ → Fin 4 → ℝ) (u du : ℝ → ℝ) (t : ℝ)
    (dx mixed : Fin 4 → ℝ) (dux ddu : ℝ)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (dx j) t) (hu : HasDerivAt u dux t)
    (hv : ∀ j, HasDerivAt (fun r => v r j) (mixed j) t) (hdu : HasDerivAt du ddu t) :
    let next := fun r => step (withHistory s (x r)) ⟨u r⟩
    let right := fun r => firstVariation s (next r) (v r) (du r)
    let left := firstVariation s (next t) dx dux
    let result := mixedVariation s (next t) left (right t) mixed ddu
    HasDerivAt (fun r => (right r).stage1) result.stage1 t ∧
    HasDerivAt (fun r => (right r).stage2) result.stage2 t ∧
    HasDerivAt (fun r => (right r).stage3) result.stage3 t ∧
    HasDerivAt (fun r => (right r).stage4) result.stage4 t ∧
    HasDerivAt (fun r => (right r).output) result.output t := by
  dsimp only
  let next := fun r => step (withHistory s (x r)) ⟨u r⟩
  let right := fun r => firstVariation s (next r) (v r) (du r)
  let left := firstVariation s (next t) dx dux
  let result := mixedVariation s (next t) left (right t) mixed ddu
  obtain ⟨hl1, hl2, hl3, hl4, _⟩ := step_firstVariation s hd x u t dx dux hx hu
  have h1 : HasDerivAt (fun r => (right r).stage1) result.stage1 t :=
    ((hdu.sub ((hv 3).const_mul _)).const_mul _).add ((hv 0).const_mul _)
  have h2 : HasDerivAt (fun r => (right r).stage2) result.stage2 t := by
    have hs := (NormalizedTanh.hasDerivAt_slope s.saturation (next t).stage1).comp t hl1
    exact (((hs.const_mul s.alpha).mul h1).add ((hv 1).const_mul (1-s.alpha))).congr_deriv
      (by dsimp [result, mixedVariation, right, left, firstVariation]; ring)
  have h3 : HasDerivAt (fun r => (right r).stage3) result.stage3 t := by
    have hs := (NormalizedTanh.hasDerivAt_slope s.saturation (next t).stage2).comp t hl2
    exact (((hs.const_mul s.alpha).mul h2).add ((hv 2).const_mul (1-s.alpha))).congr_deriv
      (by dsimp [result, mixedVariation, right, left, firstVariation]; ring)
  have h4 : HasDerivAt (fun r => (right r).stage4) result.stage4 t := by
    have hs := (NormalizedTanh.hasDerivAt_slope s.saturation (next t).stage3).comp t hl3
    exact (((hs.const_mul s.alpha).mul h3).add ((hv 3).const_mul (1-s.alpha))).congr_deriv
      (by dsimp [result, mixedVariation, right, left, firstVariation]; ring)
  have ho : HasDerivAt (fun r => (right r).output) result.output t := by
    have hs := (NormalizedTanh.hasDerivAt_slope s.saturation (next t).stage4).comp t hl4
    exact ((hs.const_mul (s.baseFeedback+1)).mul h4).congr_deriv
      (by dsimp [result, mixedVariation, right, left, firstVariation]; ring)
  exact ⟨h1, h2, h3, h4, ho⟩

end DSP.NaiveReuppingLadder.HarmonicBalance

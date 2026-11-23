import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.BlockVariation
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Enclosure
import DSP.Analysis.IntervalDerivatives

/-! Interval propagation of the exact mixed variation. Applying this to pairs
of coordinate directions gives the sample-level Hessian checker. Curvature of
the initial chart is retained in the old mixed variation. The chart, its
derivatives and final polar encoding require separate certificates. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure
open LeanCert.Core DSP.IntervalArithmetic

structure VariationBox where
  stage1 : IntervalRat
  stage2 : IntervalRat
  stage3 : IntervalRat
  stage4 : IntervalRat
  output : IntervalRat

def VariationBox.Mem (box : VariationBox) (v : Variation) : Prop :=
  v.stage1 ∈ box.stage1 ∧ v.stage2 ∈ box.stage2 ∧ v.stage3 ∈ box.stage3 ∧
    v.stage4 ∈ box.stage4 ∧ v.output ∈ box.output

def mixedTerm (c : CoefficientBox) (saturation left right previous : IntervalRat) : IntervalRat :=
  (((curvature c.gain c.inverseNormalization saturation).mul left).mul right).add
    ((slope c.gain c.inverseNormalization saturation).mul previous)

def mixedStageRange (c : CoefficientBox)
    (saturation left right previous old : IntervalRat) : IntervalRat :=
  (c.alpha.mul (mixedTerm c saturation left right previous)).add (c.beta.mul old)

def mixedRanges (c : CoefficientBox) (saturation left right old : Box)
    (input : IntervalRat) : VariationBox :=
  let w1 := firstRange c input old
  let w2 := mixedStageRange c (saturation 0) (left 0) (right 0) w1 (old 1)
  let w3 := mixedStageRange c (saturation 1) (left 1) (right 1) w2 (old 2)
  let w4 := mixedStageRange c (saturation 2) (left 2) (right 2) w3 (old 3)
  ⟨w1, w2, w3, w4, (c.feedback.add (IntervalRat.singleton 1)).mul
    (mixedTerm c (saturation 3) (left 3) (right 3) w4)⟩

noncomputable section

/-- All terms, including the output saturator, are enclosed using checked
tanh boxes. This theorem applies to arbitrary first and mixed directions. -/
theorem mixedRanges_sound (s next : NaiveReuppingLadder ℝ)
    (c : CoefficientBox) (hc : c.Valid s) (state saturation left right old : Box)
    (input : IntervalRat) (depth : ℕ)
    (hs : ∀ i, audioHistory next i ∈ state i)
    (ht : ∀ i, IntervalTanh.check ((state i).mul c.gain) (saturation i) depth = true)
    (v w : Variation) (hv : ∀ i, v.history i ∈ left i)
    (hw : ∀ i, w.history i ∈ right i)
    (mixed : Fin 4 → ℝ) (hm : ∀ i, mixed i ∈ old i)
    (du : ℝ) (hu : du ∈ input) :
    (mixedRanges c saturation left right old input).Mem (mixedVariation s next v w mixed du) := by
  let result := mixedVariation s next v w mixed du
  have hterm (i : Fin 4) (previous : ℝ) (box : IntervalRat) (hp : previous ∈ box) :
      NormalizedTanh.curvature s.saturation (audioHistory next i)*v.history i*w.history i+
        NormalizedTanh.slope s.saturation (audioHistory next i)*previous ∈
          mixedTerm c (saturation i) (left i) (right i) box := by
    exact IntervalRat.mem_add (IntervalRat.mem_mul (IntervalRat.mem_mul
      (mem_curvature s.saturation c.gain c.inverseNormalization (state i) (saturation i) depth
        hc.2.2.2.1 hc.2.2.2.2 (ht i) _ (hs i)) (hv i)) (hw i))
      (IntervalRat.mem_mul (mem_slope s.saturation c.gain c.inverseNormalization (state i)
        (saturation i) depth hc.2.2.2.1 hc.2.2.2.2 (ht i) _ (hs i)) hp)
  have h1 : result.stage1 ∈ (mixedRanges c saturation left right old input).stage1 :=
    IntervalRat.mem_add
      (IntervalRat.mem_mul hc.1 (IntervalRat.mem_sub hu (IntervalRat.mem_mul hc.2.2.1 (hm 3))))
      (IntervalRat.mem_mul hc.2.1 (hm 0))
  have h2 : result.stage2 ∈ (mixedRanges c saturation left right old input).stage2 :=
    IntervalRat.mem_add (IntervalRat.mem_mul hc.1 (hterm 0 _ _ h1)) (IntervalRat.mem_mul hc.2.1 (hm 1))
  have h3 : result.stage3 ∈ (mixedRanges c saturation left right old input).stage3 :=
    IntervalRat.mem_add (IntervalRat.mem_mul hc.1 (hterm 1 _ _ h2)) (IntervalRat.mem_mul hc.2.1 (hm 2))
  have h4 : result.stage4 ∈ (mixedRanges c saturation left right old input).stage4 :=
    IntervalRat.mem_add (IntervalRat.mem_mul hc.1 (hterm 2 _ _ h3)) (IntervalRat.mem_mul hc.2.1 (hm 3))
  have ho : result.output ∈ (mixedRanges c saturation left right old input).output :=
    IntervalRat.mem_mul (IntervalRat.mem_add hc.2.2.1 (by simpa using IntervalRat.mem_singleton 1)) (hterm 3 _ _ h4)
  exact ⟨h1,h2,h3,h4,ho⟩

end
end DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure

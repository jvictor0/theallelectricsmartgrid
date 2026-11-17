import DSP.Filters.Tuning.Peaks

noncomputable section
namespace DSP
open ResponseFrequency

namespace TPTLadder

def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop :=
  Tuning.ValidCutoff spec.cutoff ∧ 0 ≤ spec.feedback ∧ spec.feedback < 4

def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ :=
  let g := Tuning.warp spec.cutoff
  ⟨g,g,g,g,spec.feedback⟩

/-- Connect the peak polynomial to the actual optimized ladder kernel. -/
theorem normalized (g k x : ℝ) (f : ResponseFrequency ℝ)
    (hc : g*f.cosHalf ≠ 0) (hx : f.sinHalf = g*f.cosHalf*x) :
    (⟨g,g,g,g,k⟩ : Snapshot ℝ).magnitudeSquared f = 1 / Tuning.ladderDen k (x^2) := by
  unfold Snapshot.magnitudeSquared
  rw [Response.ratio_eq _ _ (mul_self_nonneg _)]
  simp only [Response.normSquared, fma_eq, hx]
  calc
    _ = ((g*f.cosHalf)^8 * 1) / ((g*f.cosHalf)^8*Tuning.ladderDen k (x^2)) := by
      unfold Tuning.ladderDen
      congr 1 <;> ring
    _ = _ := mul_div_mul_left _ _ (pow_ne_zero 8 hc)

theorem cutoff_gain (spec : LogicalSpec ℝ) (h : spec.Valid) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample spec.cutoff) = 1/(4-spec.feedback)^2 := by
  rw [LogicalSpec.snapshot, normalized _ _ 1 _
    (ne_of_gt (mul_pos (Tuning.warp_pos _ h.1) (Tuning.cos_pos _ h.1))) (by
      simp [Tuning.warp_mul_cos _ h.1])]
  simpa using Tuning.ladder_cutoff_power spec.feedback
end TPTLadder

namespace MorphingTPTSVF

def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop :=
  spec.svf.Valid ∧ 0 ≤ spec.blend ∧ spec.blend ≤ 1

def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ :=
  let w := blendWeights spec.blend
  ⟨spec.svf.snapshot,w.low,w.band,w.high⟩

theorem computeCoefficients_weights (spec : LogicalSpec ℝ) (state : MorphingTPTSVF ℝ)
    (h : spec.Valid) : stateWeights (computeCoefficients spec state) = blendWeights spec.blend := by
  have hc : clamped spec.blend = spec.blend := by
    simp [clamped, max_eq_right h.2.1, min_eq_right h.2.2]
  simp only [stateWeights, upperHalf, computeCoefficients, blendWeights, phase, upperHalfOf, hc,
    IfLess.ifLess, cos2pi_real, sin2pi_real, LawfulIfLess.less]
  simp only [show (0.5 : ℝ) = 1/2 by norm_num]
  simp [div_eq_mul_inv]

theorem cutoff_gain (spec : LogicalSpec ℝ) (h : spec.Valid) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample spec.svf.cutoff) = spec.svf.q^2 := by
  have hp := blendWeights_power spec.blend
  have hz : (blendWeights spec.blend).low * (blendWeights spec.blend).high = 0 := by
    unfold blendWeights weights
    split <;> simp
  change spec.svf.snapshot.weightedMagnitudeSquared _ _ _ _ = _
  rw [TPTSVF.weighted_cutoff spec.svf h.1]
  have hw : ((blendWeights spec.blend).low-(blendWeights spec.blend).high)^2 +
      (blendWeights spec.blend).band^2 = 1 := by nlinarith
  simp only [LogicalSpec.snapshot, hw, one_mul]
end MorphingTPTSVF

namespace OnePoleZOHLowPass

def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ :=
  ⟨1-Real.exp (-(2*Real.pi*spec.cutoff))⟩

/-- ZOH cutoff specifies the matched analog pole rate. It is not a digital
-3dB claim: the exact digital gain is the expression on the right. -/
theorem cutoff_gain (spec : LogicalSpec ℝ) (h : spec.Valid) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample spec.cutoff) =
      spec.snapshot.alpha^2 / (spec.snapshot.alpha^2 +
        4*(1-spec.snapshot.alpha)*Real.sin (Real.pi*spec.cutoff)^2) := by
  have ha := computeCoefficients_bounds spec ⟨0,0⟩ h
  change 0 < spec.snapshot.alpha ∧ spec.snapshot.alpha < 1 at ha
  rw [Snapshot.magnitudeSquared_eq_normSq _ _ (le_of_lt ha.1), Snapshot.response_normSq]

theorem peak_dc (spec : LogicalSpec ℝ) (h : spec.Valid) (f : ℝ) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample f) ≤ 1 := by
  have ha := computeCoefficients_bounds spec ⟨0,0⟩ h
  change 0 < spec.snapshot.alpha ∧ spec.snapshot.alpha < 1 at ha
  rw [Snapshot.magnitudeSquared_eq_normSq _ _ (le_of_lt ha.1), Snapshot.response_normSq]
  apply (div_le_one (magnitude_denominator_pos _ _ ha.1 (le_of_lt ha.2))).mpr
  have hp : 0 ≤ 4*(1-spec.snapshot.alpha)*Real.sin (Real.pi*f)^2 := by
    apply mul_nonneg _ (sq_nonneg _)
    linarith [ha.2]
  linarith
end OnePoleZOHLowPass
end DSP

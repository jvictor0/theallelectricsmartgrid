import DSP.Filters.Tuning.History

noncomputable section
namespace DSP
open ResponseFrequency

namespace Tuning

theorem sin_pos (f : ℝ) (h : ValidCutoff f) : 0 < Real.sin (Real.pi*f) := by
  rw [← warp_mul_cos f h]
  exact mul_pos (warp_pos f h) (cos_pos f h)

/-- Normalized squared-frequency denominator of a second-order section.
Here t=(tan(pi*f)/tan(pi*cutoff))^2. -/
def svfDen (k t : ℝ) : ℝ := (1-t)^2+k^2*t

theorem svfDen_pos (k t : ℝ) (hk : 0 < k) (ht : 0 ≤ t) : 0 < svfDen k t := by
  by_cases hz : t = 0
  · simp [svfDen, hz]
  · have ht' : 0 < t := lt_of_le_of_ne ht (Ne.symm hz)
    exact add_pos_of_nonneg_of_pos (sq_nonneg _) (mul_pos (sq_pos_of_pos hk) ht')

end Tuning

namespace OnePoleTPTLowPass

def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ := ⟨Tuning.warp spec.cutoff⟩

theorem cutoff_half_power (spec : LogicalSpec ℝ) (h : spec.Valid) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample spec.cutoff) = 1/2 := by
  simp only [LogicalSpec.snapshot, Snapshot.magnitudeSquared,
    ofCyclesPerSample_cosHalf, ofCyclesPerSample_sinHalf, Tuning.warp_mul_cos _ h]
  rw [Response.ratio_eq _ _ (mul_self_nonneg _)]
  simp only [Response.normSquared, fma_eq]
  have hs := ne_of_gt (Tuning.sin_pos spec.cutoff h)
  field_simp
  ring
end OnePoleTPTLowPass

namespace OnePoleTPTHighPass

def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop := Tuning.ValidCutoff spec.cutoff
def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ := ⟨Tuning.warp spec.cutoff⟩

theorem cutoff_half_power (spec : LogicalSpec ℝ) (h : spec.Valid) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample spec.cutoff) = 1/2 := by
  simp only [LogicalSpec.snapshot, Snapshot.magnitudeSquared,
    ofCyclesPerSample_cosHalf, ofCyclesPerSample_sinHalf, Tuning.warp_mul_cos _ h]
  rw [Response.ratio_eq _ _ (mul_self_nonneg _)]
  simp only [Response.normSquared, fma_eq]
  have hs := ne_of_gt (Tuning.sin_pos spec.cutoff h)
  field_simp
  ring
end OnePoleTPTHighPass

namespace TPTSVF

def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ :=
  ⟨Tuning.warp spec.cutoff, 1/spec.q⟩

/-- The optimized kernel reduces to the standard normalized response whenever
sinHalf=g*cosHalf*x. This includes every interior digital frequency. -/
theorem Snapshot.normalized (s : Snapshot ℝ) (f : ResponseFrequency ℝ) (x l b h : ℝ)
    (hc : s.g*f.cosHalf ≠ 0) (hx : f.sinHalf = s.g*f.cosHalf*x) :
    s.weightedMagnitudeSquared l b h f =
      ((l-h*x^2)^2+(b*x)^2) / Tuning.svfDen s.k (x^2) := by
  unfold Snapshot.weightedMagnitudeSquared
  rw [Response.ratio_eq _ _ (by
    simp only [Response.normSquared, fma_eq]
    exact add_nonneg (mul_self_nonneg _) (mul_self_nonneg _))]
  simp only [Response.normSquared, fma_eq, hx]
  calc
    _ = ((s.g*f.cosHalf)^4*((l-h*x^2)^2+(b*x)^2)) /
        ((s.g*f.cosHalf)^4*Tuning.svfDen s.k (x^2)) := by
      unfold Tuning.svfDen
      congr 1 <;> ring
    _ = _ := mul_div_mul_left _ _ (pow_ne_zero 4 hc)

theorem weighted_cutoff (spec : LogicalSpec ℝ) (h : spec.Valid) (l b hi : ℝ) :
    spec.snapshot.weightedMagnitudeSquared l b hi (ofCyclesPerSample spec.cutoff) =
      ((l-hi)^2+b^2)*spec.q^2 := by
  have hg := Tuning.warp_pos spec.cutoff h.1
  have hc := Tuning.cos_pos spec.cutoff h.1
  rw [Snapshot.normalized _ _ 1 l b hi (ne_of_gt (mul_pos hg hc)) (by
    simp [LogicalSpec.snapshot, Tuning.warp_mul_cos _ h.1])]
  simp only [Tuning.svfDen, LogicalSpec.snapshot, one_pow, mul_one, sub_self, zero_pow,
    ne_eq, OfNat.ofNat_ne_zero, not_false_eq_true, zero_add]
  field_simp

theorem cutoff_gains (spec : LogicalSpec ℝ) (h : spec.Valid) :
    spec.snapshot.lowPassMagnitudeSquared (ofCyclesPerSample spec.cutoff) = spec.q^2 ∧
    spec.snapshot.bandPassMagnitudeSquared (ofCyclesPerSample spec.cutoff) = spec.q^2 ∧
    spec.snapshot.highPassMagnitudeSquared (ofCyclesPerSample spec.cutoff) = spec.q^2 ∧
    spec.snapshot.notchMagnitudeSquared (ofCyclesPerSample spec.cutoff) = 0 := by
  simp [Snapshot.lowPassMagnitudeSquared, Snapshot.bandPassMagnitudeSquared,
    Snapshot.highPassMagnitudeSquared, Snapshot.notchMagnitudeSquared, weighted_cutoff spec h]
end TPTSVF

namespace TPTBaseWidth

def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop := spec.highPass.Valid ∧ spec.lowPass.Valid
def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ :=
  ⟨spec.highPass.snapshot, spec.lowPass.snapshot⟩

/-- Each edge is a section cutoff, not an overall -3dB point. -/
theorem cutoff_edges (spec : LogicalSpec ℝ) (h : spec.Valid) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample spec.highPass.cutoff) =
      (1/2)*spec.lowPass.snapshot.magnitudeSquared (ofCyclesPerSample spec.highPass.cutoff) ∧
    spec.snapshot.magnitudeSquared (ofCyclesPerSample spec.lowPass.cutoff) =
      spec.highPass.snapshot.magnitudeSquared (ofCyclesPerSample spec.lowPass.cutoff)*(1/2) := by
  simp [LogicalSpec.snapshot, Snapshot.magnitudeSquared,
    OnePoleTPTHighPass.cutoff_half_power _ h.1, OnePoleTPTLowPass.cutoff_half_power _ h.2]
end TPTBaseWidth

namespace TPTSVF4Pole

def LogicalSpec.Valid (spec : LogicalSpec ℝ) : Prop := spec.stage1.Valid ∧ spec.stage2.Valid
def LogicalSpec.snapshot (spec : LogicalSpec ℝ) : Snapshot ℝ :=
  ⟨spec.stage1.snapshot, spec.stage2.snapshot⟩

theorem equal_cutoff_gains (stage : TPTSVF.LogicalSpec ℝ) (h : stage.Valid) :
    (LogicalSpec.snapshot ⟨stage,stage⟩).lowPassMagnitudeSquared (ofCyclesPerSample stage.cutoff) = stage.q^4 ∧
    (LogicalSpec.snapshot ⟨stage,stage⟩).bandPassMagnitudeSquared (ofCyclesPerSample stage.cutoff) = stage.q^4 ∧
    (LogicalSpec.snapshot ⟨stage,stage⟩).highPassMagnitudeSquared (ofCyclesPerSample stage.cutoff) = stage.q^4 := by
  obtain ⟨hl,hb,hh,_⟩ := TPTSVF.cutoff_gains stage h
  simp only [LogicalSpec.snapshot, Snapshot.lowPassMagnitudeSquared,
    Snapshot.bandPassMagnitudeSquared, Snapshot.highPassMagnitudeSquared, hl, hb, hh]
  constructor
  · ring
  constructor <;> ring
end TPTSVF4Pole

end DSP

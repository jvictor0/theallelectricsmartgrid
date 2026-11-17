import DSP.Filters.Tuning.Stability

noncomputable section
namespace DSP
open ResponseFrequency

namespace Tuning

theorem frequency_coordinates (cutoff f : ℝ) (hc : ValidCutoff cutoff) (hf : ValidCutoff f) :
    Real.sin (Real.pi*f) = warp cutoff * Real.cos (Real.pi*f) * (warp f / warp cutoff) := by
  rw [← warp_mul_cos f hf]
  have hg := ne_of_gt (warp_pos cutoff hc)
  field_simp

theorem frequencyAtRatio_below_cutoff (cutoff t : ℝ) (hc : ValidCutoff cutoff)
    (ht : 0 < t) (ht1 : t < 1) :
    0 < frequencyAtRatio cutoff t ∧ frequencyAtRatio cutoff t < cutoff := by
  have hg := warp_pos cutoff hc
  have hs : Real.sqrt t < 1 := by simpa using Real.sqrt_lt_sqrt (le_of_lt ht) ht1
  have hinv : Real.arctan (warp cutoff) = Real.pi*cutoff := by
    apply Real.arctan_tan <;> nlinarith [hc.1, hc.2, Real.pi_pos]
  constructor
  · unfold frequencyAtRatio
    exact div_pos (Real.arctan_pos.mpr (mul_pos hg (Real.sqrt_pos.mpr ht))) Real.pi_pos
  · unfold frequencyAtRatio
    apply (div_lt_iff₀ Real.pi_pos).mpr
    have hlt := Real.arctan_strictMono (mul_lt_mul_of_pos_left hs hg)
    simpa only [mul_one, hinv, mul_comm cutoff Real.pi] using hlt

/-- HP's global resonant peak; at t=0 its numerator vanishes. -/
theorem svf_high_peak (k t : ℝ) (hk : 0 < k) (hk2 : k^2 < 2) (ht : 0 ≤ t) :
    t^2/svfDen k t ≤ 1/(k^2*(1-k^2/4)) := by
  by_cases hz : t = 0
  · subst t
    simp only [zero_pow, OfNat.ofNat_ne_zero, ne_eq, not_false_eq_true, zero_div]
    have hd : 0 < k^2*(1-k^2/4) := mul_pos (sq_pos_of_pos hk) (by linarith)
    exact le_of_lt (one_div_pos.mpr hd)
  rw [svf_high_reciprocal k t hz, ← svf_peak_power]
  exact svf_low_peak k (1/t) hk hk2 (le_of_lt (one_div_pos.mpr (lt_of_le_of_ne ht (Ne.symm hz))))

/-- An undamped/resonant section is not claimed to peak at cutoff: only the
band-pass does. HP without a resonance peak is bounded by its Nyquist gain. -/
theorem svf_high_peak_nyquist (k t : ℝ) (hk : 0 < k) (hk2 : 2 ≤ k^2) (ht : 0 ≤ t) :
    t^2/svfDen k t ≤ 1 := by
  apply (div_le_one (svfDen_pos k t hk ht)).mpr
  have hp : 0 ≤ (k^2-2)*t := mul_nonneg (by linarith) ht
  unfold svfDen
  nlinarith
end Tuning

namespace TPTSVF
/-- Bridge from physical digital frequency to the normalized peak theorems. -/
theorem digital_normalized (spec : LogicalSpec ℝ) (h : spec.Valid) (f l b hi : ℝ)
    (hf : Tuning.ValidCutoff f) :
    spec.snapshot.weightedMagnitudeSquared l b hi (ofCyclesPerSample f) =
      ((l-hi*(Tuning.warp f/Tuning.warp spec.cutoff)^2)^2 +
        (b*(Tuning.warp f/Tuning.warp spec.cutoff))^2) /
      Tuning.svfDen (1/spec.q) ((Tuning.warp f/Tuning.warp spec.cutoff)^2) :=
  Snapshot.normalized _ _ _ _ _ _
    (ne_of_gt (mul_pos (Tuning.warp_pos _ h.1) (Tuning.cos_pos _ hf)))
    (Tuning.frequency_coordinates _ _ h.1 hf)
end TPTSVF

namespace TPTLadder
theorem digital_normalized (spec : LogicalSpec ℝ) (h : spec.Valid) (f : ℝ)
    (hf : Tuning.ValidCutoff f) :
    spec.snapshot.magnitudeSquared (ofCyclesPerSample f) =
      1/Tuning.ladderDen spec.feedback ((Tuning.warp f/Tuning.warp spec.cutoff)^2) :=
  normalized _ _ _ _ (ne_of_gt (mul_pos (Tuning.warp_pos _ h.1) (Tuning.cos_pos _ hf)))
    (Tuning.frequency_coordinates _ _ h.1 hf)
end TPTLadder

end DSP

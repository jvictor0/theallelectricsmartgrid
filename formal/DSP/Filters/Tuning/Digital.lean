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

namespace LinkwitzRileyCrossover
/-- Two matched Butterworth-2 sections per branch sum to an all-pass.
The complex sum has unit magnitude; its phase is not asserted to be zero. -/
theorem butterworth_sum_allpass (g k s c : ℝ) (hg : 0 < g) (hk : k^2 = 2)
    (hsc : s^2+c^2 = 1) :
    Complex.normSq (
      Response.svf (g:ℂ) k 1 0 0 ⟨0,s⟩ c * Response.svf (g:ℂ) k 1 0 0 ⟨0,s⟩ c +
      Response.svf (g:ℂ) k 0 0 1 ⟨0,s⟩ c * Response.svf (g:ℂ) k 0 0 1 ⟨0,s⟩ c) = 1 := by
  let n := (g*c)^4+s^4
  let d : ℂ := ⟨(g*c)^2-s^2,k*g*c*s⟩
  have hn : 0 < n := by
    by_cases hs : s = 0
    · have hc : c ≠ 0 := by intro hz; simp [hs,hz] at hsc
      have hp : 0 < ((g*c)^2)^2 := sq_pos_of_pos (sq_pos_of_ne_zero (mul_ne_zero (ne_of_gt hg) hc))
      dsimp [n]
      nlinarith [sq_nonneg (s^2)]
    · have hp : 0 < (s^2)^2 := sq_pos_of_pos (sq_pos_of_ne_zero hs)
      dsimp [n]
      nlinarith [sq_nonneg ((g*c)^2)]
  have hd : Complex.normSq d = n := by
    rw [Complex.normSq_apply]
    change ((g*c)^2-s^2)*((g*c)^2-s^2) + (k*g*c*s)*(k*g*c*s) = (g*c)^4+s^4
    calc
      _ = (g*c)^4+s^4+(k^2-2)*(g*c*s)^2 := by ring
      _ = _ := by rw [hk]; ring
  have hl : Response.svf (g:ℂ) k 1 0 0 ⟨0,s⟩ c = ((g*c)^2 : ℝ)/d := by
    unfold Response.svf
    congr 1 <;> apply Complex.ext <;> norm_num [d, pow_succ]
    all_goals ring_nf
    all_goals simp
  have hh : Response.svf (g:ℂ) k 0 0 1 ⟨0,s⟩ c = (-(s^2) : ℝ)/d := by
    unfold Response.svf
    congr 1 <;> apply Complex.ext <;> norm_num [d, pow_succ]
    all_goals ring_nf
    all_goals simp
  rw [hl,hh,div_mul_div_comm,div_mul_div_comm,← add_div]
  have hnum : (((g*c)^2 : ℝ):ℂ)*((g*c)^2 : ℝ) + ((-(s^2) : ℝ):ℂ)*(-(s^2) : ℝ) = (n:ℂ) := by
    apply Complex.ext
    · simp [n, pow_succ]
      ring
    · simp [n, pow_succ]
  rw [hnum, Complex.normSq_div, Complex.normSq_mul, hd, Complex.normSq_ofReal]
  exact div_self (ne_of_gt (mul_pos hn hn))

theorem sum_allpass (spec : LogicalSpec ℝ) (h : spec.Valid) (f : ℝ) :
    Complex.normSq (spec.snapshot.lowPassResponse f + spec.snapshot.highPassResponse f) = 1 := by
  have hq := Tuning.butterworth_q_squared
  have hk : (1/(Trig.cos2pi (1/8) : ℝ))^2 = 2 := by
    rw [div_pow, one_pow, hq]
    norm_num
  exact butterworth_sum_allpass _ _ _ _ (Tuning.warp_pos _ h) hk (Real.sin_sq_add_cos_sq _)
end LinkwitzRileyCrossover
end DSP

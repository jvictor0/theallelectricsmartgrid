import DSP.Filters.Tuning.Response
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Arctan
import Mathlib.Topology.Order.IntermediateValue

noncomputable section
namespace DSP.Tuning

/-- Convert a squared warped-frequency ratio into digital cycles/sample. -/
def frequencyAtRatio (cutoff t : ℝ) : ℝ := Real.arctan (warp cutoff * Real.sqrt t) / Real.pi

theorem frequencyAtRatio_warp (cutoff t : ℝ) (ht : 0 ≤ t) (hc : ValidCutoff cutoff) :
    (warp (frequencyAtRatio cutoff t) / warp cutoff)^2 = t := by
  have hg := ne_of_gt (warp_pos cutoff hc)
  simp only [warp] at hg
  simp only [warp, frequencyAtRatio]
  rw [mul_div_cancel₀ _ (ne_of_gt Real.pi_pos), Real.tan_arctan,
    mul_div_cancel_left₀ _ hg, Real.sq_sqrt ht]

/-- The LP peak is at t=1-k^2/2 when this is positive. The difference
of denominators is a square, so this proves a global, not merely local, peak. -/
theorem svf_low_peak (k t : ℝ) (hk : 0 < k) (hk2 : k^2 < 2) (_ht : 0 ≤ t) :
    1 / svfDen k t ≤ 1 / svfDen k (1-k^2/2) := by
  have hr : 0 ≤ 1-k^2/2 := by linarith
  apply one_div_le_one_div_of_le (svfDen_pos k _ hk hr)
  have hid : svfDen k t - svfDen k (1-k^2/2) = (t-(1-k^2/2))^2 := by unfold svfDen; ring
  nlinarith [sq_nonneg (t-(1-k^2/2))]

/-- Below the resonance threshold Q=1/sqrt(2), LP has its maximum at DC. -/
theorem svf_low_peak_dc (k t : ℝ) (hk2 : 2 ≤ k^2) (ht : 0 ≤ t) :
    1 / svfDen k t ≤ 1 := by
  have he : 0 ≤ (k^2-2)*t := mul_nonneg (by linarith) ht
  have hd : 1 ≤ svfDen k t := by unfold svfDen; nlinarith [sq_nonneg t]
  exact (div_le_one (by linarith : 0 < svfDen k t)).mpr hd

/-- BP always peaks at the natural frequency, with power Q^2. -/
theorem svf_band_peak (k t : ℝ) (hk : 0 < k) (ht : 0 ≤ t) :
    t / svfDen k t ≤ 1 / k^2 := by
  apply (div_le_div_iff₀ (svfDen_pos k t hk ht) (sq_pos_of_pos hk)).mpr
  unfold svfDen
  nlinarith [sq_nonneg (1-t)]

/-- The HP resonant peak is the reciprocal warped frequency of the LP peak. -/
theorem svf_high_reciprocal (k t : ℝ) (ht : t ≠ 0) :
    t^2 / svfDen k t = 1 / svfDen k (1/t) := by
  unfold svfDen
  field_simp
  ring

/-- Exact peak power of the resonant LP (and, by reciprocal frequency, HP). -/
theorem svf_peak_power (k : ℝ) :
    1 / svfDen k (1-k^2/2) = 1 / (k^2*(1-k^2/4)) := by
  congr 1
  unfold svfDen
  ring

/-- Positive cascades preserve a common peak. This is reused for two identical
SVFs without differentiating their fourth-order response. -/
theorem cascade_peak (a aMax b bMax : ℝ) (ha : 0 ≤ b) (hb : 0 ≤ aMax)
    (h1 : a ≤ aMax) (h2 : b ≤ bMax) : a*b ≤ aMax*bMax :=
  mul_le_mul h1 h2 ha hb

/-- Base-width HP(g1) followed by LP(g2), in squared warped frequency t.
The peak is t=g1*g2, rather than either section's own cutoff. -/
theorem baseWidth_peak (g1 g2 t : ℝ) (h1 : 0 < g1) (h2 : 0 < g2) (ht : 0 ≤ t) :
    g2^2*t / ((t+g1^2)*(t+g2^2)) ≤ g2^2/(g1+g2)^2 := by
  have hd : 0 < (t+g1^2)*(t+g2^2) := by positivity
  apply (div_le_div_iff₀ hd (sq_pos_of_pos (add_pos h1 h2))).mpr
  have hdiff : g2^2*((t+g1^2)*(t+g2^2)) - (g2^2*t)*(g1+g2)^2 =
      g2^2*(t-g1*g2)^2 := by ring
  nlinarith [mul_nonneg (sq_nonneg g2) (sq_nonneg (t-g1*g2))]

/-- Equal-stage ladder's normalized squared denominator. -/
def ladderDen (k t : ℝ) : ℝ := (1+t)^4 + 2*k*(1-6*t+t^2)+k^2

def ladderStationary (k t : ℝ) : ℝ := (1+t)^3+k*(t-3)

theorem ladderDen_squares (k t : ℝ) :
    ladderDen k t = (1+k-6*t+t^2)^2 + 16*t*(1-t)^2 := by
  unfold ladderDen
  ring

theorem ladderDen_pos (k t : ℝ) (hk : 0 ≤ k) (hk4 : k < 4) (ht : 0 ≤ t) :
    0 < ladderDen k t := by
  rw [ladderDen_squares]
  by_cases hz : t = 0
  · subst t
    simp only [mul_zero, zero_pow, OfNat.ofNat_ne_zero, ne_eq, not_false_eq_true,
      sub_zero, add_zero, zero_mul]
    positivity
  by_cases h1 : t = 1
  · subst t
    have hn : 1+k-6*1+(1:ℝ)^2 ≠ 0 := by linarith
    nlinarith [sq_pos_of_ne_zero hn]
  have hp : 0 < 16*t*(1-t)^2 := by
    apply mul_pos (mul_pos (by norm_num) (lt_of_le_of_ne ht (Ne.symm hz)))
    exact sq_pos_of_ne_zero (sub_ne_zero.mpr (Ne.symm h1))
  nlinarith [sq_nonneg (1+k-6*t+t^2)]

theorem ladder_cutoff_power (k : ℝ) : 1 / ladderDen k 1 = 1/(4-k)^2 := by
  congr 1
  unfold ladderDen
  ring

theorem ladder_no_feedback_cutoff : 1 / ladderDen 0 1 = 1/16 := by norm_num [ladderDen]

/-- The stationary equation has one solution strictly below natural frequency
when 1/3 < k < 4. At k=4 that solution reaches t=1. -/
theorem ladder_peak_exists (k : ℝ) (hk : 1/3 < k) (hk4 : k < 4) :
    ∃ r, 0 < r ∧ r < 1 ∧ ladderStationary k r = 0 := by
  have h0 : ladderStationary k 0 < 0 := by unfold ladderStationary; nlinarith
  have h1 : 0 < ladderStationary k 1 := by unfold ladderStationary; nlinarith
  have hc : ContinuousOn (ladderStationary k) (Set.Icc (0:ℝ) 1) := by
    unfold ladderStationary
    fun_prop
  obtain ⟨r, hr, he⟩ := intermediate_value_Icc (by norm_num : (0:ℝ) ≤ 1) hc
    ⟨le_of_lt h0, le_of_lt h1⟩
  refine ⟨r, ?_, ?_, he⟩
  · exact lt_of_le_of_ne hr.1 (by intro hz; subst r; linarith)
  · exact lt_of_le_of_ne hr.2 (by intro hz; subst r; linarith)

/-- A root of the stationary equation is the unique global maximum of the
ladder response on t>=0. The proof factors the denominator difference. -/
theorem ladder_peak (k r t : ℝ) (hk : 0 ≤ k) (hk4 : k < 4)
    (hr : 0 ≤ r) (ht : 0 ≤ t) (he : ladderStationary k r = 0) :
    1/ladderDen k t ≤ 1/ladderDen k r := by
  have hid : ladderDen k t - ladderDen k r =
      (t-r)^2*(t^2+2*t*r+3*r^2+4*t+8*r+6+2*k) + 4*ladderStationary k r*(t-r) := by
    unfold ladderDen ladderStationary
    ring
  have hp : 0 ≤ (t-r)^2*(t^2+2*t*r+3*r^2+4*t+8*r+6+2*k) := by positivity
  rw [he] at hid
  apply one_div_le_one_div_of_le (ladderDen_pos k r hk hk4 hr)
  nlinarith

theorem ladder_peak_unique (k r t : ℝ) (hk : 0 ≤ k) (hr : 0 ≤ r) (ht : 0 ≤ t)
    (he : ladderStationary k r = 0) (heq : ladderDen k t = ladderDen k r) : t = r := by
  have hid : ladderDen k t - ladderDen k r =
      (t-r)^2*(t^2+2*t*r+3*r^2+4*t+8*r+6+2*k) + 4*ladderStationary k r*(t-r) := by
    unfold ladderDen ladderStationary
    ring
  have hp : 0 < t^2+2*t*r+3*r^2+4*t+8*r+6+2*k := by positivity
  rw [he, heq] at hid
  have hz : (t-r)^2 = 0 := (mul_eq_zero.mp (by nlinarith [hid])).resolve_right (ne_of_gt hp)
  nlinarith [sq_nonneg (t-r)]

theorem ladder_peak_dc (k t : ℝ) (hk : 0 ≤ k) (hk3 : k ≤ 1/3) (ht : 0 ≤ t) :
    1/ladderDen k t ≤ 1/ladderDen k 0 := by
  have hid : ladderDen k t - ladderDen k 0 =
      t^4+4*t^3+(6+2*k)*t^2+4*(1-3*k)*t := by unfold ladderDen; ring
  have hp : 0 ≤ t^4+4*t^3+(6+2*k)*t^2+4*(1-3*k)*t := by
    have : 0 ≤ 1-3*k := by linarith
    positivity
  apply one_div_le_one_div_of_le (ladderDen_pos k 0 hk (by linarith) (by norm_num))
  linarith

end DSP.Tuning

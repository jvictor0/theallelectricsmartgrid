import DSP.Filters.TunedDecayComb.Stability
import DSP.Filters.Tuning.Proofs
import Mathlib.Analysis.SpecialFunctions.Complex.Arg
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Arctan
import Mathlib.Algebra.Order.Floor.Semiring

noncomputable section
namespace DSP.TunedDecayComb

/-- Physical inputs to the handwritten coefficient tuner, after UI clamping.
The logarithmic duration and damping parameters retain their base-two units. -/
structure LogicalSpec where
  frequency : ℝ
  feedbackSign : ℝ
  log2DecaySeconds : ℝ
  dampOctaves : ℝ
  sampleRate : ℝ

namespace LogicalSpec

def cycles (p : LogicalSpec) : ℝ := if p.feedbackSign < 0 then 1 / 2 else 1
def duration (p : LogicalSpec) : ℝ := Real.exp (Real.log 2 * p.log2DecaySeconds)
def cutoff (p : LogicalSpec) : ℝ := min (49 / 100) (p.frequency * Real.exp (Real.log 2 * p.dampOctaves))
def t (p : LogicalSpec) : ℝ := Tuning.warp p.frequency
def ratio (p : LogicalSpec) : ℝ := Tuning.warp p.cutoff / p.t
def shape (p : LogicalSpec) : ℝ := Real.sqrt (1 + (p.ratio ^ 2 - 1) ^ 2)
def u (p : LogicalSpec) : ℝ := 1 / p.shape
def g (p : LogicalSpec) : ℝ := p.t * Real.sqrt p.shape
def k (p : LogicalSpec) : ℝ := Real.sqrt (2 * (1 - p.u))
def gain (p : LogicalSpec) : ℝ := Real.sqrt ((1 - p.u) * (1 + p.u))
def phase (p : LogicalSpec) : ℝ :=
  if p.feedbackSign = 0 then 0 else -Complex.arg ⟨1 - p.u, p.k * Real.sqrt p.u⟩
def delay (p : LogicalSpec) : ℝ := (2 * Real.pi * p.cycles + p.phase) / (2 * Real.pi * p.frequency)
def samples (p : LogicalSpec) : ℕ := max 1 (⌊p.delay⌋₊ - 1)
def fraction (p : LogicalSpec) : ℝ := p.delay - p.samples
def td (p : LogicalSpec) : ℝ := Real.tan (Real.pi * p.frequency * p.fraction)
def a (p : LogicalSpec) : ℝ := (p.t - p.td) / (p.t + p.td)
def radius (p : LogicalSpec) : ℝ := Real.exp (-Real.log 1000 / (p.sampleRate * p.duration))

def Valid (capacity : ℕ) (p : LogicalSpec) : Prop :=
  3 ≤ capacity ∧ 0 < p.frequency ∧ p.frequency ≤ 1 / 4 ∧
  p.cycles / p.frequency ≤ (capacity - 1 : ℕ) ∧ 1 ≤ p.dampOctaves ∧
  0 < p.sampleRate ∧ (p.feedbackSign = -1 ∨ p.feedbackSign = 0 ∨ p.feedbackSign = 1)

theorem cutoff_bounds {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    p.frequency < p.cutoff ∧ p.cutoff < 1 / 2 := by
  have hlog : 0 < Real.log 2 := Real.log_pos (by norm_num)
  have he : (2 : ℝ) ≤ Real.exp (Real.log 2 * p.dampOctaves) := by
    calc
      2 = Real.exp (Real.log 2) := (Real.exp_log (by norm_num : (0 : ℝ) < 2)).symm
      _ ≤ _ := Real.exp_le_exp.mpr (by nlinarith [hp.2.2.2.2.1])
  constructor
  · apply lt_min
    · linarith [hp.2.2.1]
    · have hm := mul_le_mul_of_nonneg_left he hp.2.1.le
      nlinarith [hp.2.1]
  · exact (min_le_left _ _).trans_lt (by norm_num)

theorem t_pos {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) : 0 < p.t :=
  Tuning.warp_pos _ ⟨hp.2.1, by linarith [hp.2.2.1]⟩

theorem ratio_gt_one {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) : 1 < p.ratio := by
  obtain ⟨hfc, hc⟩ := p.cutoff_bounds hp
  have ht := p.t_pos hp
  apply (one_lt_div ht).mpr
  apply Real.strictMonoOn_tan
  · constructor <;> nlinarith [Real.pi_pos, hp.2.1, hp.2.2.1]
  · constructor <;> nlinarith [Real.pi_pos, hp.2.1, hfc, hc]
  · exact mul_lt_mul_of_pos_left hfc Real.pi_pos

theorem shape_gt_one {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) : 1 < p.shape := by
  have hv := p.ratio_gt_one hp
  have hsq := Real.sq_sqrt (show 0 ≤ 1 + (p.ratio ^ 2 - 1) ^ 2 by positivity)
  have hn := Real.sqrt_nonneg (1 + (p.ratio ^ 2 - 1) ^ 2)
  change 1 < Real.sqrt _
  nlinarith [sq_pos_of_pos (show 0 < p.ratio ^ 2 - 1 by nlinarith)]

theorem u_bounds {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) : 0 < p.u ∧ p.u < 1 := by
  have h := p.shape_gt_one hp
  exact ⟨one_div_pos.mpr (by linarith), (div_lt_one (by linarith)).mpr h⟩

theorem lowpass_properties {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    0 < p.g ∧ 0 < p.k ∧ 0 < p.gain ∧
    p.k ^ 2 = 2 * (1 - p.u) ∧ p.gain ^ 2 = 1 - p.u ^ 2 ∧ p.g ^ 2 * p.u = p.t ^ 2 := by
  obtain ⟨hu, hu1⟩ := p.u_bounds hp
  have hs := p.shape_gt_one hp
  have ht := p.t_pos hp
  have hk : 0 < 2 * (1 - p.u) := by linarith
  have hv : 0 < (1 - p.u) * (1 + p.u) := mul_pos (by linarith) (by linarith)
  refine ⟨mul_pos ht (Real.sqrt_pos.mpr (by linarith)), Real.sqrt_pos.mpr hk,
    Real.sqrt_pos.mpr hv, Real.sq_sqrt hk.le, ?_, ?_⟩
  · rw [gain, Real.sq_sqrt hv.le]
    ring
  · rw [g, mul_pow, Real.sq_sqrt (by linarith : 0 ≤ p.shape), u]
    field_simp

theorem phase_bounds {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    -(Real.pi / 2) < p.phase ∧ p.phase ≤ 0 := by
  unfold phase
  split_ifs
  · constructor <;> linarith [Real.pi_pos]
  · obtain ⟨hu, hu1⟩ := p.u_bounds hp
    have hx : 0 < (⟨1 - p.u, p.k * Real.sqrt p.u⟩ : ℂ).re := by dsimp; linarith
    have hy : 0 ≤ (⟨1 - p.u, p.k * Real.sqrt p.u⟩ : ℂ).im := by
      exact mul_nonneg (Real.sqrt_nonneg _) (Real.sqrt_nonneg _)
    have hlt := Complex.arg_lt_pi_div_two_iff.mpr (Or.inl hx)
    have hnon := Complex.arg_nonneg_iff.mpr hy
    constructor <;> linarith

theorem delay_bounds {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    1 < p.delay ∧ p.delay ≤ (capacity - 1 : ℕ) := by
  obtain ⟨hphase, hphase0⟩ := p.phase_bounds hp
  have hf := hp.2.1
  have hfmax := hp.2.2.1
  have hcycles : (1 : ℝ) / 2 ≤ p.cycles := by unfold cycles; split_ifs <;> norm_num
  have hden : 0 < 2 * Real.pi * p.frequency := by positivity
  constructor
  · apply (one_lt_div hden).mpr
    have hm := mul_le_mul_of_nonneg_left hcycles (by positivity : 0 ≤ 2 * Real.pi)
    nlinarith [Real.pi_pos]
  · have he : p.delay ≤ p.cycles / p.frequency := by
      unfold delay
      apply (div_le_iff₀ hden).mpr
      have he : p.cycles / p.frequency * (2 * Real.pi * p.frequency) = 2 * Real.pi * p.cycles := by
        field_simp
      rw [he]
      linarith
    exact he.trans hp.2.2.2.1

theorem samples_fraction_bounds {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    0 < p.samples ∧ p.samples < capacity ∧ 0 < p.fraction ∧ p.fraction < 2 := by
  obtain ⟨hd, hdc⟩ := p.delay_bounds hp
  have hf : 0 < ⌊p.delay⌋₊ := Nat.floor_pos.mpr hd.le
  have hfle := Nat.floor_le (by linarith : 0 ≤ p.delay)
  have hflt := Nat.lt_floor_add_one p.delay
  have hfc := Nat.floor_le_of_le hdc
  have hsub : ((⌊p.delay⌋₊ - 1 : ℕ) : ℝ) = (⌊p.delay⌋₊ : ℝ) - 1 := by
    rw [Nat.cast_sub (by omega), Nat.cast_one]
  have hn : p.samples = 1 ∨ p.samples = ⌊p.delay⌋₊ - 1 :=
    (max_cases 1 (⌊p.delay⌋₊ - 1)).imp And.left And.left
  have hpN : 0 < p.samples := lt_of_lt_of_le (by omega : 0 < 1) (le_max_left _ _)
  refine ⟨hpN, ?_, ?_, ?_⟩
  · rcases hn with h | h <;> rw [h] <;> omega
  · unfold fraction
    rcases hn with h | h
    · simpa [h] using sub_pos.mpr hd
    · rw [h, hsub]
      linarith
  · unfold fraction
    have hmax : ⌊p.delay⌋₊ - 1 ≤ p.samples := le_max_right _ _
    have hcast : ((⌊p.delay⌋₊ - 1 : ℕ) : ℝ) ≤ p.samples := by exact_mod_cast hmax
    rw [hsub] at hcast
    linarith

theorem allpass_bounds {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) : |p.a| < 1 := by
  have ht := p.t_pos hp
  obtain ⟨_, _, hd, hd2⟩ := p.samples_fraction_bounds hp
  have htd : 0 < p.td := by
    apply Real.tan_pos_of_pos_of_lt_pi_div_two
    · exact mul_pos (mul_pos Real.pi_pos hp.2.1) hd
    · have hprod : p.frequency * p.fraction < 1 / 2 := by
        nlinarith [hp.2.1, hp.2.2.1]
      nlinarith [Real.pi_pos]
  have hden : 0 < p.t + p.td := by linarith
  rw [a, abs_lt]
  constructor
  · apply (lt_div_iff₀ hden).mpr
    linarith
  · apply (div_lt_iff₀ hden).mpr
    linarith

theorem radius_bounds {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    0 < p.radius ∧ p.radius < 1 := by
  refine ⟨Real.exp_pos _, Real.exp_lt_one_iff.mpr ?_⟩
  exact div_neg_of_neg_of_pos (neg_neg_of_pos (Real.log_pos (by norm_num)))
    (mul_pos hp.2.2.2.2.2.1 (Real.exp_pos _))

def prepare {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) : Coefficients capacity ℝ :=
  let age := DelayBuffer.ofDelay p.samples (p.samples_fraction_bounds hp).1
    (p.samples_fraction_bounds hp).2.1.le
  ⟨age, p.a, Real.sqrt (max 0 (1 - p.a ^ 2)), p.radius, p.radius ^ p.samples,
    p.g, p.k, 1 / (1 + p.k * p.g + p.g ^ 2), p.gain, p.feedbackSign, p.feedbackSign == 0⟩

theorem prepare_properties {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    Prepared (p.prepare hp) ∧ PeakNormalized (p.prepare hp) ∧
    |(p.prepare hp).a| < 1 ∧ 0 < (p.prepare hp).r ∧ (p.prepare hp).r < 1 ∧
    (p.prepare hp).delayGain = (p.prepare hp).r ^ ((p.prepare hp).age.val + 1) := by
  have ha := p.allpass_bounds hp
  have ha2 := (sq_lt_one_iff_abs_lt_one p.a).mpr ha
  obtain ⟨hg, hk, hv, hk2, hv2, _⟩ := p.lowpass_properties hp
  have hD : 1 + p.k * p.g + p.g ^ 2 ≠ 0 := by positivity
  have hn : p.samples - 1 + 1 = p.samples := by have := (p.samples_fraction_bounds hp).1; omega
  refine ⟨⟨?_, ?_⟩, ⟨hg, hk, hv, p.u, hk2, hv2⟩, ha, (p.radius_bounds hp).1,
    (p.radius_bounds hp).2, ?_⟩
  · change p.a ^ 2 + Real.sqrt (max 0 (1 - p.a ^ 2)) ^ 2 = 1
    rw [max_eq_right (by linarith), Real.sq_sqrt (by linarith)]
    ring
  · exact one_div_mul_cancel hD
  · simp [prepare, DelayBuffer.ofDelay, hn]

theorem prepared_pole_bound {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) (z : ℂ)
    (hz : closedDenominator ((p.prepare hp).map Complex.ofReal) z⁻¹ = 0) :
    ‖z‖ ≤ p.radius := by
  obtain ⟨_, hc, ha, hr0, _, hr⟩ := p.prepare_properties hp
  apply pole_radius_bound _ z hc ha ?_ hr0.le hr hz
  change |p.feedbackSign| ≤ 1
  rcases hp.2.2.2.2.2.2 with h | h | h <;> rw [h] <;> norm_num

/-- T60 is an amplitude ratio of 1/1000, not an energy ratio. -/
theorem radius_t60 {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) :
    p.radius ^ (p.sampleRate * p.duration) = (1 : ℝ) / 1000 := by
  have htime : p.sampleRate * p.duration ≠ 0 :=
    ne_of_gt (mul_pos hp.2.2.2.2.2.1 (Real.exp_pos _))
  rw [radius, ← Real.exp_mul, div_mul_cancel₀ _ htime, Real.exp_neg,
    Real.exp_log (by norm_num : (0 : ℝ) < 1000)]
  norm_num

/-- Each stationary pole's modal amplitude obeys the requested T60 bound.
This is not a monotone bound on arbitrary sums of modes or switching transients. -/
theorem pole_t60 {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) (z : ℂ)
    (hz : closedDenominator ((p.prepare hp).map Complex.ofReal) z⁻¹ = 0) :
    ‖z‖ ^ (p.sampleRate * p.duration) ≤ (1 : ℝ) / 1000 := by
  rw [← p.radius_t60 hp]
  exact Real.rpow_le_rpow (norm_nonneg _) (p.prepared_pole_bound hp z hz)
    (mul_nonneg hp.2.2.2.2.2.1.le (Real.exp_pos _).le)

theorem pole_samples_after_t60 {capacity : ℕ} (p : LogicalSpec) (hp : p.Valid capacity) (z : ℂ)
    (hz : closedDenominator ((p.prepare hp).map Complex.ofReal) z⁻¹ = 0)
    (n : ℕ) (hn : p.sampleRate * p.duration ≤ n) : ‖z‖ ^ n ≤ (1 : ℝ) / 1000 := by
  have hr := p.radius_bounds hp
  calc
    ‖z‖ ^ n ≤ p.radius ^ n := pow_le_pow_left₀ (norm_nonneg _) (p.prepared_pole_bound hp z hz) n
    _ = p.radius ^ (n : ℝ) := (Real.rpow_natCast _ _).symm
    _ ≤ p.radius ^ (p.sampleRate * p.duration) := Real.rpow_le_rpow_of_exponent_ge hr.1 hr.2.le hn
    _ = 1 / 1000 := p.radius_t60 hp

end LogicalSpec
end DSP.TunedDecayComb

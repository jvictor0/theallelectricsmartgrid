import DSP.Filters.NaiveReuppingLadder.NoiseResponse.LagMarginal
import DSP.Filters.NaiveReuppingLadder.NoiseResponse.BlockCorrection

open MeasureTheory Set Finset Filter
open scoped ENNReal Topology
noncomputable section

namespace NoiseDisplay.SurveyAccuracy

variable {Ξ : Type*} [MeasurableSpace Ξ]

def block (Y : ℕ → Ξ → ℝ) (x : Ξ) : ℝ := (Y 4 x + Y 5 x + Y 6 x + Y 7 x) / 4

def clippedMean (ν : Measure Ξ) (Y : ℕ → Ξ → ℝ) (W : ℝ) : ℝ :=
  ∫ x, min (block Y x) (2 * W) ∂ν

/-- A finite-age second moment controls the mean power removed by clipping.
    The pointwise quadratic touches the positive excess at three times W. -/
theorem clipping_bias (ν : Measure Ξ) [IsFiniteMeasure ν] (Y : ℕ → Ξ → ℝ)
    (hY : ∀ j, Integrable (Y j) ν) {W : ℝ} (hw : 0 < W)
    (hZ : Integrable (fun x => (block Y x-W)^2) ν)
    (moment : (∫ x, (block Y x-W)^2 ∂ν) ≤ 0.12*W^2) :
    (∫ x, block Y x ∂ν) - clippedMean ν Y W ∈ Icc 0 (0.03*W) := by
  let y := block Y
  have hb : Integrable y ν := (((((hY 4).add (hY 5)).add (hY 6)).add (hY 7)).div_const 4)
  have hc : Integrable (fun x => min (y x) (2*W)) ν := hb.inf (integrable_const _)
  let e := fun x => y x - min (y x) (2*W)
  have he : Integrable e ν := hb.sub hc
  have nonneg : 0 ≤ ∫ x, e x ∂ν := integral_nonneg fun x => sub_nonneg.mpr (min_le_left _ _)
  have major (x : Ξ) : 4*W*e x ≤ (y x-W)^2 := by
    dsimp only [e]
    by_cases hy : y x ≤ 2*W
    · rw [min_eq_left hy, sub_self, mul_zero]; exact sq_nonneg _
    · rw [min_eq_right (le_of_not_ge hy)]; nlinarith [sq_nonneg (y x-3*W)]
  have total := (integral_mono ((he.const_mul (4*W))) hZ major).trans moment
  rw [integral_const_mul] at total
  change (∫ x, y x ∂ν) - (∫ x, min (y x) (2*W) ∂ν) ∈ _
  rw [← integral_sub hb hc]
  change (∫ x, e x ∂ν) ∈ _
  exact ⟨nonneg, by nlinarith [mul_pos hw (show (0:ℝ)<4 by norm_num)]⟩

/-- A passing calibration of the 16-record survey and a wet-power certificate can
    falsely certify 95% population coverage at 0.5 dB only on the specified bad
    events. The wet power is split into a bounded measured part and a signed
    residual. The drawing test is part of acceptance on the observed data. -/
theorem false_accept {A B Ω C S R : Type*} [MeasurableSpace A] [MeasurableSpace Ω] [PseudoMetricSpace C]
    (μ : Measure Ω) [IsProbabilityMeasure μ] (probes : Measure A) [IsProbabilityMeasure probes]
    (probe : A → C)
    (ref : Ω → C) (wet physical : C) (dry : dist wet physical ≤ 0.000216)
    (pass : Set Ω) (badCal badClip badTail : Set Ω)
    (pCal : μ badCal ≤ ENNReal.ofReal 0.025) (pClip : μ badClip ≤ ENNReal.ofReal 0.01)
    (pTail : μ badTail ≤ ENNReal.ofReal 0.015)
    (W l u : Ω → B → ℝ) (ν : Measure Ξ) [IsFiniteMeasure ν]
    (next : S → R → S) (reward : B → S → R → ℝ) (initial : Ξ → S)
    (input : ℕ → Ξ → R) (T : Ξ → Ξ) (hT : MeasurePreserving T ν ν)
    (shift : ∀ k x, input k (T x) = input (k+1) x) (keepInitial : ∀ x, initial (T x) = initial x)
    (actual : B → ℝ) :
    let Y := fun b => LagMarginal.recordPower next (reward b) initial input
    let Δ := fun b j => ∫ x, LagMarginal.observedDifference next (reward b) initial input j x ∂ν
    (∀ b n, Integrable (Y b n) ν) →
    (∀ b, Tendsto (fun n : ℕ => (n⁻¹ : ℝ) * ∑ j ∈ range n, ∫ x, Y b j x ∂ν)
      atTop (𝓝 (actual b))) →
    (∀ ω ∈ pass, ω ∉ badCal → ENNReal.ofReal 0.95 ≤ probes {a | dist (probe a) (ref ω) ≤ 0.153}) →
    (∀ ω ∉ badClip, ∀ b, clippedMean ν (Y b) (W ω b) ∈ Icc (l ω b) (u ω b)) →
    (∀ ω b, 0 < W ω b) →
    (∀ ω b, Integrable (fun x => (block (Y b) x - W ω b)^2) ν) →
    (∀ ω ∉ badTail, ∀ b, (∫ x, (block (Y b) x - W ω b)^2 ∂ν) ≤ 0.12*(W ω b)^2) →
    (∀ ω ∉ badTail, ∀ b (N : ℕ), 7 ≤ N →
      |Δ b 4/4 + Δ b 5/2 + 3*Δ b 6/4 + ∑ j ∈ range (N-7), Δ b (j+7)| ≤ 0.03*W ω b) →
    (∀ ω ∈ pass, (∀ b, actual b ∈
      Icc (l ω b - 0.03*W ω b) (u ω b + 0.06*W ω b)) → dist (ref ω) wet ≤ 0.346784) →
    μ {ω | ω ∈ pass ∧ probes {a | dist (probe a) physical ≤ 0.5} < ENNReal.ofReal 0.95} ≤ ENNReal.ofReal 0.05 := by
  intro Y Δ hY target cal cap positive integrableSquare moment history render
  have incl : {ω | ω ∈ pass ∧ probes {a | dist (probe a) physical ≤ 0.5} < ENNReal.ofReal 0.95} ⊆
      badCal ∪ badClip ∪ badTail := by
    rintro ω ⟨ok, fail⟩
    by_contra! h
    simp only [mem_union, not_or] at h
    have box : dist (ref ω) wet ≤ 0.346784 := render ω ok (fun b => by
      let m := fun n => ∫ x, Y b n x ∂ν
      have law (j : ℕ) : Δ b j = m (j+1) - m j := by
        exact LagMarginal.expected_difference_of_shift next (reward b) initial input T hT shift
          keepInitial (hY b) j
      have age := BlockCorrection.finite_horizon_bound m (Δ b) law (target b)
        (show 0 ≤ 0.03*W ω b by exact mul_nonneg (by norm_num) (positive ω b).le) (history ω h.2 b)
      have base : (m 4 + m 5 + m 6 + m 7)/4 = ∫ x, block (Y b) x ∂ν := by
        unfold block
        rw [integral_div]
        change _ = (∫ x, (Y b 4 + Y b 5 + Y b 6 + Y b 7) x ∂ν) / 4
        rw [integral_add' (((hY b 4).add (hY b 5)).add (hY b 6)) (hY b 7),
          integral_add' ((hY b 4).add (hY b 5)) (hY b 6), integral_add' (hY b 4) (hY b 5)]
      rw [base] at age
      obtain ⟨alo,ahi⟩ := abs_le.mp age
      obtain ⟨clo,chi⟩ := clipping_bias ν (Y b) (hY b) (positive ω b)
        (integrableSquare ω b) (moment ω h.2 b)
      obtain ⟨lo,hi⟩ := cap ω h.1.2 b
      exact ⟨by linarith [le_of_lt (positive ω b)],by linarith⟩)
    have total : dist (ref ω) physical ≤ 0.347 := by linarith [dist_triangle (ref ω) wet physical]
    have covered : {a | dist (probe a) (ref ω) ≤ 0.153} ⊆ {a | dist (probe a) physical ≤ 0.5} := by
      intro a ha
      change dist (probe a) (ref ω) ≤ 0.153 at ha
      change dist (probe a) physical ≤ 0.5
      linarith [dist_triangle (probe a) (ref ω) physical]
    exact (not_lt_of_ge ((cal ω ok h.1.1).trans (measure_mono covered))) fail
  calc
    _ ≤ _ := measure_mono incl
    _ ≤ μ (badCal ∪ badClip) + μ badTail := measure_union_le _ _
    _ ≤ (μ badCal + μ badClip) + μ badTail := add_le_add (measure_union_le _ _) le_rfl
    _ ≤ (ENNReal.ofReal 0.025 + ENNReal.ofReal 0.01) + ENNReal.ofReal 0.015 := add_le_add (add_le_add pCal pClip) pTail
    _ = ENNReal.ofReal 0.05 := by rw [← ENNReal.ofReal_add (by norm_num) (by norm_num), ← ENNReal.ofReal_add (by norm_num) (by norm_num)]; norm_num

end NoiseDisplay.SurveyAccuracy

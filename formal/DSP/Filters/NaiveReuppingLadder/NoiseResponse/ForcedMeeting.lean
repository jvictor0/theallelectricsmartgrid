import DSP.Filters.NaiveReuppingLadder.NoiseResponse.FiniteLookback

noncomputable section
namespace NoiseDisplay.ForcedMeeting
open Finset Filter MarkovPower RunAverage FiniteLookback
open scoped Topology

variable {S A : Type*} [Fintype A] (next : S → A → S) (w : A → ℝ)

/-- One letter can itself be a block of independent records. A successful block
    sends every reachable incoming state to a common state, possibly different
    for each successful block. The range of any expected bounded readout shrinks
    geometrically, including on trajectories that have not reached equilibrium. -/
theorem expected_spread (hw : ∀ a, 0 ≤ w a) (good : A → Bool)
    (reach : S → Prop) (closed : ∀ i, reach i → ∀ a, 0 < w a → reach (next i a))
    (forget : ∀ a, good a = true → ∀ i j, reach i → reach j → next i a = next j a)
    (r : S → ℝ) (D : ℝ) (diam : ∀ i j, reach i → reach j → |r i-r j| ≤ D)
    (n : ℕ) (i j : S) (hi : reach i) (hj : reach j) :
    |atTime (recordStep next w) n r i-atTime (recordStep next w) n r j| ≤
      D*(∑ a ∈ univ.filter (fun a => good a = false), w a)^n := by
  let q := ∑ a ∈ univ.filter (fun a => good a = false), w a
  change _ ≤ D*q^n
  induction n generalizing i j with
  | zero => simpa [atTime] using diam i j hi hj
  | succ n ih =>
    let v := atTime (recordStep next w) n r
    change |(∑ a, w a*v (next i a)) - ∑ a, w a*v (next j a)| ≤ _
    rw [← sum_sub_distrib]
    simp_rw [← mul_sub]
    calc
      _ ≤ ∑ a, |w a*(v (next i a)-v (next j a))| := abs_sum_le_sum_abs _ _
      _ ≤ ∑ a, (if good a = false then w a else 0)*(D*q^n) := by
        apply sum_le_sum
        intro a _
        rcases (hw a).eq_or_lt with hz | hp
        · simp [← hz]
        cases h : good a
        · simp only [if_true,abs_mul,abs_of_nonneg (hw a)]
          exact mul_le_mul_of_nonneg_left (ih _ _ (closed _ hi a hp) (closed _ hj a hp)) (hw a)
        · simp [forget a h i j hi hj]
      _ = D*q^(n+1) := by rw [← sum_mul,← sum_filter]; change q*(D*q^n) = _; ring

/-- The same survival bound controls finite-age bias directly. The center is
    the reset mean after n blocks. The target is its original Cesàro limit. -/
theorem finite_bias [Fintype S] (hw : ∀ a, 0 ≤ w a) (one : ∑ a, w a = 1)
    (good : A → Bool) (reach : S → Prop)
    (closed : ∀ i, reach i → ∀ a, 0 < w a → reach (next i a))
    (forget : ∀ a, good a = true → ∀ i j, reach i → reach j → next i a = next j a)
    (r : S → ℝ) (D : ℝ) (diam : ∀ i j, reach i → reach j → |r i-r j| ≤ D)
    (reset : S) (hr : reach reset) :
    ∃ m, Tendsto (fun n : ℕ => (∑ k ∈ range n, atTime (recordStep next w) k r reset)/(n:ℝ))
      atTop (𝓝 m) ∧ ∀ n : ℕ, |m-atTime (recordStep next w) n r reset| ≤
        D*(∑ a ∈ univ.filter (fun a => good a = false), w a)^n := by
  let err := fun n : ℕ => D*(∑ a ∈ univ.filter (fun a => good a = false), w a)^n
  let center := fun n => atTime (recordStep next w) n r reset
  have h n := cesaro_expected_sandwich next w hw one r n (center n-err n) (center n+err n)
    reach closed reset hr (fun i hi => by
      have b := abs_le.mp (expected_spread next w hw good reach closed forget r D diam n i reset hi hr)
      change center n-err n ≤ _ ∧ _ ≤ center n+err n
      dsimp [center,err] at *
      constructor <;> linarith [b.1,b.2])
  obtain ⟨m, hm, _⟩ := h 0
  refine ⟨m,hm,fun n => ?_⟩
  obtain ⟨m', h',lo,hi⟩ := h n
  have same := tendsto_nhds_unique hm h'
  subst m'
  change |m-center n| ≤ err n
  exact abs_le.mpr ⟨by linarith,by linarith⟩

end NoiseDisplay.ForcedMeeting

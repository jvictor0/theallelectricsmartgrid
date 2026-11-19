import DSP.Filters.TunedDecayComb.Proofs

noncomputable section
namespace DSP.TunedDecayComb

open Bilinear
variable {capacity : ℕ} {K : Type} [Field K]

def branchSeries (s : ℕ → Branch K) : Branch (Stream K) :=
  ⟨ser (fun n => (s n).allpass), ser (fun n => (s n).ic1),
    ser (fun n => (s n).ic2), ser (fun n => (s n).y)⟩

theorem ser_mul_right (u : ℕ → K) (c : K) :
    ser (fun n => u n * c) = ser u * const c := by
  simpa only [mul_comm] using ser_mul c u

theorem ser_zero : ser (fun _ : ℕ => (0 : K)) = 0 := by
  simp [ser, show PowerSeries.mk (fun _ : ℕ => (0 : K)) = 0 from rfl]

theorem branch_series (c : Coefficients capacity K) (s : ℕ → Branch K) (u d : ℕ → K) :
    branchSeries (fun n => branchStep c (s n) (u n) (d n)) =
      branchStep (c.map const) (branchSeries s) (ser u) (ser d) := by
  have htwo : const (2 : K) = 2 := by
    rw [const, map_ofNat, map_ofNat]
  cases hb : c.bypass <;>
    simp [branchSeries, branchStep, Coefficients.map, NormalizedAllPass.step, hb,
      ser_add, ser_sub, ser_mul, ser_mul_right, ser_zero, htwo]

theorem prepared_series (c : Coefficients capacity K) (hc : Prepared c) :
    Prepared (c.map const) := by
  obtain ⟨ha, hh⟩ := hc
  constructor
  · simpa [Coefficients.map, const] using congrArg const ha
  · simpa [Coefficients.map, const] using congrArg const hh

theorem regular_series (c : Coefficients capacity K) (hc : Prepared c) :
    Regular (c.map const) delay := by
  constructor
  · have h := embed_ne_zero
      (1 + PowerSeries.C c.a * PowerSeries.C c.r * PowerSeries.X) (by simp)
    simpa [Coefficients.map, const, delay] using h
  · intro _
    have hD : 1 + c.k * c.g + c.g ^ 2 ≠ 0 := by
      intro h
      have := hc.2
      rw [h, mul_zero] at this
      exact zero_ne_one this
    have h := embed_ne_zero
      ((1 + PowerSeries.C c.k * PowerSeries.C c.g + PowerSeries.C c.g ^ 2) +
        2 * (PowerSeries.C c.g ^ 2 - 1) * (PowerSeries.C c.r * PowerSeries.X) +
        (1 - PowerSeries.C c.k * PowerSeries.C c.g + PowerSeries.C c.g ^ 2) *
          (PowerSeries.C c.r * PowerSeries.X) ^ 2) (by simpa using hD)
    simpa [lowpassDenominator, Coefficients.map, const, delay, map_ofNat] using h

def Branch.AtRest (s : Branch K) : Prop := s.allpass = 0 ∧ s.ic1 = 0 ∧ s.ic2 = 0

/-- Exact arbitrary-input branch law. The same mode algebra applies to formal
series because zero initial memory supplies the unit-delay memory relations. -/
theorem branch_trajectory_series (c : Coefficients capacity K) (hc : Prepared c)
    (s : ℕ → Branch K) (u d : ℕ → K) (h0 : (s 0).AtRest)
    (hs : ∀ n, s (n + 1) = branchStep c (s n) (u n) (d n)) :
    ser (fun n => (s (n + 1)).y) = lowpassResponse (c.map const) delay *
      (ser u + const c.sign * NormalizedAllPass.response (const c.a) (const c.r) delay *
        const c.delayGain * ser d) := by
  have he : branchSeries (fun n => s (n + 1)) =
      branchStep (c.map const) (branchSeries s) (ser u) (ser d) := by
    simp_rw [hs]
    exact branch_series c s u d
  have hm (f : Branch K → K) (hf : f (s 0) = 0) :
      ser (fun n => f (s n)) = delay * ser (fun n => f (s (n + 1))) := by
    have h := ser_shift (fun n => f (s n))
    simpa only [hf, const_zero, zero_add] using h
  have ha := hm (·.allpass) h0.1
  have hi := hm (·.ic1) h0.2.1
  have hj := hm (·.ic2) h0.2.2
  have hp := prepared_series c hc
  have hr := regular_series c hc
  have h := branch_mode (c.map const) (branchSeries s) (ser u) (ser d) delay
    hp.1 hp.2 hr.1 hr.2 (by rw [← he]; exact ha)
    (by rw [← he]; exact hi) (by rw [← he]; exact hj)
  rw [← he] at h
  exact h

def State.AtRest (s : State capacity K) : Prop :=
  s.buffer.history = (fun _ => 0) ∧ s.first.AtRest ∧ s.second.AtRest

/-- Both branch streams return through one shared output history. This cleared
equation holds for every input stream, not only a geometric test signal. -/
theorem mixed_trajectory_series (c₀ c₁ : Coefficients capacity K) (m : K)
    (hc₀ : Prepared c₀) (hc₁ : Prepared c₁) (s : ℕ → State capacity K) (u : ℕ → K)
    (h0 : (s 0).AtRest) (hs : ∀ n, s (n + 1) = step c₀ c₁ (some m) (s n) ⟨u n⟩) :
    (1 - ((1 - const m) * returnResponse (c₀.map const) delay +
      const m * returnResponse (c₁.map const) delay)) * ser (fun n => (s (n + 1)).y) =
    ((1 - const m) * lowpassResponse (c₀.map const) delay +
      const m * lowpassResponse (c₁.map const) delay) * ser u := by
  have h₀ := branch_trajectory_series c₀ hc₀ (fun n => (s n).first) u
    (fun n => (s n).buffer.read c₀.age) h0.2.1
    (fun n => by rw [hs n]; rfl)
  have h₁ := branch_trajectory_series c₁ hc₁ (fun n => (s n).second) u
    (fun n => (s n).buffer.read c₁.age) h0.2.2
    (fun n => by rw [hs n]; rfl)
  have hd₀ := read_series (fun _ => c₀) (fun _ => c₁) (fun _ => some m) s u h0.1 hs c₀.age
  have hd₁ := read_series (fun _ => c₀) (fun _ => c₁) (fun _ => some m) s u h0.1 hs c₁.age
  rw [hd₀] at h₀
  rw [hd₁] at h₁
  have hy : ser (fun n => (s (n + 1)).y) =
      (1 - const m) * ser (fun n => (s (n + 1)).first.y) +
      const m * ser (fun n => (s (n + 1)).second.y) := by
    have he : ∀ n, (s (n + 1)).y = (1 - m) * (s (n + 1)).first.y +
        m * (s (n + 1)).second.y := by
      intro n
      rw [hs n]
      simp only [step]
      ring
    rw [ser_congr he, ser_add, ser_mul, ser_mul]
    simp [const]
  unfold returnResponse
  dsimp only [Coefficients.map] at *
  linear_combination hy + (1 - const m) * h₀ + const m * h₁

theorem settled_trajectory_series (c₀ c₁ : Coefficients capacity K) (hc : Prepared c₀)
    (s : ℕ → State capacity K) (u : ℕ → K) (h0 : (s 0).AtRest)
    (hs : ∀ n, s (n + 1) = step c₀ c₁ none (s n) ⟨u n⟩) :
    (1 - returnResponse (c₀.map const) delay) * ser (fun n => (s (n + 1)).y) =
      lowpassResponse (c₀.map const) delay * ser u := by
  have h := branch_trajectory_series c₀ hc (fun n => (s n).first) u
    (fun n => (s n).buffer.read c₀.age) h0.2.1 (fun n => by rw [hs n]; rfl)
  have hd := read_series (fun _ => c₀) (fun _ => c₁) (fun _ => none) s u h0.1 hs c₀.age
  have hy : (fun n => (s (n + 1)).first.y) = (fun n => (s (n + 1)).y) := by
    funext n
    rw [hs n]
    rfl
  rw [hd, hy] at h
  unfold returnResponse
  dsimp only [Coefficients.map] at *
  linear_combination h

private theorem embed_inverse (p : PowerSeries K) (hp : PowerSeries.constantCoeff p ≠ 0) :
    embed p⁻¹ = (embed p)⁻¹ := by
  apply mul_right_cancel₀ (embed_ne_zero p hp)
  rw [inv_mul_cancel₀ (embed_ne_zero p hp), ← map_mul, PowerSeries.inv_mul_cancel p hp,
    map_one]

private def lowpassPowerDenominator (c : Coefficients capacity K) : PowerSeries K :=
  (1 + PowerSeries.C c.k * PowerSeries.C c.g + PowerSeries.C c.g ^ 2) +
    2 * (PowerSeries.C c.g ^ 2 - 1) * (PowerSeries.C c.r * PowerSeries.X) +
    (1 - PowerSeries.C c.k * PowerSeries.C c.g + PowerSeries.C c.g ^ 2) *
      (PowerSeries.C c.r * PowerSeries.X) ^ 2

private def returnPowerSeries (c : Coefficients capacity K) : PowerSeries K :=
  (if c.bypass then 1 else PowerSeries.C c.gain * PowerSeries.C c.g ^ 2 *
    (1 + PowerSeries.C c.r * PowerSeries.X) ^ 2 * (lowpassPowerDenominator c)⁻¹) *
  PowerSeries.C c.sign * ((PowerSeries.C c.a + PowerSeries.C c.r * PowerSeries.X) *
    (1 + PowerSeries.C c.a * PowerSeries.C c.r * PowerSeries.X)⁻¹) *
  PowerSeries.C c.delayGain * PowerSeries.X ^ (c.age.val + 1)

private theorem returnPowerSeries_embed (c : Coefficients capacity K) (hc : Prepared c) :
    embed (returnPowerSeries c) = returnResponse (c.map const) delay := by
  have hD : PowerSeries.constantCoeff (lowpassPowerDenominator c) ≠ 0 := by
    simp only [lowpassPowerDenominator, map_add, map_mul, map_sub, map_pow,
      PowerSeries.constantCoeff_X, PowerSeries.constantCoeff_C, map_one]
    norm_num
    intro h
    have := hc.2
    rw [h, mul_zero] at this
    exact zero_ne_one this
  have hL := embed_inverse (lowpassPowerDenominator c) hD
  unfold lowpassPowerDenominator at hL
  cases hb : c.bypass <;>
    simp [returnPowerSeries, returnResponse, lowpassResponse, hb, Coefficients.map,
      embed_inverse _ (by simp : PowerSeries.constantCoeff
        (1 + PowerSeries.C c.a * PowerSeries.C c.r * PowerSeries.X) ≠ 0),
      hL, lowpassPowerDenominator, lowpassDenominator,
      NormalizedAllPass.response, const, delay, div_eq_mul_inv, map_ofNat]

theorem mixed_stream_denominator_ne_zero (c₀ c₁ : Coefficients capacity K) (m : K)
    (hc₀ : Prepared c₀) (hc₁ : Prepared c₁) :
    1 - ((1 - const m) * returnResponse (c₀.map const) delay +
      const m * returnResponse (c₁.map const) delay) ≠ (0 : Stream K) := by
  have hz (c : Coefficients capacity K) : PowerSeries.constantCoeff (returnPowerSeries c) = 0 := by
    unfold returnPowerSeries
    simp only [map_mul, map_pow, PowerSeries.constantCoeff_X, zero_pow (by omega : c.age.val + 1 ≠ 0),
      mul_zero]
  have h := embed_ne_zero
    (1 - ((1 - PowerSeries.C m) * returnPowerSeries c₀ + PowerSeries.C m * returnPowerSeries c₁))
    (by simp only [map_sub, map_add, map_mul, map_one, hz, mul_zero, add_zero, sub_zero]; exact one_ne_zero)
  simpa only [map_sub, map_add, map_mul, map_one, returnPowerSeries_embed c₀ hc₀,
    returnPowerSeries_embed c₁ hc₁, const] using h

theorem settled_stream_denominator_ne_zero (c : Coefficients capacity K) (hc : Prepared c) :
    1 - returnResponse (c.map const) delay ≠ (0 : Stream K) := by
  simpa using mixed_stream_denominator_ne_zero c c 0 hc hc

theorem realizes_mixed (c₀ c₁ : Coefficients capacity K) (m : K)
    (hc₀ : Prepared c₀) (hc₁ : Prepared c₁) (s : State capacity K) (h0 : s.AtRest) :
    Realizes (step c₀ c₁ (some m)) s (·.y)
      (mixedResponse (c₀.map const) (c₁.map const) (const m) delay) := by
  intro u
  have h := mixed_trajectory_series c₀ c₁ m hc₀ hc₁
    (Step.states (step c₀ c₁ (some m)) (fun n => ⟨u n⟩) s) u h0 (fun _ => rfl)
  unfold mixedResponse
  rw [div_mul_eq_mul_div, eq_div_iff (mixed_stream_denominator_ne_zero c₀ c₁ m hc₀ hc₁)]
  exact (mul_comm _ _).trans h

theorem realizes_settled (c₀ c₁ : Coefficients capacity K) (hc : Prepared c₀)
    (s : State capacity K) (h0 : s.AtRest) :
    Realizes (step c₀ c₁ none) s (·.y) (settledResponse (c₀.map const) delay) := by
  intro u
  have h := settled_trajectory_series c₀ c₁ hc
    (Step.states (step c₀ c₁ none) (fun n => ⟨u n⟩) s) u h0 (fun _ => rfl)
  unfold settledResponse
  rw [div_mul_eq_mul_div, eq_div_iff (settled_stream_denominator_ne_zero c₀ hc)]
  exact (mul_comm _ _).trans h

end DSP.TunedDecayComb

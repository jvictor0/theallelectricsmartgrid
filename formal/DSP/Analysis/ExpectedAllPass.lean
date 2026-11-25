import DSP.Analysis.ModulatedMode
import DSP.Filters.SchroederAllPass.Geometric
import DSP.WanderingRandomLFO.Characteristic
import Mathlib.Analysis.SpecificLimits.Normed

namespace DSP.ExpectedAllPass

open MeasureTheory Filter WanderingRandomLFO
open scoped BigOperators Topology

noncomputable section

private def phasor (angle : ℝ) : ℂ := Complex.exp ((↑(-angle) : ℂ) * Complex.I)

private theorem norm_phasor (angle : ℝ) : ‖phasor angle‖ = 1 := by
  simp [phasor, Complex.norm_exp, Complex.mul_re]

private theorem phasor_add (a b : ℝ) : phasor (a+b) = phasor a * phasor b := by
  unfold phasor
  rw [← Complex.exp_add]
  congr 1
  push_cast
  ring

private theorem phasor_pow (angle : ℝ) (n : ℕ) : phasor angle ^ n = phasor ((n : ℝ) * angle) := by
  unfold phasor
  rw [← Complex.exp_nat_mul]
  congr 1
  push_cast
  ring

private def frozenAtAngle (gain angle : ℝ) : ℂ :=
  SchroederAllPass.response (gain : ℂ) (phasor angle) 1

/-- Coherent mean of the ideal frozen response. The two angles are the
frequency in radians per sample times the nominal delay and the wander depth. -/
def expectedResponse (gain center spread : ℝ) : ℂ :=
  ∫ x, frozenAtAngle gain (center + spread * x) ∂stationaryAmplitudeLaw

theorem expectedResponse_eq_frozen (gain frequency delay depth : ℝ) :
    expectedResponse gain (2 * Real.pi * frequency * delay) (2 * Real.pi * frequency * depth) =
      ∫ x, ModulatedMode.frozenAllPass gain frequency (delay + depth * x) ∂stationaryAmplitudeLaw := by
  unfold expectedResponse
  apply integral_congr_ae
  filter_upwards with x
  have h : phasor (2 * Real.pi * frequency * delay + (2 * Real.pi * frequency * depth) * x) =
      ModulatedMode.delayPhasor frequency (delay + depth * x) := by
    unfold phasor ModulatedMode.delayPhasor
    congr 1
    push_cast
    ring
  rw [frozenAtAngle, ModulatedMode.frozenAllPass, h]

/-- The nth term, indexed from zero, is the contribution of n+1 traversals
of the same frozen delay before the factor 1 - gain². -/
def echoTerm (gain center spread : ℝ) (n : ℕ) : ℂ :=
  (gain : ℂ)^n * phasor (((n : ℝ) + 1) * center) *
    stationaryCharacteristic (((n : ℝ) + 1) * spread)

def partialResponse (gain center spread : ℝ) (terms : ℕ) : ℂ :=
  -(gain : ℂ) + (1 - (gain : ℂ)^2) * ∑ n ∈ Finset.range terms, echoTerm gain center spread n

private theorem norm_frozenAtAngle {gain angle : ℝ} (hg : |gain| < 1) :
    ‖frozenAtAngle gain angle‖ = 1 := by
  have hq : Complex.normSq (phasor angle) = 1 := by
    simp [Complex.normSq_eq_norm_sq, norm_phasor]
  have h := SchroederAllPass.response_unit gain (phasor angle) 1 hg hq
  change Complex.normSq (frozenAtAngle gain angle) = 1 at h
  rw [Complex.normSq_eq_norm_sq] at h
  rw [← Real.sqrt_sq (norm_nonneg (frozenAtAngle gain angle)), h, Real.sqrt_one]

theorem expectedResponse_dc {gain : ℝ} (hg : |gain| < 1) : expectedResponse gain 0 0 = 1 := by
  have hg' : gain ≠ 1 := (lt_of_le_of_lt (le_abs_self gain) hg).ne
  have hd : (1 : ℂ) - (gain : ℂ) ≠ 0 := by
    apply sub_ne_zero.mpr
    exact_mod_cast hg'.symm
  simp [expectedResponse, frozenAtAngle, phasor, SchroederAllPass.response, hd]

theorem norm_expectedResponse_le_one {gain : ℝ} (hg : |gain| < 1) (center spread : ℝ) :
    ‖expectedResponse gain center spread‖ ≤ 1 := by
  have h (x : ℝ) : ‖frozenAtAngle gain (center + spread * x)‖ ≤ 1 := (norm_frozenAtAngle hg).le
  simpa [expectedResponse] using norm_integral_le_of_norm_le_const (ae_of_all stationaryAmplitudeLaw h)

/-- Every frozen ideal delay has unit power gain before the coherent average. -/
theorem expected_frozen_power_eq_one {gain : ℝ} (hg : |gain| < 1) (frequency delay depth : ℝ) :
    (∫ x, Complex.normSq (ModulatedMode.frozenAllPass gain frequency (delay + depth * x))
      ∂stationaryAmplitudeLaw) = 1 := by
  simp [ModulatedMode.frozenAllPass_normSq hg]

private theorem integrable_frozenAtAngle {gain : ℝ} (hg : |gain| < 1) (center spread : ℝ) :
    Integrable (fun x => frozenAtAngle gain (center + spread * x)) stationaryAmplitudeLaw := by
  apply (integrable_const (1 : ℝ)).mono'
  · have h : Measurable (fun x => frozenAtAngle gain (center + spread * x)) := by
      unfold frozenAtAngle SchroederAllPass.response phasor
      fun_prop
    exact h.aestronglyMeasurable
  · exact ae_of_all _ fun _ => (norm_frozenAtAngle hg).le

private theorem integrable_echoAt (gain center spread : ℝ) (n : ℕ) :
    Integrable (fun x => (gain : ℂ)^n * phasor (((n : ℝ) + 1) * (center + spread * x)))
      stationaryAmplitudeLaw := by
  apply (integrable_const (|gain| ^ n : ℝ)).mono'
  · have h : Continuous (fun x => (gain : ℂ)^n * phasor (((n : ℝ) + 1) * (center + spread * x))) := by
      unfold phasor
      fun_prop
    exact h.aestronglyMeasurable
  · exact ae_of_all _ fun _ => by simp [norm_phasor, norm_pow]

private theorem integral_echoAt (gain center spread : ℝ) (n : ℕ) :
    (∫ x, (gain : ℂ)^n * phasor (((n : ℝ) + 1) * (center + spread * x)) ∂stationaryAmplitudeLaw) =
      echoTerm gain center spread n := by
  let k := (n : ℝ) + 1
  have h : ∀ x : ℝ, phasor (k * (center + spread * x)) =
      phasor (k * center) * phasor ((k * spread) * x) := by
    intro x
    rw [← phasor_add]
    congr 1
    ring
  change (∫ x, (gain : ℂ)^n * phasor (k * (center + spread * x)) ∂stationaryAmplitudeLaw) = _
  simp_rw [h, ← mul_assoc]
  rw [integral_const_mul]
  have hs : (∫ x, phasor ((k * spread) * x) ∂stationaryAmplitudeLaw) =
      stationaryCharacteristic (-(k * spread)) := by
    rw [stationaryCharacteristic, charFun_apply_real]
    apply integral_congr_ae
    filter_upwards with x
    unfold phasor
    congr 1
    push_cast
    ring
  rw [hs, stationaryCharacteristic_neg]
  rfl

private def partialAt (gain angle : ℝ) (terms : ℕ) : ℂ :=
  -(gain : ℂ) + (1 - (gain : ℂ)^2) *
    ∑ n ∈ Finset.range terms, (gain : ℂ)^n * phasor (((n : ℝ) + 1) * angle)

private theorem integral_partialAt (gain center spread : ℝ) (terms : ℕ) :
    (∫ x, partialAt gain (center + spread * x) terms ∂stationaryAmplitudeLaw) =
      partialResponse gain center spread terms := by
  have hi := integrable_finsetSum (μ := stationaryAmplitudeLaw) (Finset.range terms)
    (fun n _ => integrable_echoAt gain center spread n)
  unfold partialAt partialResponse
  rw [integral_add (integrable_const _) (hi.const_mul _), integral_const, integral_const_mul,
    integral_finsetSum _ (fun n _ => integrable_echoAt gain center spread n)]
  simp [integral_echoAt]

private theorem integrable_partialAt (gain center spread : ℝ) (terms : ℕ) :
    Integrable (fun x => partialAt gain (center + spread * x) terms) stationaryAmplitudeLaw := by
  have hi := integrable_finsetSum (μ := stationaryAmplitudeLaw) (Finset.range terms)
    (fun n _ => integrable_echoAt gain center spread n)
  exact (integrable_const _).add (hi.const_mul _)

/-- The tail bound applies after averaging over the frozen delay. -/
theorem expectedResponse_truncation_le {gain : ℝ} (hg : |gain| < 1)
    (center spread : ℝ) (terms : ℕ) :
    ‖expectedResponse gain center spread - partialResponse gain center spread terms‖ ≤
      (1 + |gain|) * |gain|^terms := by
  unfold expectedResponse
  rw [← integral_partialAt gain center spread terms,
    ← integral_sub (integrable_frozenAtAngle hg center spread) (integrable_partialAt gain center spread terms)]
  have h (x : ℝ) : ‖frozenAtAngle gain (center + spread*x) -
      partialAt gain (center + spread*x) terms‖ ≤ (1 + |gain|) * |gain|^terms := by
    simpa [frozenAtAngle, partialAt, phasor_pow, Nat.cast_add] using
      SchroederAllPass.response_truncation_le hg (phasor (center + spread*x)) (norm_phasor _) terms
  simpa using norm_integral_le_of_norm_le_const (ae_of_all stationaryAmplitudeLaw h)

private theorem norm_echoTerm_le (gain center spread : ℝ) (n : ℕ) :
    ‖echoTerm gain center spread n‖ ≤ |gain|^n := by
  simpa [echoTerm, norm_mul, norm_phasor, norm_pow] using
    mul_le_mul_of_nonneg_left (norm_stationaryCharacteristic_le_one (((n : ℝ) + 1) * spread))
      (pow_nonneg (abs_nonneg gain) n)

/-- The mean response is the direct term plus the characteristic-function
weighted series of delayed returns. -/
theorem expectedResponse_eq_tsum {gain : ℝ} (hg : |gain| < 1) (center spread : ℝ) :
    expectedResponse gain center spread =
      -(gain : ℂ) + (1 - (gain : ℂ)^2) * ∑' n, echoTerm gain center spread n := by
  have hs : Summable (echoTerm gain center spread) :=
    (hasSum_geometric_of_lt_one (abs_nonneg gain) hg).summable.of_norm_bounded
      (norm_echoTerm_le gain center spread)
  have ht := hs.hasSum.tendsto_sum_nat
  have hseries : Tendsto (partialResponse gain center spread) atTop
      (𝓝 (-(gain : ℂ) + (1 - (gain : ℂ)^2) * ∑' n, echoTerm gain center spread n)) :=
    (ht.const_mul _).const_add _
  have hbound := (tendsto_pow_atTop_nhds_zero_of_lt_one (abs_nonneg gain) hg).const_mul (1 + |gain|)
  have herror : Tendsto (fun n => ‖partialResponse gain center spread n -
      expectedResponse gain center spread‖) atTop (𝓝 0) := by
    apply squeeze_zero (fun _ => norm_nonneg _) (fun n => ?_) (by simpa using hbound)
    simpa [norm_sub_rev] using expectedResponse_truncation_le hg center spread n
  have hm : Tendsto (partialResponse gain center spread) atTop (𝓝 (expectedResponse gain center spread)) :=
    tendsto_iff_norm_sub_tendsto_zero.mpr herror
  exact tendsto_nhds_unique hm hseries

/-- The complex mean tends to the direct term, uniformly in the nominal phase. -/
theorem norm_expectedResponse_add_gain_le {gain spread : ℝ} (hg : |gain| < 1) (center : ℝ)
    (hspread : spread ≠ 0) :
    ‖expectedResponse gain center spread + (gain : ℂ)‖ ≤ 6 * (1 + |gain|) / |spread| := by
  let B : ℝ := 6 / |spread|
  have hb (n : ℕ) : ‖echoTerm gain center spread n‖ ≤ B * |gain|^n := by
    have hk : 1 ≤ (n : ℝ) + 1 := by linarith [Nat.cast_nonneg (α := ℝ) n]
    have hkp : 0 < (n : ℝ) + 1 := zero_lt_one.trans_le hk
    have hden : |spread| ≤ |((n : ℝ) + 1) * spread| := by
      rw [abs_mul, abs_of_pos hkp]
      simpa using mul_le_mul_of_nonneg_right hk (abs_nonneg spread)
    have hc := (norm_stationaryCharacteristic_le_six_div_abs (mul_ne_zero hkp.ne' hspread)).trans
      (div_le_div_of_nonneg_left (by norm_num) (abs_pos.mpr hspread) hden)
    simp only [echoTerm, norm_mul, norm_pow, norm_phasor, mul_one, Complex.norm_real, Real.norm_eq_abs]
    simpa only [mul_comm] using mul_le_mul_of_nonneg_left hc (pow_nonneg (abs_nonneg gain) n)
  have hgeom := (hasSum_geometric_of_lt_one (abs_nonneg gain) hg).mul_left B
  have hnorm : Summable (fun n => ‖echoTerm gain center spread n‖) :=
    Summable.of_nonneg_of_le (fun _ => norm_nonneg _) hb hgeom.summable
  have hsum := (norm_tsum_le_tsum_norm hnorm).trans
    ((hnorm.tsum_le_tsum hb hgeom.summable).trans_eq hgeom.tsum_eq)
  rw [expectedResponse_eq_tsum hg, neg_add_cancel_comm, norm_mul, SchroederAllPass.norm_one_sub_gain_sq hg]
  calc
    _ ≤ ((1 + |gain|) * (1 - |gain|)) * (B * (1 - |gain|)⁻¹) :=
      mul_le_mul_of_nonneg_left hsum (by positivity)
    _ = _ := by
      unfold B
      field_simp [ne_of_gt (sub_pos.mpr hg)]

theorem expectedResponse_tendsto_neg_gain {ι : Type*} {l : Filter ι} {gain : ℝ} (hg : |gain| < 1)
    (center spread : ι → ℝ) (h : Tendsto (fun i => |spread i|) l atTop) :
    Tendsto (fun i => expectedResponse gain (center i) (spread i)) l (𝓝 (-(gain : ℂ))) := by
  apply tendsto_iff_norm_sub_tendsto_zero.mpr
  simp only [sub_neg_eq_add]
  have ht : Tendsto (fun i => (6 * (1 + |gain|)) / |spread i|) l (𝓝 (0 : ℝ)) :=
    tendsto_const_nhds.div_atTop h
  apply squeeze_zero' (Eventually.of_forall fun _ => norm_nonneg _) _ ht
  filter_upwards [(tendsto_atTop.1 h) 1] with i hi
  exact norm_expectedResponse_add_gain_le hg _ (abs_pos.mp (zero_lt_one.trans_le hi))

end

end DSP.ExpectedAllPass

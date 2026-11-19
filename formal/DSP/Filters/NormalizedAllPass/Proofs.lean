import DSP.Filters.NormalizedAllPass
import DSP.Transfer.Digital
import DSP.Transfer.Delay
import DSP.ResponseFrequency.Proofs
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.LinearCombination
import Mathlib.Tactic.Linarith

namespace DSP.NormalizedAllPass

open Matrix Polynomial

variable {K : Type}

/-- Exact normalization of the lattice's two coefficients. -/
def Normalized [CommRing K] (f : NormalizedAllPass K) : Prop := f.a ^ 2 + f.b ^ 2 = 1

/-- One dynamic state, observed before it advances. -/
def discrete [CommRing K] (a b r : K) : StateSpace 1 K where
  A _ _ := -a * r
  B _ := b
  C _ := b * r
  D := a

section Realization
variable [CommRing K]

theorem step_coefficients (f : NormalizedAllPass K) (x : Sample K) :
    (step f x).a = f.a ∧ (step f x).b = f.b ∧ (step f x).r = f.r := ⟨rfl, rfl, rfl⟩

/-- The executable recurrence is this state-space realization in exact arithmetic. -/
theorem step_eq_discrete (f : NormalizedAllPass K) (x : Sample K) :
    (fun _ : Fin 1 => (step f x).s, (step f x).y) =
      (discrete f.a f.b f.r).processor (fun _ => f.s) x.x := by
  simp only [StateSpace.processor, StateSpace.snapshot_values, StateSpace.observe,
    StateSpace.dot, discrete, step, fma_eq]
  refine Prod.ext (funext fun _ => ?_) ?_ <;>
    simp only [StateSpace.advance, StateSpace.dot, fma_eq] <;> ring

theorem states_eq (u : ℕ → K) (f : NormalizedAllPass K) (n : ℕ) :
    let st := Step.states step (fun k => ⟨u k⟩) f n
    st.a = f.a ∧ st.b = f.b ∧ st.r = f.r ∧
      (fun _ : Fin 1 => st.s) =
        Processor.runState (discrete f.a f.b f.r).processor u (fun _ => f.s) n := by
  induction n with
  | zero => exact ⟨rfl, rfl, rfl, rfl⟩
  | succ n ih =>
    obtain ⟨ha, hb, hr, hs⟩ := ih
    have h := congrArg Prod.fst
      (step_eq_discrete (Step.states step (fun k => ⟨u k⟩) f n) ⟨u n⟩)
    rw [ha, hb, hr, hs] at h
    exact ⟨ha, hb, hr, h⟩

/-- Every output, for arbitrary input and initial memory, matches the realization. -/
theorem run_eq (u : ℕ → K) (f : NormalizedAllPass K) (n : ℕ) :
    (Step.states step (fun k => ⟨u k⟩) f (n + 1)).y =
      Processor.run (discrete f.a f.b f.r).processor u (fun _ => f.s) n := by
  obtain ⟨ha, hb, hr, hs⟩ := states_eq u f n
  have h := congrArg Prod.snd
    (step_eq_discrete (Step.states step (fun k => ⟨u k⟩) f n) ⟨u n⟩)
  rw [ha, hb, hr, hs] at h
  exact h

end Realization

section Transfer
variable [Field K]

/-- Transfer in the unit-delay variable q=z⁻¹, including radial memory loss. -/
def response (a r q : K) : K := (a + r * q) / (1 + a * r * q)

/-- A compositional mode law for the executable junction, including q=0.
Clients supply the memory relation instead of constructing a matrix inverse. -/
theorem mode_equation (f : NormalizedAllPass K) (x : Sample K) (q : K)
    (hn : f.Normalized) (hs : f.s = q * (step f x).s) :
    (1 + f.a * f.r * q) * (step f x).y = (f.a + f.r * q) * x.x := by
  simp only [step] at hs ⊢
  unfold Normalized at hn
  linear_combination f.b * f.r * hs + (f.r * q * x.x) * hn

theorem discrete_regular (a b r z : K) (hz : z + a * r ≠ 0) :
    (discrete a b r).RegularAt z := by
  simpa [StateSpace.RegularAt, StateSpace.resolvent, Matrix.det_unique, discrete] using hz

/-- Compute the transfer of the actual state-space realization, away from its pole. -/
theorem discrete_transfer (a b r z : K) (hn : a ^ 2 + b ^ 2 = 1)
    (hz : z + a * r ≠ 0) :
    (discrete a b r).transfer z = (a * z + r) / (z + a * r) := by
  symm
  apply ((discrete a b r).modeEquations_iff_transfer z _ (discrete_regular a b r z hz)).mp
  refine ⟨fun _ => b / (z + a * r), ?_, ?_⟩
  · funext i
    fin_cases i
    simp [discrete, Matrix.mulVec, dotProduct]
    field_simp
    ring
  · simp [discrete, dotProduct]
    field_simp
    linear_combination -r * hn

/-- The rational formula characterizes geometric input/state/output trajectories. -/
theorem digital_modes (a b r z gain : K) (hn : a ^ 2 + b ^ 2 = 1)
    (hz : z + a * r ≠ 0) :
    (discrete a b r).HasDigitalMode z gain ↔ gain = (a * z + r) / (z + a * r) := by
  rw [StateSpace.digital_hasTransfer _ _ _ (discrete_regular a b r z hz),
    discrete_transfer a b r z hn hz]

theorem transfer_eq_response (a b r z : K) (hn : a ^ 2 + b ^ 2 = 1)
    (hz : z ≠ 0) (hp : z + a * r ≠ 0) :
    (discrete a b r).transfer z = response a r z⁻¹ := by
  rw [discrete_transfer a b r z hn hp]
  unfold response
  field_simp

theorem delay_denominator (a b r : K) :
    (discrete a b r).delayDenominator = 1 + C (a * r) * X := by
  simp [StateSpace.delayDenominator, StateSpace.delayDenominatorAt, StateSpace.delayMatrix,
    StateSpace.map, discrete]
  ring

theorem delay_numerator (a b r : K) (hn : a ^ 2 + b ^ 2 = 1) :
    (discrete a b r).delayNumerator = C a + C r * X := by
  simp [StateSpace.delayNumerator, StateSpace.delayNumeratorAt, StateSpace.delayDenominatorAt,
    StateSpace.delayMatrix, StateSpace.map, discrete, Matrix.adjugate_fin_one,
    dotProduct, Matrix.mulVec]
  have hc : C a ^ 2 + C b ^ 2 = (1 : K[X]) := by
    simpa using congrArg (Polynomial.C : K →+* K[X]) hn
  linear_combination C r * X * hc

/-- From rest, arbitrary input streams obey the numerator/denominator equation.
This complements the complex-mode theorem without assuming convergence of a transform. -/
theorem run_seriesEquation (a b r y₀ : K) (hn : a ^ 2 + b ^ 2 = 1) (u : ℕ → K) :
    ((1 + C (a * r) * X : K[X]) : PowerSeries K) *
        IIR.series (fun n => (Step.states step (fun k => ⟨u k⟩)
          ⟨a, b, r, 0, y₀⟩ (n + 1)).y) =
      ((C a + C r * X : K[X]) : PowerSeries K) * IIR.series u := by
  simp_rw [run_eq]
  have h := (discrete a b r).run_seriesEquation u
  rw [delay_denominator, delay_numerator a b r hn] at h
  exact h

end Transfer

section ComplexResponse

/-- Evaluate the realized transfer in the complex delay variable q=z⁻¹.
The denominator hypothesis excludes poles rather than defining a response there. -/
theorem complex_transfer (a b r : ℝ) (q : ℂ) (hn : a ^ 2 + b ^ 2 = 1)
    (hq : q ≠ 0) (hd : 1 + (a : ℂ) * r * q ≠ 0) :
    (discrete (a : ℂ) b r).transfer q⁻¹ = response (a : ℂ) r q := by
  have hn' : (a : ℂ) ^ 2 + (b : ℂ) ^ 2 = 1 := by exact_mod_cast hn
  have hp : q⁻¹ + (a : ℂ) * r ≠ 0 := by
    have he : q⁻¹ + (a : ℂ) * r = (1 + (a : ℂ) * r * q) / q := by
      field_simp
    rw [he]
    exact div_ne_zero hd hq
  simpa using transfer_eq_response (a : ℂ) b r q⁻¹ hn' (inv_ne_zero hq) hp

/-- A stable real pole cannot make the response singular on the unit circle. -/
theorem unit_denominator_ne_zero (a r : ℝ) (q : ℂ) (har : |a * r| < 1)
    (hq : Complex.normSq q = 1) : 1 + (a : ℂ) * r * q ≠ 0 := by
  have hqnorm : ‖q‖ = 1 := by
    rw [Complex.normSq_eq_norm_sq] at hq
    nlinarith [norm_nonneg q]
  intro h
  have hp : (a : ℂ) * r * q = -1 := by linear_combination h
  have hn := congrArg norm hp
  rw [← Complex.ofReal_mul, norm_mul, hqnorm, mul_one, Complex.norm_real,
    Real.norm_eq_abs] at hn
  norm_num at hn
  rw [abs_mul] at har
  linarith

/-- The difference of squared denominator/numerator magnitudes factors into
the two losses. In particular it vanishes for the lossless section r=1. -/
theorem normSq_gap (a r : ℝ) (q : ℂ) (hq : Complex.normSq q = 1) :
    Complex.normSq (1 + (a : ℂ) * r * q) - Complex.normSq ((a : ℂ) + r * q) =
      (1 - a ^ 2) * (1 - r ^ 2) := by
  simp only [Complex.normSq_apply] at hq
  simp [Complex.normSq_apply]
  linear_combination (a ^ 2 * r ^ 2 - r ^ 2) * hq

/-- Unity magnitude at every point of the unit circle, including DC and Nyquist. -/
theorem response_unit (a : ℝ) (q : ℂ) (ha : |a| < 1)
    (hq : Complex.normSq q = 1) : Complex.normSq (response (a : ℂ) 1 q) = 1 := by
  have hd := unit_denominator_ne_zero a 1 q (by simpa using ha) hq
  have he := normSq_gap a 1 q hq
  have heq : Complex.normSq ((a : ℂ) + (1 : ℂ) * q) =
      Complex.normSq (1 + (a : ℂ) * 1 * q) := by
    norm_num at he ⊢
    linarith
  rw [response, Complex.normSq_div, heq]
  exact div_self (mt Complex.normSq_eq_zero.mp hd)

/-- The prepared frequency uses the same exp(-2πif) convention as the app. -/
theorem frequency_response_unit (a frequency : ℝ) (ha : |a| < 1) :
    Complex.normSq (response (a : ℂ) 1 (ResponseFrequency.unitDelay frequency)) = 1 := by
  apply response_unit a _ ha
  simp [Complex.normSq_apply, ResponseFrequency.unitDelay_re, ResponseFrequency.unitDelay_im,
    ← sq, Real.cos_sq_add_sin_sq]

end ComplexResponse

section Energy

/-- The canonical nonnegative square root used by the C++ coefficient constructor. -/
theorem sqrt_normalized (a : ℝ) (ha : |a| ≤ 1) :
    a ^ 2 + Real.sqrt (1 - a ^ 2) ^ 2 = 1 := by
  have h : 0 ≤ 1 - a ^ 2 := by
    linarith [(sq_le_one_iff_abs_le_one a).mpr ha]
  rw [Real.sq_sqrt h]
  ring

/-- Output energy plus new memory energy equals input energy plus retained memory.
This is local: the coefficients may change before every call. -/
theorem step_energy (f : NormalizedAllPass ℝ) (x : Sample ℝ) (hn : f.Normalized) :
    (step f x).y ^ 2 + (step f x).s ^ 2 = x.x ^ 2 + f.r ^ 2 * f.s ^ 2 := by
  unfold step Normalized at *
  dsimp only
  linear_combination (x.x ^ 2 + f.r ^ 2 * f.s ^ 2) * hn

theorem step_energy_lossless (f : NormalizedAllPass ℝ) (x : Sample ℝ)
    (hn : f.Normalized) (hr : f.r = 1) :
    (step f x).y ^ 2 + (step f x).s ^ 2 = x.x ^ 2 + f.s ^ 2 := by
  rw [step_energy f x hn, hr]
  ring

/-- The damped update cannot create energy, including under coefficient modulation. -/
theorem step_energy_le (f : NormalizedAllPass ℝ) (x : Sample ℝ)
    (hn : f.Normalized) (hr : |f.r| ≤ 1) :
    (step f x).y ^ 2 + (step f x).s ^ 2 ≤ x.x ^ 2 + f.s ^ 2 := by
  rw [step_energy f x hn]
  have hr2 : f.r ^ 2 ≤ 1 := (sq_le_one_iff_abs_le_one f.r).mpr hr
  nlinarith [mul_le_mul_of_nonneg_right hr2 (sq_nonneg f.s)]

/-- A finite-horizon energy balance for arbitrary coefficient schedules.
Only the memory is carried between calls; retuning may change a, b and r. -/
theorem trajectory_energy (f : ℕ → NormalizedAllPass ℝ) (u : ℕ → ℝ)
    (hn : ∀ n, (f n).Normalized)
    (hs : ∀ n, (f (n + 1)).s = (step (f n) ⟨u n⟩).s) (N : ℕ) :
    (∑ n ∈ Finset.range N, (step (f n) ⟨u n⟩).y ^ 2) + (f N).s ^ 2 +
        (∑ n ∈ Finset.range N, (1 - (f n).r ^ 2) * (f n).s ^ 2) =
      (∑ n ∈ Finset.range N, u n ^ 2) + (f 0).s ^ 2 := by
  induction N with
  | zero => simp
  | succ N ih =>
    rw [Finset.sum_range_succ, Finset.sum_range_succ, Finset.sum_range_succ]
    have he := step_energy (f N) ⟨u N⟩ (hn N)
    rw [hs N]
    dsimp only at he
    nlinarith

end Energy

end DSP.NormalizedAllPass

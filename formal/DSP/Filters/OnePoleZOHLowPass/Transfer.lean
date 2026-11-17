import DSP.Filters.OnePoleZOHLowPass.Proofs
import DSP.Discretization.Pipeline
import DSP.Transfer.Analog
import DSP.Transfer.Bilinear

namespace DSP.OnePoleZOHLowPass

open Matrix Polynomial

/-- Complex frequencies describe the real lowpass ODE's exponential modes. -/
theorem analog_transfer (τ : ℝ) (hτ : τ ≠ 0) (s gain : ℂ)
    (hs : 1 + (τ : ℂ) * s ≠ 0) :
    ((continuousSystem τ).map Complex.ofRealHom).HasAnalogMode s gain ↔
      gain = 1 / (1 + (τ : ℂ) * s) := by
  rw [StateSpace.analogMode_iff_equations]
  have ht : (τ : ℂ) ≠ 0 := by exact_mod_cast hτ
  constructor
  · rintro ⟨v, hv, hg⟩
    have he := congrFun hv 0
    simp [continuousSystem, StateSpace.map, Matrix.mulVec, dotProduct] at he hg
    rw [hg]
    apply (eq_div_iff hs).mpr
    field_simp [ht] at he
    linear_combination he
  · intro hg
    refine ⟨fun _ => gain, ?_, ?_⟩
    · funext i
      fin_cases i
      simp [continuousSystem, StateSpace.map, Matrix.mulVec, dotProduct, hg]
      have hs' : 1 + s * (τ : ℂ) ≠ 0 := by simpa [mul_comm] using hs
      field_simp [ht, hs, hs']
      ring
    · simp [continuousSystem, StateSpace.map, dotProduct]

/-- Delay-variable transfer for the exported updated-state observation. -/
theorem delay_denominator (alpha : ℝ) :
    (discrete alpha).observeNext.delayDenominator = 1 - Polynomial.C (1 - alpha) * X := by
  simp [StateSpace.delayDenominator, StateSpace.delayDenominatorAt, StateSpace.delayMatrix,
    StateSpace.map, StateSpace.observeNext, discrete]
  ring

theorem delay_numerator (alpha : ℝ) :
    (discrete alpha).observeNext.delayNumerator = Polynomial.C alpha := by
  simp [StateSpace.delayNumerator, StateSpace.delayNumeratorAt, StateSpace.delayDenominatorAt,
    StateSpace.delayMatrix, StateSpace.map, StateSpace.observeNext, discrete,
    Matrix.adjugate_fin_one, dotProduct, Matrix.vecMul, Matrix.mulVec]
  ring

/-- The stored output is both the next state and the updated-state observation. -/
theorem exported_real_step (state : OnePoleZOHLowPass ℝ) (input : Sample ℝ) :
    (fun _ : Fin 1 => (step state input).y, (step state input).y) =
      (discrete state.alpha).observeNext.processor (fun _ => state.y) input.x := by
  rw [StateSpace.observeNext_equiv]
  simp [StateSpace.processorNext, StateSpace.observe, StateSpace.dot, StateSpace.orderedSum, discrete,
    step, process, onePoleStep, StateSpace.advance, StateSpace.snapshot_values]
  rfl

/-- The generated step's real interpretation equals the generic ZOH endpoint. -/
theorem exported_zoh_step (T τ : ℝ) (hT : 0 ≤ T) (input initial : ℝ) :
    (step { alpha := 1 - Real.exp (-T / τ), y := initial } { x := input }).y =
      (((continuousSystem τ).zoh T).advance (fun _ => initial) input) 0 := by
  rw [zoh_advance_eq T τ input initial hT]
  rfl

/-- The normalized digital rational function is realized for arbitrary input streams. -/
theorem lowpass_iir_equivalent (alpha : ℝ) (u : ℕ → ℝ) :
    Processor.run (IIR.processor (IIR.ofPolynomials (Polynomial.C alpha) (1 - Polynomial.C (1 - alpha) * X)))
        u IIR.zeroState = Processor.run (discrete alpha).observeNext.processor u (fun _ => 0) := by
  have h := (discrete alpha).observeNext.toIIR_correct u
  change Processor.run (IIR.processor (IIR.ofPolynomials _ _)) _ _ = _ at h
  rw [delay_numerator, delay_denominator] at h
  exact h

/-- All analog held trajectories starting at rest share the synthesized IIR output. -/
theorem analog_iir_endpoint (T τ : ℝ) (hT : 0 ≤ T) (u : ℕ → ℝ)
    (q : (continuousSystem τ).HeldRealization T u (fun _ => 0)) (k : ℕ) :
    Processor.run (IIR.processor ((continuousSystem τ).zoh T).observeNext.toIIR) u IIR.zeroState k =
      q.segment k T 0 := by
  have h := (continuousSystem τ).zoh_iir_endpointOutput T hT u q k
  simpa [continuousSystem, StateSpace.observe, StateSpace.dot, StateSpace.orderedSum] using h

open Bilinear in
/-- From rest, the stored endpoint realizes α / (1 - (1 - α) q), with q the
unit delay. This is the ZOH recurrence's digital transfer, without a bilinear
substitution of the analog prototype. -/
theorem realizes {K : Type} [Field K] (f : OnePoleZOHLowPass K) (hy : f.y = 0) :
    Realizes step f (·.y) (const f.alpha / (1 - const (1 - f.alpha) * delay)) := by
  intro u
  let st := Step.states step (fun k => ⟨u k⟩) f
  have ha : ∀ n, (st n).alpha = f.alpha :=
    fun n => Step.states_invariant step (·.alpha) (fun _ _ => rfl) _ f n
  have hrec : ser (fun n => (st (n + 1)).y) =
      const (1 - f.alpha) * ser (fun n => (st n).y) + const f.alpha * ser u := by
    rw [← ser_mul, ← ser_mul, ← ser_add]
    apply ser_congr
    intro n
    simp only [st, Step.states_succ, step_y, onePoleStep_eq, ha]
  have hshift := ser_shift (fun n => (st n).y)
  change ser (fun n => (st n).y) = const f.y + _ at hshift
  rw [hy, const_zero, zero_add] at hshift
  rw [hshift] at hrec
  have hd : (1 : Stream K) - const (1 - f.alpha) * delay ≠ 0 := by
    have h := embed_ne_zero (1 - PowerSeries.C (1 - f.alpha) * PowerSeries.X)
      (by simp)
    simpa [const, delay] using h
  change ser (fun n => (st (n + 1)).y) = _
  rw [div_mul_eq_mul_div, eq_div_iff hd]
  linear_combination hrec

end DSP.OnePoleZOHLowPass

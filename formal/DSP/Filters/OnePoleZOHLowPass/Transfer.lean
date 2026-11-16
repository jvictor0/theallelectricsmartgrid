import DSP.Filters.OnePoleZOHLowPass.Proofs
import DSP.Discretization.Pipeline
import DSP.Transfer.Analog

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

end DSP.OnePoleZOHLowPass

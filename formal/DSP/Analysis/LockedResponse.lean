import DSP.Analysis.PeriodicOrbit
import DSP.ResponseMeasurement.ErrorBounds

/-! From a block contraction certificate to a certified measured response.
Orbit existence and attraction are conclusions. Uniform block/prefix bounds
and reference evaluation errors are explicit certificate inputs. -/

noncomputable section
namespace DSP.LockedResponse
open Filter Metric
open scoped NNReal Topology

variable {E : Type} [MetricSpace E]

/-- A candidate-state error propagates through a Lipschitz sample observation.
The separate evaluation term may include numerical error or a truncated
waveform approximation. It cannot silently be set to zero. -/
theorem observation_error (observe : E → ℝ) (center orbit : E) (radius : ℝ) (L : ℝ≥0)
    (hr : 0 ≤ radius) (hp : orbit ∈ closedBall center radius)
    (hl : LipschitzOnWith L observe (closedBall center radius))
    (stateError reference evaluationError : ℝ) (hs : dist center orbit ≤ stateError)
    (he : |observe center-reference| ≤ evaluationError) :
    |observe orbit-reference| ≤ L*stateError+evaluationError := by
  have hb := hl.dist_le_mul orbit hp center (mem_closedBall_self hr)
  rw [Real.dist_eq, dist_comm orbit center] at hb
  have ht := abs_add_le (observe orbit-observe center) (observe center-reference)
  rw [sub_add_sub_cancel] at ht
  have hm := mul_le_mul_of_nonneg_left hs L.coe_nonneg
  linarith

/-- Local existence and attraction, with certified coherent and total-power
errors for any proposed period of approximate output values. The output is a
continuous observation of the sample state. Positive N and a periodic unit
carrier are required; coherent power uses the usual non-DC/Nyquist convention. -/
theorem exists_response_bounds [CompleteSpace E] {I : Type}
    (step : DSP.Step E I) (hc : ∀ u, Continuous (fun x => step x u))
    (input : ℕ → I) (N : ℕ) (hN : 0 < N) (hu : ∀ n, input (N+n) = input n)
    (observe : E → ℝ) (ho : Continuous observe)
    (center : E) (radius : ℝ) (K : ℝ≥0) (hr : 0 ≤ radius) (hk : K < 1)
    (hblock : LipschitzOnWith K (fun x => DSP.Step.states step input x N) (closedBall center radius))
    (htrap : dist (DSP.Step.states step input center N) center + K*radius ≤ radius)
    (L : ℕ → ℝ≥0) (reference evaluationError : ℕ → ℝ)
    (hprefix : ∀ j < N, LipschitzOnWith (L j)
      (fun x => observe (DSP.Step.states step input x j)) (closedBall center radius))
    (heval : ∀ j < N, |observe (DSP.Step.states step input center j)-reference j| ≤ evaluationError j)
    (z : ℂ) (hz : ‖z‖ = 1) (hzN : z^N = 1) :
    let stateError := dist center (DSP.Step.states step input center N)/(1-K)
    let error := fun j => L j*stateError+evaluationError j
    ∃ orbit ∈ closedBall center radius,
      DSP.Step.states step input orbit N = orbit ∧
      (∀ x ∈ closedBall center radius,
        DSP.ResponseMeasurement.HasCoherentResponse z
          (fun n => observe (DSP.Step.states step input x n))
          (DSP.ResponseMeasurement.coherent z (fun n => observe (DSP.Step.states step input orbit n)) N) ∧
        DSP.ResponseMeasurement.HasMeanSquare
          (fun n => observe (DSP.Step.states step input x n))
          (DSP.ResponseMeasurement.meanSquare (fun n => observe (DSP.Step.states step input orbit n)) N)) ∧
      ‖DSP.ResponseMeasurement.coherent z (fun n => observe (DSP.Step.states step input orbit n)) N -
        DSP.ResponseMeasurement.coherent z reference N‖ ≤ 2*(N:ℝ)⁻¹*∑ j ∈ Finset.range N, error j ∧
      |DSP.ResponseMeasurement.meanSquare (fun n => observe (DSP.Step.states step input orbit n)) N -
        DSP.ResponseMeasurement.meanSquare reference N| ≤
          (N:ℝ)⁻¹*∑ j ∈ Finset.range N, error j*(2*|reference j|+error j) := by
  dsimp only
  obtain ⟨orbit, hm, hfix, _, hattract, herror⟩ := DSP.AttractingBlock.exists_attracting_fixedPoint
    (fun x => DSP.Step.states step input x N) center radius K hr hk hblock htrap
  have hperiod : Function.Periodic (fun n => observe (DSP.Step.states step input orbit n)) N := by
    intro n
    dsimp only
    rw [Nat.add_comm, DSP.Step.states_periodic_of_fixed step input N hu orbit hfix]
  have herr (j : ℕ) (hj : j < N) := observation_error
    (fun x => observe (DSP.Step.states step input x j)) center orbit radius (L j) hr hm (hprefix j hj)
    _ (reference j) (evaluationError j) herror (heval j hj)
  refine ⟨orbit, hm, hfix, ?_, ?_, ?_⟩
  · intro x hx
    have ht := DSP.Step.states_observer_tracking step hc input N hN hu x orbit hfix
      (hattract x hx) observe ho
    exact ⟨DSP.ResponseMeasurement.coherent_of_periodic_tracking z hz _ _ N hN hzN hperiod ht,
      DSP.ResponseMeasurement.meanSquare_of_periodic_tracking _ _ N hN hperiod ht⟩
  · exact DSP.ResponseMeasurement.coherent_error_bound z hz _ _ _ N herr
  · exact DSP.ResponseMeasurement.meanSquare_error_bound _ _ _ N herr

end DSP.LockedResponse

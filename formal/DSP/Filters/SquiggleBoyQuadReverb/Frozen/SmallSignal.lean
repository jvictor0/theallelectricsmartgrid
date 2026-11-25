import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Refinement
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Modal
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Complexification
import DSP.Processor.Simulation

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Filter Matrix ResponseMeasurement
open scoped Topology

set_option maxRecDepth 2500 in
set_option maxHeartbeats 1500000 in
private theorem smallSignal_from_mode (p : Spec ℝ) (i j : Fin 4) (theta : Phase)
    (hfreq : star (fourier 1 theta) * star (fourier 1 theta) ≠ 1)
    (gauge : StabilityGauge (LocalDynamics.statePart (columnTangent p j)).toLinearMap)
    (mode : ∃ h : History ℂ, tangent p.complex (h, basisLane j) = fourier 1 theta • h ∧
      h.wet = (fourier 1 theta)⁻¹ • (transfer p.complex (fourier 1 theta)⁻¹ *ᵥ basisLane j)) :
    HasSmallSignalResponse (fourier 1 theta)
      (fun e n => wetLane i (LocalDynamics.orbit (columnUpdate p j)
        (fun k => e * ((fourier 1 theta)^k).re) 0 (n+1)))
      (transfer p.complex (fourier 1 theta)⁻¹ i j) := by
  let z := fourier 1 theta
  have hz : ‖z‖ = 1 := Circle.norm_coe _
  have hz0 : z ≠ 0 := by intro h; simp [h] at hz
  obtain ⟨h, hmode, hwet⟩ := mode
  let ref (n : ℕ) := realHistory (z^n • h)
  have href (n : ℕ) : columnTangent p j (ref n, (z^n).re) = ref (n+1) := by
    have hi : lanesRe (z^n • (basisLane j : Lanes ℂ)) = (z^n).re • (basisLane j : Lanes ℝ) := by
      funext k
      by_cases hk : k = j <;> simp [lanesRe, basisLane, hk, Pi.smul_apply, smul_eq_mul]
    change tangent p (ref n, (z^n).re • basisLane j) = _
    change tangent p (realHistory (z^n • h), (z^n).re • basisLane j) = _
    rw [← hi, ← realHistory_tangent]
    change realHistory (tangent p.complex (z^n • (h, (basisLane j : Lanes ℂ)))) = _
    rw [map_smul, hmode, smul_smul, ← pow_succ]
  have ho (n : ℕ) : wetLane i (ref (n+1)) =
      (transfer p.complex z⁻¹ i j * z^n).re := by
    change (realHistory (z^(n+1) • h)).wet i = _
    rw [realHistory_wet]
    have hmv : (transfer p.complex z⁻¹ *ᵥ basisLane j) i = transfer p.complex z⁻¹ i j := by
      simp [Matrix.mulVec, dotProduct, basisLane, mul_ite]
    change (z^(n+1) * h.wet i).re = _
    rw [show h.wet i = z⁻¹ * transfer p.complex z⁻¹ i j by
      rw [hwet]; change z⁻¹ * _ = _; rw [hmv]]
    congr 1
    rw [pow_succ, mul_assoc (z^n), ← mul_assoc z, mul_inv_cancel₀ hz0, one_mul, mul_comm]
  apply smallSignal_of_stable_update (columnUpdate_zero p j) (columnTangent p j)
    (columnUpdate_hasStrictFDerivAt p j) (columnUpdate_continuous p j) gauge (wetLane i) theta
  apply coherent_of_sinusoid_tracking z _ hz hfreq
  have ht := LocalDynamics.linear_tracks (columnTangent p j) gauge
    (fun n => (z^n).re) 0 ref href
  have h := ((wetLane i).continuous.tendsto 0).comp (ht.comp (tendsto_add_atTop_nat 1))
  simpa only [Function.comp_def, map_sub, map_zero, ho] using h

private theorem run_from_refinement {S T C I O : Type}
    (processor : Processor S I O) (config : S → C) (represent : S → T)
    (update : C → T × I → T) (observe : T → O)
    (step : ∀ s x, config (processor s x).1 = config s ∧
      represent (processor s x).1 = update (config s) (represent s, x) ∧
      (processor s x).2 = observe (represent (processor s x).1))
    (s : S) (input : Nat → I) :
    let model : Processor T I O := fun t x => let next := update (config s) (t, x)
      (next, observe next)
    Processor.run processor input s = Processor.run model input (represent s) := by
  let model : Processor T I O := fun t x => let next := update (config s) (t, x)
    (next, observe next)
  let related (u : S) (t : T) := config u = config s ∧ represent u = t
  apply Processor.run_simulation processor model related
  · intro u t h x
    obtain ⟨hc, ht⟩ := h
    obtain ⟨hconf, hstep, hout⟩ := step u x
    change related (processor u x).1 (update (config s) (t, x)) ∧
      (processor u x).2 = observe (update (config s) (t, x))
    rw [hc, ht] at hstep
    exact ⟨⟨hconf.trans hc, hstep⟩, hout.trans (congrArg observe hstep)⟩
  · exact ⟨rfl, rfl⟩

set_option maxRecDepth 2500 in
set_option maxHeartbeats 500000 in
private theorem smallSignal_runtime_from {S : Type}
    (processor : Processor S (Lanes ℝ) (Lanes ℝ))
    (prepared : S → Spec ℝ) (histories : S → History ℝ)
    (step : ∀ s x, prepared (processor s x).1 = prepared s ∧
      histories (processor s x).1 = update (prepared s) (histories s, x) ∧
      (processor s x).2 = (histories (processor s x).1).wet)
    (s : S) (rest : histories s = 0) (i j : Fin 4) (theta : Phase)
    (signal : HasSmallSignalResponse (fourier 1 theta)
      (fun e n => wetLane i (LocalDynamics.orbit (columnUpdate (prepared s) j)
        (fun k => e * ((fourier 1 theta)^k).re) 0 (n+1)))
      (transfer (prepared s).complex (fourier 1 theta)⁻¹ i j)) :
    HasSmallSignalResponse (fourier 1 theta)
      (fun e n => (Processor.run processor
        (fun k => (e * ((fourier 1 theta)^k).re) • basisLane j) s n) i)
      (transfer (prepared s).complex (fourier 1 theta)⁻¹ i j) := by
  let model : Processor (History ℝ) (Lanes ℝ) (Lanes ℝ) := fun h x =>
    let next := update (prepared s) (h, x)
    (next, next.wet)
  have hr (u : Nat → ℝ) :
      Processor.run processor (fun k => u k • basisLane j) s =
        Processor.run model (fun k => u k • basisLane j) 0 := by
    rw [← rest]
    exact run_from_refinement processor prepared histories update History.wet step s _
  have hm (u : Nat → ℝ) (n : Nat) :
      Processor.runState model (fun k => u k • basisLane j) 0 n =
        LocalDynamics.orbit (columnUpdate (prepared s) j) u 0 n := by
    induction n with
    | zero => rfl
    | succ n ih =>
      rw [Processor.runState_succ]
      change update (prepared s) (_, u n • basisLane j) = _
      rw [ih]
      rfl
  have h (e : ℝ) (n : ℕ) :
      (Processor.run processor (fun k => (e * ((fourier 1 theta)^k).re) • basisLane j) s n) i =
        wetLane i (LocalDynamics.orbit (columnUpdate (prepared s) j)
          (fun k => e * ((fourier 1 theta)^k).re) 0 (n+1)) := by
    let u (k : ℕ) := e * ((fourier 1 theta)^k).re
    change (Processor.run processor (fun k => u k • basisLane j) s n) i = _
    rw [hr u]
    change (update (prepared s)
      (Processor.runState model (fun k => u k • basisLane j) 0 n, u n • basisLane j)).wet i = _
    rw [hm u n]
    rfl
  rwa [show (fun (e : ℝ) (n : ℕ) => (Processor.run processor
      (fun k => (e * ((fourier 1 theta)^k).re) • basisLane j) s n) i) = _
    from funext fun e => funext (h e)]

private theorem histories_wet (s : FrozenState) : (histories s).wet = s.wet := rfl

private theorem processor_observes_histories (s : FrozenState) (x : Lanes ℝ) :
    prepared (processor s x).1 = prepared s ∧
      histories (processor s x).1 = update (prepared s) (histories s, x) ∧
      (processor s x).2 = (histories (processor s x).1).wet := by
  simpa only [histories_wet] using processor_frozen s x

set_option maxRecDepth 2500 in
set_option maxHeartbeats 600000 in
/-- The small-signal coherent response of the exact frozen runtime at
silence. The sample limit is taken first, then the drive amplitude limit.
Input lane `j` is driven with a real sinusoid and wet output lane `i` is
observed. The clock readouts and prepared controls stay fixed throughout. -/
theorem hasSmallSignalResponse (s : FrozenState) (rest : histories s = 0)
    (i j : Fin 4) (theta : Phase)
    (hfreq : star (fourier 1 theta) * star (fourier 1 theta) ≠ 1)
    (gauge : StabilityGauge (LocalDynamics.statePart (columnTangent (prepared s) j)).toLinearMap)
    (blocks : BlockDenominators (prepared s).complex (fourier 1 theta)⁻¹)
    (closed : IsUnit (returnDen (prepared s).complex (fourier 1 theta)⁻¹).det) :
    HasSmallSignalResponse (fourier 1 theta)
      (fun amplitude n => (Processor.run processor
        (fun k => (amplitude * ((fourier 1 theta)^k).re) • basisLane j) s n) i)
      (transfer (prepared s).complex (fourier 1 theta)⁻¹ i j) := by
  have hz : (fourier 1 theta : ℂ) ≠ 0 := by
    intro h
    have hn : ‖(fourier 1 theta : ℂ)‖ = 1 := Circle.norm_coe _
    simp [h] at hn
  have hmode := transfer_mode (prepared s).complex (fourier 1 theta)⁻¹
    (inv_ne_zero hz) blocks closed (basisLane j)
  exact smallSignal_runtime_from processor prepared histories processor_observes_histories s rest i j theta
    (smallSignal_from_mode (prepared s) i j theta hfreq gauge (by simpa only [inv_inv] using hmode))

end DSP.SquiggleBoyQuadReverb.Frozen

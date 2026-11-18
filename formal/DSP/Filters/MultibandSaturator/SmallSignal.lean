import DSP.Filters.MultibandSaturator.Contracts
import DSP.Filters.LinkwitzRileySplitter.Settling
import DSP.ResponseMeasurement.StaticObservation

noncomputable section
namespace DSP.MultibandSaturator
open Filter ResponseMeasurement
open scoped Topology BigOperators

def Snapshot.wave {n : Nat} (s : Snapshot n ℝ) (frequency : ℝ) : C(Phase,Fin n → ℝ) :=
  ⟨fun phase i => (s.splitter.bandResponse frequency i * fourier 1 phase).re,by fun_prop⟩

theorem linear_wave {n : Nat} (s : Snapshot n ℝ) (frequency : ℝ) (k : ℕ) :
    SaturatingSum.tangent s.bandGain s.masterGain (s.wave frequency (k • (frequency : Phase))) =
      (s.transfer frequency * (fourier 1 (frequency : Phase))^k).re := by
  change SaturatingSum.tangent _ _ (fun i =>
    (s.splitter.bandResponse frequency i * fourier 1 (k • (frequency : Phase))).re) = _
  rw [SaturatingSum.tangent_real,fourier_orbit]
  rfl

/-- Fixed valid tuning, zero initial histories, time first and amplitude second.
The measured output is the actual processor's master saturation output. -/
theorem prepared_smallSignal {n : Nat} [NeZero n] (spec : LogicalSpec n ℝ) (initial : State n ℝ)
    (hv : LinkwitzRileySplitter.ValidSpec _ spec.splitter.tree) (frequency : ℝ)
    (hfrequency : star (fourier 1 (frequency : Phase))*star (fourier 1 (frequency : Phase)) ≠ 1) :
    let s := computeCoefficients spec (reset initial)
    HasSmallSignalResponse (fourier 1 (frequency : Phase))
      (fun e k => (Processor.run step (fun i => ⟨e*((fourier 1 (frequency : Phase))^i).re⟩) s k).master.after)
      ((prepare spec).snapshot.transfer frequency) := by
  dsimp only
  let s := computeCoefficients spec (reset initial)
  let snap := (prepare spec).snapshot
  let z := fourier 1 (frequency : Phase)
  have hz : ‖z‖=1 := Circle.norm_coe _
  have hg : s.bandGain = snap.bandGain ∧ s.masterGain = snap.masterGain := ⟨rfl,rfl⟩
  have hs : s.splitter = LinkwitzRileySplitter.computeCoefficients spec.splitter (LinkwitzRileySplitter.reset initial.splitter) :=
    prepared_splitter spec (reset initial)
  have hsnap : snap.splitter = LinkwitzRileySplitter.makeSnapshot spec.splitter := by
    exact congrArg Snapshot.splitter (prepared_snapshot spec)
  apply smallSignal_of_static_observation (frequency : Phase) (snap.wave frequency)
    (SaturatingSum.lipschitz snap.bandGain snap.masterGain).continuous
    (SaturatingSum.apply_zero _ _) (SaturatingSum.tangent _ _)
    (SaturatingSum.hasFDerivAt_zero _ _) (snap.transfer frequency)
  · have h := coherent_sinusoid_tendsto z (snap.transfer frequency) hz hfrequency
    simpa only [linear_wave] using h
  · intro e he
    let bands := fun k => (Step.states LinkwitzRileySplitter.step (fun i => ⟨e*(z^i).re⟩) s.splitter (k+1)).bands
    have hb : Tendsto (fun k => bands k-e • snap.wave frequency (k • (frequency : Phase))) atTop (𝓝 0) := by
      apply tendsto_pi_nhds.mpr
      intro i
      have h := LinkwitzRileySplitter.computeCoefficients_settles_band spec.splitter
        (LinkwitzRileySplitter.reset initial.splitter) hv (LinkwitzRileySplitter.resetTree_atRest _ _)
        frequency i (fun k => e*(z^k).re) (e:ℂ)
        (by simp [Tracks,Complex.mul_re,z])
      rw [← hs,← hsnap] at h
      change Tendsto (fun k => bands k i-e *
        (snap.splitter.bandResponse frequency i * fourier 1 (k • (frequency : Phase))).re) atTop (𝓝 0)
      simp only [fourier_orbit]
      simpa [Tracks,observed,bands,z,Complex.mul_re,Complex.mul_im,mul_assoc,
        mul_left_comm,mul_comm] using h
    have ho := lipschitz_tracks (SaturatingSum.lipschitz snap.bandGain snap.masterGain) _ _ hb
    apply ho.congr
    intro k
    rw [output_eq,hg.1,hg.2]

/-- The same snapshot used in `prepared_smallSignal` supplies the executable
power kernel; its value is the squared magnitude of that proved response. -/
theorem prepared_magnitudeSquared {n : Nat} (spec : LogicalSpec n ℝ)
    (hv : LinkwitzRileySplitter.ValidSpec _ spec.splitter.tree) (frequency : ℝ) :
    (prepare spec).snapshot.magnitudeSquared (ResponseFrequency.fromHalfAngle frequency
      (Real.sin (Real.pi*frequency)) (Real.cos (Real.pi*frequency))) =
      Complex.normSq ((prepare spec).snapshot.transfer frequency) := by
  apply Snapshot.magnitudeSquared_eq_normSq _ _ _ rfl rfl
  rw [prepared_snapshot]
  exact LinkwitzRileySplitter.snapshotTree_valid _ spec.splitter.tree hv

end DSP.MultibandSaturator

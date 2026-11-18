import DSP.Filters.MultibandSaturator.MonoBass

noncomputable section
namespace DSP.MultibandSaturator
open Filter ResponseMeasurement
open scoped Topology BigOperators
variable {n c : Nat}

def Snapshot.lane (s : Snapshot n (Fin c → ℝ)) (j : Fin c) : Snapshot n ℝ :=
  ⟨Channels.channel j s.splitter,fun i => s.bandGain i j,s.masterGain j⟩

/-- The complex input amplitudes include relative channel phase. In particular,
opposite-phase stereo bass cancels before its saturation. -/
def Snapshot.monoTransfer (s : Snapshot n (Fin c → ℝ)) (frequency : ℝ)
    (amplitudes : Fin c → ℂ) (lane : Fin c) : ℂ :=
  (s.masterGain lane:ℂ) * ∑ i, (s.bandGain i lane:ℂ) *
    bassRoute ℂ lane (fun i j => (s.lane j).splitter.bandResponse frequency i * amplitudes j) i

/-- Explicit channel coupling: only the lowest band takes the channel mean. -/
theorem Snapshot.monoTransfer_eq (s : Snapshot n (Fin c → ℝ)) (frequency : ℝ)
    (amplitudes : Fin c → ℂ) (lane : Fin c) :
    s.monoTransfer frequency amplitudes lane = (s.masterGain lane:ℂ) * ∑ i,
      if i.val=0 then (s.bandGain i lane:ℂ) *
        (∑ j, (s.lane j).splitter.bandResponse frequency i * amplitudes j)/(c:ℂ)
      else (s.bandGain i lane:ℂ) * (s.lane lane).splitter.bandResponse frequency i * amplitudes lane := by
  simp only [monoTransfer,bassRoute,LinearMap.coe_mk,AddHom.coe_mk,mul_ite,mul_div_assoc,mul_assoc]

/-- Identical channel tuning and a common input mode recover the scalar response.
Band and master gains are still allowed to vary across the output channels. -/
theorem Snapshot.monoTransfer_common [NeZero c] (s : Snapshot n (Fin c → ℝ))
    (frequency : ℝ) (lane : Fin c)
    (h : ∀ j, (s.lane j).splitter = (s.lane lane).splitter) (amplitude : ℂ) :
    s.monoTransfer frequency (fun _ => amplitude) lane = (s.lane lane).transfer frequency*amplitude := by
  have hc : (c:ℂ) ≠ 0 := by exact_mod_cast NeZero.ne c
  simp only [monoTransfer_eq,h,Finset.sum_const,Finset.card_univ,Fintype.card_fin,nsmul_eq_mul]
  have he (i : Fin n) : (s.bandGain i lane:ℂ) *
      ((c:ℂ)*((s.lane lane).splitter.bandResponse frequency i*amplitude))/(c:ℂ) =
      (s.bandGain i lane:ℂ)*(s.lane lane).splitter.bandResponse frequency i*amplitude := by
    field_simp
  simp only [he,ite_self]
  simp only [Snapshot.transfer,Snapshot.lane,Finset.sum_mul,mul_assoc]

/-- With identical tuning, any zero-mean channel mode loses its bass contribution.
Stereo anti-phase input is the usual example. -/
theorem Snapshot.monoTransfer_zeroMean (s : Snapshot n (Fin c → ℝ)) (frequency : ℝ)
    (amplitudes : Fin c → ℂ) (lane : Fin c)
    (h : ∀ j, (s.lane j).splitter = (s.lane lane).splitter) (ha : ∑ j, amplitudes j=0) :
    s.monoTransfer frequency amplitudes lane = (s.masterGain lane:ℂ) * ∑ i,
      if i.val=0 then 0 else
        (s.bandGain i lane:ℂ)*(s.lane lane).splitter.bandResponse frequency i*amplitudes lane := by
  simp only [monoTransfer_eq,h,← Finset.mul_sum,ha,mul_zero,zero_div]

def Snapshot.channelWave (s : Snapshot n (Fin c → ℝ)) (frequency : ℝ)
    (amplitudes : Fin c → ℂ) : C(Phase,Fin n → Fin c → ℝ) :=
  ⟨fun phase i j => ((s.lane j).splitter.bandResponse frequency i * amplitudes j * fourier 1 phase).re,
    by fun_prop⟩

theorem mono_linear_wave (s : Snapshot n (Fin c → ℝ)) (frequency : ℝ)
    (amplitudes : Fin c → ℂ) (lane : Fin c) (k : ℕ) :
    ((SaturatingSum.tangent (s.lane lane).bandGain (s.masterGain lane)).comp
      (bassRouteContinuous lane)) (s.channelWave frequency amplitudes (k • (frequency:Phase))) =
    (s.monoTransfer frequency amplitudes lane * (fourier 1 (frequency:Phase))^k).re := by
  change SaturatingSum.tangent _ _ (bassRoute ℝ lane (fun i j =>
    ((s.lane j).splitter.bandResponse frequency i * amplitudes j * fourier 1 (k • (frequency:Phase))).re)) = _
  rw [bassRoute_real,SaturatingSum.tangent_real,fourier_orbit]
  rfl

/-- Time-first response of the actual mono-bass processor, for any complex
amplitude vector and valid per-channel tuning. Gains may also differ by lane. -/
theorem prepared_monoSmallSignal [NeZero n] [NeZero c]
    (spec : LogicalSpec n (Fin c → ℝ)) (initial : State n (Fin c → ℝ))
    (hv : ∀ j, LinkwitzRileySplitter.ValidSpec _ (Channels.channel j spec.splitter).tree)
    (frequency : ℝ)
    (hfrequency : star (fourier 1 (frequency:Phase))*star (fourier 1 (frequency:Phase)) ≠ 1)
    (amplitudes : Fin c → ℂ) (lane : Fin c) :
    let s := computeCoefficients spec (reset initial)
    HasSmallSignalResponse (fourier 1 (frequency:Phase))
      (fun e k => (Processor.run stepMonoBass
        (fun i => ⟨fun j => e*(amplitudes j*(fourier 1 (frequency:Phase))^i).re⟩) s k).master.after lane)
      ((prepare spec).snapshot.monoTransfer frequency amplitudes lane) := by
  dsimp only
  let s := computeCoefficients spec (reset initial)
  let snap := (prepare spec).snapshot
  let z := fourier 1 (frequency:Phase)
  let route := bassRouteContinuous (n := n) lane
  let out := fun x => SaturatingSum.apply (snap.lane lane).bandGain (snap.masterGain lane) (route x)
  have hc := (SaturatingSum.lipschitz (snap.lane lane).bandGain (snap.masterGain lane)).comp route.lipschitz
  have hz : out 0=0 := by simp [out,SaturatingSum.apply_zero]
  have hd := (SaturatingSum.hasFDerivAt_zero (snap.lane lane).bandGain (snap.masterGain lane))
  have hd' : HasFDerivAt out
      ((SaturatingSum.tangent (snap.lane lane).bandGain (snap.masterGain lane)).comp route) 0 := by
    have hd_at : HasFDerivAt (SaturatingSum.apply (snap.lane lane).bandGain (snap.masterGain lane))
        (SaturatingSum.tangent (snap.lane lane).bandGain (snap.masterGain lane)) (route 0) := by
      simpa only [map_zero] using hd
    exact hd_at.comp 0 route.hasFDerivAt
  apply smallSignal_of_static_observation (frequency:Phase) (snap.channelWave frequency amplitudes)
    hc.continuous hz _ hd' (snap.monoTransfer frequency amplitudes lane)
  · simpa only [route,mono_linear_wave] using coherent_sinusoid_tendsto z
      (snap.monoTransfer frequency amplitudes lane) (Circle.norm_coe _) hfrequency
  · intro e he
    let u : ℕ → Sample (Fin c → ℝ) := fun i => ⟨fun j => e*(amplitudes j*z^i).re⟩
    let bands := fun k => (Step.states LinkwitzRileySplitter.step u s.splitter (k+1)).bands
    have hs (j : Fin c) : Channels.channel j s.splitter =
        LinkwitzRileySplitter.computeCoefficients (Channels.channel j spec.splitter)
          (LinkwitzRileySplitter.reset (Channels.channel j initial.splitter)) := by
      change Channels.channel j (LinkwitzRileySplitter.installPrepared (LinkwitzRileySplitter.prepare spec.splitter)
        (LinkwitzRileySplitter.reset initial.splitter)) = _
      rw [LinkwitzRileySplitter.install_prepare,LinkwitzRileySplitter.computeCoefficients_channel,
        LinkwitzRileySplitter.reset_channel]
    have hsnap (j : Fin c) : (snap.lane j).splitter =
        LinkwitzRileySplitter.makeSnapshot (Channels.channel j spec.splitter) := by
      change Channels.channel j (LinkwitzRileySplitter.prepare spec.splitter).snapshot = _
      rw [LinkwitzRileySplitter.prepare_snapshot,LinkwitzRileySplitter.makeSnapshot_channel]
    have hband (k : ℕ) (i : Fin n) (j : Fin c) : bands k i j =
        (Step.states LinkwitzRileySplitter.step (fun k => ⟨e*(amplitudes j*z^k).re⟩)
          (Channels.channel j s.splitter) (k+1)).bands i := by
      exact congrArg (fun v => v.bands i) (Step.states_map LinkwitzRileySplitter.step LinkwitzRileySplitter.step
        (Channels.channel j) (fun _ x => Channels.channel j x) (LinkwitzRileySplitter.natural j) u s.splitter (k+1))
    have hb : Tendsto (fun k => bands k-e • snap.channelWave frequency amplitudes (k • (frequency:Phase))) atTop (𝓝 0) := by
      apply tendsto_pi_nhds.mpr
      intro i
      apply tendsto_pi_nhds.mpr
      intro j
      have h := LinkwitzRileySplitter.computeCoefficients_settles_band (Channels.channel j spec.splitter)
        (LinkwitzRileySplitter.reset (Channels.channel j initial.splitter)) (hv j)
        (LinkwitzRileySplitter.resetTree_atRest _ _) frequency i (fun k => e*(amplitudes j*z^k).re)
        ((e:ℂ)*amplitudes j) (by simp [Tracks,z,Complex.mul_re,Complex.mul_im,mul_assoc,mul_sub])
      rw [← hs j,← hsnap j] at h
      change Tendsto (fun k => bands k i j-e *
        ((snap.lane j).splitter.bandResponse frequency i * amplitudes j * fourier 1 (k • (frequency:Phase))).re) atTop (𝓝 0)
      simp only [hband,fourier_orbit]
      simpa [Tracks,observed,z,Complex.mul_re,Complex.mul_im,mul_assoc,mul_left_comm,mul_comm,
        mul_sub,sub_mul] using h
    have ho := lipschitz_tracks hc _ _ hb
    apply ho.congr
    intro k
    rw [monoBass_output_eq]
    rfl

end DSP.MultibandSaturator

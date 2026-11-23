import DSP.Filters.NaiveReuppingLadder.PreparedPeriodic

noncomputable section
namespace DSP.NaiveReuppingLadder.PeriodicWave.Setup
open DetectorDynamics BandPassedEnvelopeFollower.Periodic Metric
open scoped NNReal Topology
variable (s : Setup)

/-- Finite, phase-by-phase sufficient conditions for a periodic waveform near
a proposed fourth-stage trace. The detector denominator is tested on the
candidate with an explicit allowance for every waveform in the ball.
All filter coefficients, candidate traces, and margins can vary over a family. -/
theorem local_waveform (c : s.Wave) (r M pMin dMin : ℝ) (hr : 0 ≤ r) (hM : 0 ≤ M)
    (hp : 0 ≤ pMin) (hd : 0 < dMin) (hcap : ∀ i, |c i|+r ≤ M)
    (hpower : ∀ i : Fin s.N, pMin ≤ (s.dry i).2.2)
    (hden : ∀ i : Fin s.N, dMin ≤ s.denominator i ((s.detector c i).2.2-s.envelopeGain M*r))
    (hg : s.localGain M pMin dMin < 1)
    (hres : dist (s.update c) c + s.localGain M pMin dMin*r ≤ r) :
    ∃ b ∈ closedBall c r, s.update b = b ∧
      (∀ w ∈ closedBall c r, s.update w = w → w = b) ∧
      (∀ w ∈ closedBall c r, ∀ n, dist (s.update^[n] w) b ≤ (s.localGain M pMin dMin)^n*dist w b) ∧
      dist c b ≤ dist c (s.update c)/(1-s.localGain M pMin dMin) ∧
      (∀ n, prefixRun s.k s.g s.a s.u (s.assemble b 0) n = s.assemble b n) := by
  have hE := s.envelopeGain_nonneg M hM
  have h0 : 0 ≤ s.localGain M pMin dMin := by
    unfold localGain
    exact mul_nonneg (pow_nonneg s.S_nonneg _) (add_nonneg (div_nonneg s.feedback hd.le)
      (mul_nonneg (mul_nonneg hM (div_nonneg (mul_nonneg s.feedback s.damping)
        (mul_nonneg (add_nonneg hp floor_pos.le) (sq_nonneg _)))) hE))
  let K : ℝ≥0 := ⟨_,h0⟩
  have bound (w : s.Wave) (hw : w ∈ closedBall c r) (j : Fin s.N) : |w j| ≤ M := by
    have he : |w j-c j| ≤ r := (dist_pi_le_iff hr).mp hw j
    have h := abs_add_le (w j-c j) (c j)
    rw [sub_add_cancel] at h
    linarith [hcap j]
  have cc : c ∈ closedBall c r := mem_closedBall_self hr
  have hlo (i : ℕ) : pMin ≤ (s.dry i).2.2 := by
    rw [← s.dry_period.map_mod_nat i]
    exact hpower ⟨i%s.N,Nat.mod_lt _ s.positivePeriod⟩
  have denom (w : s.Wave) (hw : w ∈ closedBall c r) (i : ℕ) : dMin ≤ s.denominator i (s.detector w i).2.2 := by
    have hq := (s.detector_difference c w M (bound c cc) (bound w hw) i).trans
      (mul_le_mul_of_nonneg_left (show dist c w ≤ r by rw [dist_comm]; exact hw) hE)
    have hqc : (s.detector c i).2.2-s.envelopeGain M*r ≤ (s.detector w i).2.2 := by linarith [(abs_le.mp hq).2]
    let j : Fin s.N := ⟨i%s.N,Nat.mod_lt _ s.positivePeriod⟩
    have h := hden j
    change dMin ≤ s.denominator (i%s.N) ((s.detector c (i%s.N)).2.2-s.envelopeGain M*r) at h
    simp only [denominator,s.dry_period.map_mod_nat,(s.detector_period c).map_mod_nat] at h
    refine h.trans (add_le_add le_rfl (mul_le_mul_of_nonneg_left (div_le_div_of_nonneg_right
      (max_le_max le_rfl (sub_le_sub_right hqc _)) (add_nonneg (s.dry_nonneg i) floor_pos.le)) s.damping))
  have hl : LipschitzOnWith K s.update (closedBall c r) := lipschitzOnWith_iff_dist_le_mul.mpr
    (fun w hw v hv => s.update_difference M pMin dMin hM hp hd hlo w v (bound w hw) (bound v hv)
      (denom w hw) (denom v hv))
  obtain ⟨b,hb,hfix,hunique,_,herr⟩ := AttractingBlock.exists_attracting_fixedPoint s.update c r K hr hg hl hres
  exact ⟨b,hb,hfix,hunique,fun w hw => AttractingBlock.iterate_error s.update c r K hr hl hres b w hb hfix hw,
    herr,s.assemble_run b hfix⟩

/-- Any complex amplitude sets both sine level and phase, and any root of
unity sets an exactly repeating sampled frequency. -/
theorem tone_periodic (A z : ℂ) (N : ℕ) (hz : z^N = 1) :
    Function.Periodic (fun n : ℕ => (A*z^n).re) N := fun i => by simp [pow_add,hz]

/-- Both measurements are exact one-period sums for the physical initialization
returned by the waveform solver. This also applies to any independently found
waveform satisfying the same fixed-point equation. -/
theorem measured_orbit (b : s.Wave) (hb : s.update b = b) (template : NaiveReuppingLadder ℝ)
    (hk : knobs template = s.k) (hd : Tuned template.inputEnvelope s.g s.a)
    (hw : Tuned template.feedbackEnvelope s.g s.a) (z : ℂ) (hz : z^s.N = 1) :
    let initial := installHistory template (s.assemble b 0)
    let y := fun n => (run initial s.u (n+1)).output
    ResponseMeasurement.HasCoherentResponse z y (ResponseMeasurement.coherent z y s.N) ∧
    ResponseMeasurement.HasMeanSquare y (ResponseMeasurement.meanSquare y s.N) := by
  let initial := installHistory template (s.assemble b 0)
  let y := fun n => (run initial s.u (n+1)).output
  have hout := (s.physical_orbit b hb template hk hd hw).2
  have hy : Function.Periodic y s.N := by
    intro i
    change (run initial s.u ((i+s.N)+1)).output = (run initial s.u (i+1)).output
    rw [hout,hout,s.periodic i,s.assemble_period b i]
  exact ⟨ResponseMeasurement.coherent_of_periodic z y s.N s.positivePeriod hz hy,
    ResponseMeasurement.meanSquare_of_periodic y s.N s.positivePeriod hy⟩

end DSP.NaiveReuppingLadder.PeriodicWave.Setup

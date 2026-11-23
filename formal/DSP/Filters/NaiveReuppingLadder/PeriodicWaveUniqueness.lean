import DSP.Filters.NaiveReuppingLadder.PeriodicWaveOrbit

noncomputable section
namespace DSP.NaiveReuppingLadder.PeriodicWave.Setup
open DetectorDynamics BandPassedEnvelopeFollower.Periodic Filter
open scoped Topology
variable (s : Setup)

private theorem unique_detector (u : ℕ → ℝ) (hu : Function.Periodic u s.N)
    (d : ℕ → Memory) (hd : Function.Periodic d s.N)
    (hstep : ∀ i, d (i+1) = advance s.g s.a (d i) (u i)) (n : ℕ) :
    d n = orbit s.g s.a u s.N n := by
  have he (j : ℕ) : Step.states (advance s.g s.a) u (d 0) j = d j := by
    induction j with
    | zero => rfl
    | succ j ih => rw [Step.states_succ, ih, ← hstep]
  have hr := orbit_periodic s.g s.a s.detectorGain s.detectorAlpha s.detectorAlpha_le u s.N s.positivePeriod hu
  have h := tracks_orbit s.g s.a s.detectorGain s.detectorAlpha s.detectorAlpha_le u s.N s.positivePeriod hu (d 0)
  have hj : Tendsto (fun j : ℕ => j*s.N+n) atTop atTop :=
    tendsto_atTop_mono (fun j => (Nat.le_mul_of_pos_right j s.positivePeriod).trans (Nat.le_add_right _ _)) tendsto_id
  have hx (j : ℕ) : d (j*s.N+n) = d n := by simpa [Nat.add_comm] using hd.nat_mul j n
  have hy (j : ℕ) : orbit s.g s.a u s.N (j*s.N+n) = orbit s.g s.a u s.N n := by
    simpa [Nat.add_comm] using hr.nat_mul j n
  have hh := h.comp hj
  simp only [Function.comp_def, he, hx, hy] at hh
  exact sub_eq_zero.mp (tendsto_nhds_unique tendsto_const_nhds hh)

private theorem smooth_unique (v x : ℕ → ℝ) (hv : Function.Periodic v s.N)
    (hx : Function.Periodic x s.N)
    (hstep : ∀ i, v (i+1) = OnePoleZOHLowPass.onePoleStep s.k.alpha (x i) (v i)) :
    v = s.smooth x := by
  funext n
  apply sub_eq_zero.mp
  apply abs_nonpos_iff.mp
  apply smooth_bound _ s.alpha s.alpha_le (fun i => v i - s.smooth x i) _ s.positivePeriod
    (fun i => by simp only [hv i,s.smooth_period x hx i]) 0 _ n
  intro i
  change |v (i+1) - ((1-s.k.alpha)*s.smooth x i+s.k.alpha*x i)| ≤ _
  rw [hstep,OnePoleZOHLowPass.onePoleStep_eq,fma_eq]
  rw [show s.k.alpha*x i+(1-s.k.alpha)*v i-((1-s.k.alpha)*s.smooth x i+s.k.alpha*x i) =
      (1-s.k.alpha)*(v i-s.smooth x i) by ring]
  simp [abs_mul,abs_of_nonneg (sub_nonneg.mpr s.alpha_le)]

/-- Every input-periodic trajectory, including a repelling one, is represented
by the fourth-stage waveform fixed-point equation. No small-gain assumption. -/
theorem fixed_wave_of_orbit (z : Joint) (hz : prefixRun s.k s.g s.a s.u z s.N = z) :
    ∃ b : s.Wave, s.update b = b ∧ s.assemble b 0 = z := by
  let t := prefixRun s.k s.g s.a s.u z
  have ht : Function.Periodic t s.N := fun i => by
    rw [Nat.add_comm]
    exact Step.states_periodic_of_fixed _ s.u s.N (fun j => by simpa [Nat.add_comm] using s.periodic j) z hz i
  let b : s.Wave := fun j => (t j).2.1.2.2.2
  have hb (n : ℕ) : s.waveAt b n = (t n).2.1.2.2.2 :=
    congrArg (fun w : Joint => w.2.1.2.2.2) (ht.map_mod_nat n)
  have hd (n : ℕ) : (t n).1 = s.dry n :=
    s.unique_detector s.u s.periodic (fun j => (t j).1) (fun i => congrArg Prod.fst (ht i)) (fun _ => rfl) n
  have hw (n : ℕ) : (t n).2.2 = s.detector b n := by
    apply s.unique_detector (fun j => s.waveAt b (j+1)) (s.shift_period (s.waveAt_period b)) (fun j => (t j).2.2) (fun i => congrArg (fun z : Joint => z.2.2) (ht i)) _ n
    intro i
    rw [hb (i+1)]
    rfl
  let A := fun n => (t n).2.1
  have hA (n : ℕ) : A (n+1) = audio s.k (A n) (s.u n) (s.feedbackAt b n) := by
    change audio s.k (A n) (s.u n) (reduction _ _ _ (max 0 (t n).1.2.2) (t n).2.2.2.2) = _
    rw [hd,hw,max_eq_right (s.dry_nonneg n)]
    rfl
  have h₁ : (fun n => (A n).1) = s.first b := by
    apply s.smooth_unique _ _ (fun i => congrArg (fun w : Joint => w.2.1.1) (ht i)) (s.drive_period b)
    intro i
    rw [hA]
    change OnePoleZOHLowPass.onePoleStep _ _ _ = OnePoleZOHLowPass.onePoleStep _ (s.driveIn b i) _
    simp only [driveIn,hb,A]
  have next (v w : ℕ → ℝ) (hv : Function.Periodic v s.N) (hw' : Function.Periodic w s.N)
      (h : ∀ i, w (i+1) = OnePoleZOHLowPass.onePoleStep s.k.alpha
        (NormalizedTanh.apply s.k.sat (v (i+1))) (w i)) : w = s.chain v :=
    s.smooth_unique w _ hw' (fun i => congrArg (NormalizedTanh.apply s.k.sat) (s.shift_period hv i)) h
  have h₂ := next (fun n => (A n).1) (fun n => (A n).2.1)
    (fun i => congrArg (fun w : Joint => w.2.1.1) (ht i))
    (fun i => congrArg (fun w : Joint => w.2.1.2.1) (ht i)) (fun i => by rw [hA]; rfl)
  have h₃ := next (fun n => (A n).2.1) (fun n => (A n).2.2.1)
    (fun i => congrArg (fun w : Joint => w.2.1.2.1) (ht i))
    (fun i => congrArg (fun w : Joint => w.2.1.2.2.1) (ht i)) (fun i => by rw [hA]; rfl)
  have h₄ := next (fun n => (A n).2.2.1) (fun n => (A n).2.2.2)
    (fun i => congrArg (fun w : Joint => w.2.1.2.2.1) (ht i))
    (fun i => congrArg (fun w : Joint => w.2.1.2.2.2) (ht i)) (fun i => by rw [hA]; rfl)
  rw [h₁] at h₂
  rw [h₂] at h₃
  rw [h₃] at h₄
  refine ⟨b,funext (fun i => (congrFun h₄ (i:ℕ)).symm),?_⟩
  change s.assemble b 0 = t 0
  exact Prod.ext (hd 0).symm (Prod.ext (Prod.ext (congrFun h₁ 0).symm
    (Prod.ext (congrFun h₂ 0).symm (Prod.ext (congrFun h₃ 0).symm (congrFun h₄ 0).symm))) (hw 0).symm)

/-- The symbolic small-gain region has exactly one repeating physical memory
at each fixed input phase. Other periods and attraction in audio time are
separate questions. -/
theorem unique_periodic_history (pMin : ℝ) (hp : 0 ≤ pMin) (hlow : ∀ n, pMin ≤ (s.dry n).2.2)
    (h : s.L pMin < 1) : ∃! z : Joint, prefixRun s.k s.g s.a s.u z s.N = z := by
  obtain ⟨b,hb,hunique,_,hr⟩ := s.small_gain_orbit pMin hp hlow h
  refine ⟨s.assemble b 0, (hr s.N).trans (by simpa using s.assemble_period b 0),?_⟩
  intro z hz
  obtain ⟨c,hc,hz'⟩ := s.fixed_wave_of_orbit z hz
  rw [← hz',hunique c hc]

end DSP.NaiveReuppingLadder.PeriodicWave.Setup

import DSP.Filters.NaiveReuppingLadder.DetectorDynamics
import DSP.Filters.BandPassedEnvelopeFollower.PeriodicTracking
import DSP.Analysis.SettlingBlock
import DSP.ResponseMeasurement.Periodic

noncomputable section
namespace DSP.NaiveReuppingLadder.DetectorDynamics
open BandPassedEnvelopeFollower.Periodic Filter Metric
open scoped Topology NNReal

abbrev Joint := Memory × Wet

/-- Continuous extension to arbitrary trial coordinates. Physical dry power
is already nonnegative, including reset. Wet power still uses the actual knee. -/
def jointStep (k : Knobs) (g a : ℝ) (z : Joint) (x : ℝ) : Joint :=
  (advance g a z.1 x, advanceWet k g a z.2 (x,max 0 z.1.2.2))

abbrev history (s : NaiveReuppingLadder ℝ) : Joint := (memory s.inputEnvelope, wet s)

def installHistory (s : NaiveReuppingLadder ℝ) (z : Joint) : NaiveReuppingLadder ℝ :=
  installWet {s with inputEnvelope := install s.inputEnvelope z.1} z.2

theorem history_install (s : NaiveReuppingLadder ℝ) (z : Joint) :
    history (installHistory s z) = z := by simp [history,installHistory,installWet,wet]

theorem physical_run (s : NaiveReuppingLadder ℝ) (g a : ℝ)
    (hd : Tuned s.inputEnvelope g a) (hw : Tuned s.feedbackEnvelope g a)
    (ha : 0 ≤ a) (ha1 : a ≤ 1) (hp : 0 ≤ s.inputEnvelope.power) (u : ℕ → ℝ) (n : ℕ) :
    history (run s u n) = Step.states (jointStep (knobs s) g a) u (history s) n := by
  have ht (i : ℕ) : Tuned (run s u i).inputEnvelope g a ∧ Tuned (run s u i).feedbackEnvelope g a ∧
      0 ≤ (run s u i).inputEnvelope.power := by
    induction i with
    | zero => exact ⟨hd,hw,hp⟩
    | succ i ih =>
      exact ⟨tuned_step _ _ _ _ ih.1,tuned_step _ _ _ _ ih.2.1,
        Exploration.detector_power_nonnegative _ _ (ih.1.2.2.2.2.symm ▸ ha)
          (ih.1.2.2.2.2.symm ▸ ha1) ih.2.2⟩
  induction n with
  | zero => rfl
  | succ n ih =>
    rw [Step.states_succ, ← ih]
    change (_,wet (step (run s u n) ⟨u n⟩)) = (_, _)
    dsimp only [history,memory, Prod.fst, Prod.snd]
    apply Prod.ext
    · exact memory_step _ _ _ _ (ht n).1
    · rw [wet_step _ _ _ _ (ht n).2.1, knobs_run, max_eq_right (ht n).2.2]

def readout (k : Knobs) (z : Joint) (x : ℝ) := observe k z.2 (x,max 0 z.1.2.2)

theorem physical_readout (s : NaiveReuppingLadder ℝ) (x : ℝ) (hp : 0 ≤ s.inputEnvelope.power) :
    (s.step ⟨x⟩).output = readout (knobs s) (history s) x := by
  rw [output_step]
  simp only [readout, memory, max_eq_right hp]

@[fun_prop] private theorem firstState_cont :
    Continuous TPTSVF.Convergence.firstState := by unfold TPTSVF.Convergence.firstState; fun_prop

@[fun_prop] private theorem advance_cont (g a : ℝ) :
    Continuous (fun z : Memory × ℝ => advance g a z.1 z.2) := by
  unfold advance tap modalStep
  fun_prop

private theorem feedback_cont (k : Knobs) (hd : 0 ≤ k.D) :
    Continuous (fun z : ℝ × ℝ => reduction k.K k.D k.R (max 0 z.1) z.2) := by
  unfold reduction
  have hn (x : ℝ × ℝ) : 0 < max 0 x.1 + (powerFloor : ℝ) :=
    add_pos_of_nonneg_of_pos (le_max_left _ _) floor_pos
  apply continuous_const.div
  · fun_prop (disch := exact fun _ => (hn _).ne')
  · intro x
    exact (add_pos_of_pos_of_nonneg zero_lt_one
      (mul_nonneg hd (div_nonneg (le_max_left _ _) (hn x).le))).ne'

private theorem audio_cont (k : Knobs) :
    Continuous (fun z : Audio × ℝ × ℝ => audio k z.1 z.2.1 z.2.2) := by
  have hs := (NormalizedTanh.contDiff_apply k.sat).continuous
  simp only [audio, OnePoleZOHLowPass.onePoleStep_eq, fma_eq]
  fun_prop

theorem continuous_step (k : Knobs) (hd : 0 ≤ k.D) (g a x : ℝ) :
    Continuous (fun z => jointStep k g a z x) := by
  have hf : Continuous (fun z : Joint => reduction k.K k.D k.R (max 0 z.1.2.2) z.2.2.2.2) :=
    (feedback_cont k hd).comp (by fun_prop : Continuous (fun z : Joint => (z.1.2.2,z.2.2.2.2)))
  let v := fun z : Joint => audio k z.2.1 x (reduction k.K k.D k.R (max 0 z.1.2.2) z.2.2.2.2)
  have hv : Continuous v := (audio_cont k).comp
    (continuous_snd.fst.prodMk (continuous_const.prodMk hf))
  exact ((advance_cont g a).comp (continuous_fst.prodMk continuous_const)).prodMk
    (hv.prodMk ((advance_cont g a).comp (continuous_snd.snd.prodMk hv.snd.snd.snd)))

theorem readout_cont (k : Knobs) (hd : 0 ≤ k.D) (x : ℝ) :
    Continuous (fun z => readout k z x) := by
  have hf := (feedback_cont k hd).comp
    (by fun_prop : Continuous (fun z : Joint => (z.1.2.2,z.2.2.2.2)))
  have hv := (audio_cont k).comp (continuous_snd.fst.prodMk ((continuous_const (y := x)).prodMk hf))
  exact ((NormalizedTanh.contDiff_apply k.sat).continuous.comp hv.snd.snd.snd).mul (hf.add_const 1)

abbrev prefixRun (k : Knobs) (g a : ℝ) (u : ℕ → ℝ) := Step.states (jointStep k g a) u

theorem dry_prefix (k : Knobs) (g a : ℝ) (u : ℕ → ℝ) (z : Joint) (n : ℕ) :
    (prefixRun k g a u z n).1 = Step.states (advance g a) u z.1 n :=
  Step.states_map _ _ Prod.fst (fun _ x => x) (fun _ _ => rfl) _ _ _

abbrev wetReturn (k : Knobs) (g a : ℝ) (u : ℕ → ℝ) (N : ℕ) (d : Memory) (w : Wet) :=
  (prefixRun k g a u (d,w) N).2

theorem dry_fixed (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (u : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N) :
    Step.states (advance g a) u (orbit g a u N 0) N = orbit g a u N 0 := by
  have h (n : ℕ) : Step.states (advance g a) u (orbit g a u N 0) n = orbit g a u N n := by
    induction n with
    | zero => rfl
    | succ n ih => rw [Step.states_succ, ih, ← orbit_step]
  rw [h]
  simpa using orbit_periodic g a hg ha ha1 u N hN hu 0

theorem dry_block_limit (k : Knobs) (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (u : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (hu : Function.Periodic u N) (z : Joint) :
    Tendsto (fun n => (prefixRun k g a u z (n*N)).1) atTop (𝓝 (orbit g a u N 0)) := by
  have hj : Tendsto (fun n : ℕ => n*N) atTop atTop :=
    tendsto_atTop_mono (fun n => Nat.le_mul_of_pos_right n hN) tendsto_id
  have h := (tracks_orbit g a hg ha ha1 u N hN hu z.1).comp hj
  have hr (n : ℕ) : orbit g a u N (n*N) = orbit g a u N 0 := by
    simpa using (orbit_periodic g a hg ha ha1 u N hN hu).nat_mul n 0
  have hc := h.add_const (orbit g a u N 0)
  simpa only [dry_prefix, Function.comp_def, hr, sub_add_cancel, zero_add] using hc

end DSP.NaiveReuppingLadder.DetectorDynamics

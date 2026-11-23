import DSP.Filters.NaiveReuppingLadder.PeriodicWave

noncomputable section
namespace DSP.NaiveReuppingLadder.PeriodicWave.Setup
open DetectorDynamics BandPassedEnvelopeFollower.Periodic Filter Metric
open scoped NNReal Topology
variable (s : Setup)

def assemble (b : s.Wave) (n : ℕ) : Joint :=
  (s.dry n, (s.first b n,s.chain (s.first b) n,s.chain (s.chain (s.first b)) n,s.fourth b n),
    s.detector b n)

theorem assemble_period (b : s.Wave) : Function.Periodic (s.assemble b) s.N := fun i => by
  simp only [assemble,s.dry_period i,s.first_period b i,s.chain_period _ (s.first_period b) i,
    s.chain_period _ (s.chain_period _ (s.first_period b)) i,s.fourth_period b i,s.detector_period b i]

theorem waveAt_update (b : s.Wave) (i : ℕ) : s.waveAt (s.update b) i = s.fourth b i :=
  (s.fourth_period b).map_mod_nat i

private theorem smooth_step (x : ℕ → ℝ) (n : ℕ) :
    OnePoleZOHLowPass.onePoleStep s.k.alpha (x n) (s.smooth x n) = s.smooth x (n+1) := by
  rw [OnePoleZOHLowPass.onePoleStep_eq,fma_eq]
  exact add_comm _ _

theorem assemble_step (b : s.Wave) (hb : s.update b = b) (n : ℕ) :
    jointStep s.k s.g s.a (s.assemble b n) (s.u n) = s.assemble b (n+1) := by
  have h (i : ℕ) : s.fourth b i = s.waveAt b i := by rw [← s.waveAt_update, hb]
  have h₁ := s.smooth_step (s.driveIn b) n
  have h₂ := s.smooth_step (fun i => NormalizedTanh.apply s.k.sat (s.first b (i+1))) n
  have h₃ := s.smooth_step (fun i => NormalizedTanh.apply s.k.sat (s.chain (s.first b) (i+1))) n
  have h₄ := s.smooth_step (fun i => NormalizedTanh.apply s.k.sat (s.chain (s.chain (s.first b)) (i+1))) n
  change OnePoleZOHLowPass.onePoleStep _ _ _ = s.first b (n+1) at h₁
  change OnePoleZOHLowPass.onePoleStep _ _ (s.chain (s.first b) n) = s.chain (s.first b) (n+1) at h₂
  change OnePoleZOHLowPass.onePoleStep _ _ (s.chain (s.chain (s.first b)) n) = s.chain (s.chain (s.first b)) (n+1) at h₃
  change OnePoleZOHLowPass.onePoleStep _ _ (s.fourth b n) = s.fourth b (n+1) at h₄
  dsimp only [jointStep,advanceWet,assemble,audio]
  rw [max_eq_right (s.dry_nonneg n)]
  dsimp only [first,driveIn,feedbackAt] at h₁
  rw [← h n] at h₁
  change OnePoleZOHLowPass.onePoleStep _ _ (s.first b n) = s.first b (n+1) at h₁
  rw [h₁,h₂,h₃,h₄]
  exact Prod.ext rfl (Prod.ext rfl (congrArg (advance s.g s.a (s.detector b n)) (h (n+1))))

theorem assemble_run (b : s.Wave) (hb : s.update b = b) (n : ℕ) :
    prefixRun s.k s.g s.a s.u (s.assemble b 0) n = s.assemble b n := by
  induction n with
  | zero => rfl
  | succ n ih => exact (congrArg (fun z => jointStep s.k s.g s.a z (s.u n)) ih).trans (s.assemble_step b hb n)

/-- A solver fixed point realizes the complete detector-enabled sampled processor,
including its output and the phase convention used to compute its fundamental. -/
theorem physical_orbit (b : s.Wave) (hb : s.update b = b) (template : NaiveReuppingLadder ℝ)
    (hk : knobs template = s.k) (hd : Tuned template.inputEnvelope s.g s.a)
    (hw : Tuned template.feedbackEnvelope s.g s.a) :
    let initial := installHistory template (s.assemble b 0)
    (∀ n, DetectorDynamics.history (run initial s.u n) = s.assemble b n) ∧
    (∀ n, (run initial s.u (n+1)).output = readout s.k (s.assemble b n) (s.u n)) := by
  let initial := installHistory template (s.assemble b 0)
  have hkn : knobs initial = s.k := hk
  have hmem := history_install template (s.assemble b 0)
  have hrun (n : ℕ) : DetectorDynamics.history (run initial s.u n) = s.assemble b n := by
    rw [physical_run initial s.g s.a (tuned_install _ _ _ _ hd) (tuned_install _ _ _ _ hw)
      s.detectorAlpha.le s.detectorAlpha_le (s.dry_nonneg 0), hkn]
    exact (congrArg (fun z => prefixRun s.k s.g s.a s.u z n) hmem).trans (s.assemble_run b hb n)
  refine ⟨hrun, ?_⟩
  intro n
  have hpower := congrArg (fun z : Joint => z.1.2.2) (hrun n)
  change (run initial s.u n).inputEnvelope.power = (s.dry n).2.2 at hpower
  change ((run initial s.u n).step _).output = _
  rw [physical_readout _ _ (hpower.symm ▸ s.dry_nonneg n),knobs_run,hkn,hrun]

/-- The small-gain test implies an actual periodic solution. From any admissible
trial waveform, one more application of update reduces the error by at least
the stated factor. This geometric rate refers to the waveform solver. -/
theorem small_gain_orbit (pMin : ℝ) (hp : 0 ≤ pMin) (hlow : ∀ i, pMin ≤ (s.dry i).2.2)
    (h : s.L pMin < 1) :
    ∃ b : s.Wave, s.update b = b ∧ (∀ c : s.Wave, s.update c = c → c = b) ∧
      (∀ c : s.Wave, (∀ i, |c i| ≤ s.M) → ∀ n, dist (s.update^[n] c) b ≤ (s.L pMin)^n*dist c b) ∧
      (∀ n, prefixRun s.k s.g s.a s.u (s.assemble b 0) n = s.assemble b n) := by
  let ball := closedBall (0 : s.Wave) s.M
  have hball (b : s.Wave) : b ∈ ball ↔ ∀ i, |b i| ≤ s.M := by
    change dist b 0 ≤ _ ↔ _
    simpa only [Real.dist_eq,Pi.zero_apply,sub_zero] using
      (dist_pi_le_iff (f := b) (g := 0) s.M_pos.le)
  letI : Nonempty ball := ⟨⟨0,mem_closedBall_self s.M_pos.le⟩⟩
  letI : CompleteSpace ball := isClosed_closedBall.completeSpace_coe
  let F : ball → ball := fun b => ⟨s.update b, (hball _).mpr (s.update_bound _)⟩
  have hL : 0 ≤ s.L pMin := by
    unfold L
    have hF := add_pos_of_nonneg_of_pos hp floor_pos
    exact mul_nonneg (pow_nonneg s.S_nonneg _) (add_nonneg s.feedback
      (mul_nonneg (mul_nonneg s.M_pos.le (div_nonneg (mul_nonneg s.feedback s.damping) hF.le))
        (mul_nonneg (mul_nonneg (by norm_num) (pow_nonneg (tapGain_nonneg _ s.detectorGain) _)) s.M_pos.le)))
  let K : ℝ≥0 := ⟨s.L pMin,hL⟩
  have hf : ContractingWith K F := ⟨h,LipschitzWith.of_dist_le_mul (fun x y =>
    s.update_lipschitz pMin hp hlow _ _ ((hball _).mp x.property) ((hball _).mp y.property))⟩
  let p := hf.fixedPoint F
  have fix : s.update p = p := congrArg Subtype.val hf.fixedPoint_isFixedPt
  refine ⟨p,fix,?_,?_,s.assemble_run _ fix⟩
  · intro c hc
    have hmem := (hball c).mpr (fun i => hc ▸ s.update_bound c i)
    have hh : F ⟨c,hmem⟩ = ⟨c,hmem⟩ := Subtype.ext hc
    exact congrArg Subtype.val (hf.fixedPoint_unique hh)
  · intro c hc n
    have hm (j : ℕ) : s.update^[j] c ∈ ball := by
      cases j with
      | zero => exact (hball _).mpr hc
      | succ j => rw [Function.iterate_succ_apply']; exact (hball _).mpr (s.update_bound _)
    induction n with
    | zero => simp
    | succ n ih =>
      rw [Function.iterate_succ_apply']
      calc
        _ = dist (s.update (s.update^[n] c)) (s.update p) := by rw [fix]
        _ ≤ s.L pMin * dist (s.update^[n] c) p :=
          s.update_lipschitz _ hp hlow _ _ ((hball _).mp (hm n)) ((hball _).mp p.property)
        _ ≤ s.L pMin * ((s.L pMin)^n * dist c p) := mul_le_mul_of_nonneg_left ih hL
        _ = _ := by rw [pow_succ]; ring

/-- A readable strict base-feedback range. Its right side is independent of
base feedback at fixed drive, detector tuning, damping and dry stimulus. -/
def feedbackThreshold (pMin : ℝ) :=
  1 / (s.S^3*(1+2*s.k.D*tapGain s.g^4*s.M^2/(pMin+powerFloor)))

theorem threshold_pos (pMin : ℝ) (hp : 0 ≤ pMin) (hgain : s.k.sat.gain ≠ 0) :
    0 < s.feedbackThreshold pMin := by
  have hfloor := add_pos_of_nonneg_of_pos hp floor_pos
  have hS : 0 < s.S := div_pos (abs_pos.mpr hgain) s.satDen
  have hD := s.damping
  unfold feedbackThreshold
  positivity

theorem small_gain_iff_feedback_lt (pMin : ℝ) (hp : 0 ≤ pMin) (hgain : s.k.sat.gain ≠ 0) :
    s.L pMin < 1 ↔ s.k.K < s.feedbackThreshold pMin := by
  let B := s.S^3*(1+2*s.k.D*tapGain s.g^4*s.M^2/(pMin+powerFloor))
  have he : s.L pMin = s.k.K*B := by unfold L B; ring
  have hden : 0 < B := one_div_pos.mp (s.threshold_pos pMin hp hgain)
  rw [he]
  exact (lt_div_iff₀ hden).symm

end DSP.NaiveReuppingLadder.PeriodicWave.Setup

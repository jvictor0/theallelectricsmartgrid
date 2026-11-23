import DSP.Filters.NaiveReuppingLadder.Exploration
import DSP.Filters.BandPassedEnvelopeFollower.Periodic
import DSP.Analysis.NormalizedTanhDerivatives

noncomputable section
namespace DSP.NaiveReuppingLadder.DetectorDynamics
open BandPassedEnvelopeFollower.Periodic

/-- The controller's actual power floor is part of the real model. -/
def reduction (K D R p q : ℝ) : ℝ :=
  K / (1 + D * (max 0 (q-R*p) / (p+powerFloor)))

theorem reduction_eq (s : NaiveReuppingLadder ℝ) :
    effectiveFeedback s = reduction s.baseFeedback s.dampingScale s.referencePowerGain
      s.inputEnvelope.power s.feedbackEnvelope.power := by
  have hm (t : ℝ) : maxValue 0 t = max 0 t := by
    change (if 0 < t then t else 0) = max 0 t
    split <;> rename_i h
    · exact (max_eq_right h.le).symm
    · exact (max_eq_left (le_of_not_gt h)).symm
  simp only [effectiveFeedback, reduction, hm, add_comm (s.dampingScale * _)]

theorem floor_pos : 0 < (powerFloor : ℝ) := by norm_num [powerFloor]

/-- To hold effective feedback below a chosen target, the required excess is
exactly proportional to dry power plus floor. This includes the controller knee. -/
theorem reduction_le_iff (K D R p q target : ℝ) (hd : 0 ≤ D) (hp : 0 ≤ p) :
    reduction K D R p q ≤ target ↔
      (K-target)*(p+powerFloor) ≤ target*D*max 0 (q-R*p) := by
  have hf := add_pos_of_nonneg_of_pos hp floor_pos
  have hden : 0 < 1+D*(max 0 (q-R*p)/(p+powerFloor)) :=
    add_pos_of_pos_of_nonneg zero_lt_one (mul_nonneg hd (div_nonneg (le_max_left _ _) hf.le))
  unfold reduction
  rw [div_le_iff₀ hden]
  field_simp
  constructor <;> intro h <;> linarith

/-- A bound on finite differences across the knee. It improves quadratically
in a common lower bound on the two controller denominators. -/
theorem reduction_wet_lipschitz (K D R p q₁ q₂ b : ℝ)
    (hK : 0 ≤ K) (hD : 0 ≤ D) (hp : 0 ≤ p) (hb : 0 < b)
    (h₁ : b ≤ 1+D*(max 0 (q₁-R*p)/(p+powerFloor)))
    (h₂ : b ≤ 1+D*(max 0 (q₂-R*p)/(p+powerFloor))) :
    |reduction K D R p q₁ - reduction K D R p q₂| ≤
      K*D/((p+powerFloor)*b^2)*|q₁-q₂| := by
  let m₁ := max 0 (q₁-R*p)
  let m₂ := max 0 (q₂-R*p)
  let v := p+(powerFloor : ℝ)
  let d₁ := 1+D*(m₁/v)
  let d₂ := 1+D*(m₂/v)
  have hv : 0 < v := add_pos_of_nonneg_of_pos hp floor_pos
  have hd₁ : 0 < d₁ := hb.trans_le h₁
  have hd₂ : 0 < d₂ := hb.trans_le h₂
  have hdiff : |m₂-m₁| ≤ |q₁-q₂| := by
    simpa only [m₁, m₂, max_comm (0:ℝ), sub_sub_sub_cancel_right, abs_sub_comm q₂ q₁]
      using abs_max_sub_max_le_abs (q₂-R*p) (q₁-R*p) 0
  have hid : K/d₁-K/d₂ = K*D*(m₂-m₁)/(v*d₁*d₂) := by
    field_simp [hv.ne',hd₁.ne',hd₂.ne']
    dsimp [d₁,d₂]
    field_simp
    ring
  change |K/d₁-K/d₂| ≤ _
  rw [hid, abs_div, abs_of_pos (mul_pos (mul_pos hv hd₁) hd₂), abs_mul, abs_of_nonneg (mul_nonneg hK hD)]
  change _ ≤ K*D/(v*b^2)*_
  rw [div_mul_eq_mul_div]
  apply div_le_div₀ (mul_nonneg (mul_nonneg hK hD) (abs_nonneg _))
    (mul_le_mul_of_nonneg_left hdiff (mul_nonneg hK hD)) (mul_pos hv (sq_pos_of_pos hb))
  calc
    v*b^2 = v*b*b := by ring
    _ ≤ v*d₁*d₂ := mul_le_mul (mul_le_mul_of_nonneg_left h₁ hv.le) h₂ hb.le (mul_nonneg hv.le hd₁.le)

/-- Four ladder stages and five real wet-detector memories. -/
abbrev Audio := ℝ × ℝ × ℝ × ℝ
abbrev Wet := Audio × Memory
abbrev Forcing := ℝ × ℝ

def wet (s : NaiveReuppingLadder ℝ) : Wet :=
  ((s.stage1,s.stage2,s.stage3,s.stage4),memory s.feedbackEnvelope)

def installWet (s : NaiveReuppingLadder ℝ) (w : Wet) : NaiveReuppingLadder ℝ :=
  {s with
    stage1 := w.1.1, stage2 := w.1.2.1, stage3 := w.1.2.2.1, stage4 := w.1.2.2.2
    feedbackEnvelope := install s.feedbackEnvelope w.2}

@[simp] theorem wet_install (s : NaiveReuppingLadder ℝ) (w : Wet) :
    wet (installWet s w) = w := by simp [wet, installWet]

structure Knobs where
  alpha : ℝ
  sat : NormalizedTanh.Coefficients ℝ
  K : ℝ
  D : ℝ
  R : ℝ

def knobs (s : NaiveReuppingLadder ℝ) : Knobs :=
  ⟨s.alpha,s.saturation,s.baseFeedback,s.dampingScale,s.referencePowerGain⟩

def audio (k : Knobs) (s : Audio) (x feedback : ℝ) : Audio :=
  let f := OnePoleZOHLowPass.onePoleStep k.alpha
  let sat := NormalizedTanh.apply k.sat
  let v₁ := f (x-feedback*s.2.2.2) s.1
  let v₂ := f (sat v₁) s.2.1
  let v₃ := f (sat v₂) s.2.2.1
  (v₁,v₂,v₃,f (sat v₃) s.2.2.2)

/-- The second forcing component is the dry detector power BEFORE this sample. -/
def advanceWet (k : Knobs) (g a : ℝ) (s : Wet) (u : Forcing) : Wet :=
  let feedback := reduction k.K k.D k.R u.2 s.2.2.2
  let v := audio k s.1 u.1 feedback
  (v, advance g a s.2 v.2.2.2)

def observe (k : Knobs) (s : Wet) (u : Forcing) : ℝ :=
  let feedback := reduction k.K k.D k.R u.2 s.2.2.2
  NormalizedTanh.apply k.sat (audio k s.1 u.1 feedback).2.2.2 * (feedback+1)

theorem wet_step (s : NaiveReuppingLadder ℝ) (g a x : ℝ) (h : Tuned s.feedbackEnvelope g a) :
    wet (step s ⟨x⟩) = advanceWet (knobs s) g a (wet s) (x,s.inputEnvelope.power) := by
  dsimp only [advanceWet, knobs, wet, memory]
  rw [← reduction_eq s]
  exact Prod.ext rfl (memory_step _ g a _ h)

theorem output_step (s : NaiveReuppingLadder ℝ) (x : ℝ) :
    (step s ⟨x⟩).output = observe (knobs s) (wet s) (x,s.inputEnvelope.power) := by
  dsimp only [observe, knobs, wet, memory]
  rw [← reduction_eq s]
  rfl

abbrev run (s : NaiveReuppingLadder ℝ) (u : ℕ → ℝ) := Step.states step (fun n => ⟨u n⟩) s

theorem knobs_run (s : NaiveReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    knobs (run s u n) = knobs s :=
  Step.states_invariant step knobs (fun _ _ => rfl) _ s n

end DSP.NaiveReuppingLadder.DetectorDynamics

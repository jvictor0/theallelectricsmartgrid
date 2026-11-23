import DSP.Filters.NaiveReuppingLadder.DetectorDynamics
import Mathlib.Tactic

noncomputable section
namespace DSP.NaiveReuppingLadder.NoiseResponse.WetFeedbackBudget
open Finset DetectorDynamics

def threshold (K D R c : ℝ) : ℝ := R + (K/c-1)/D

def excess (T R p q : ℝ) : ℝ := q-T*p-(T-R)*powerFloor

theorem high_iff (K D R c p q : ℝ) (hc : 0 < c) (hcK : c ≤ K)
    (hD : 0 < D) (hp : 0 ≤ p) :
    c ≤ reduction K D R p q ↔ excess (threshold K D R c) R p q ≤ 0 := by
  let v := p + (powerFloor : ℝ)
  let s := (K/c-1)/D
  have hv : 0 < v := add_pos_of_nonneg_of_pos hp floor_pos
  have hs : 0 ≤ s := div_nonneg (sub_nonneg.mpr ((one_le_div hc).mpr hcK)) hD.le
  unfold reduction
  change c ≤ K/(1+D*(max 0 (q-R*p)/v)) ↔ _
  rw [le_div_iff₀ (by positivity), ← le_div_iff₀' hc]
  have eq : 1+D*(max 0 (q-R*p)/v) ≤ K/c ↔ max 0 (q-R*p) ≤ s*v := by
    calc
      _ ↔ D*(max 0 (q-R*p)/v) ≤ K/c-1 := by constructor <;> intro h <;> linarith
      _ ↔ max 0 (q-R*p)/v ≤ s := by exact (le_div_iff₀' hD).symm
      _ ↔ max 0 (q-R*p) ≤ s*v := div_le_iff₀ hv
  rw [eq,max_le_iff,and_iff_right (mul_nonneg hs hv.le)]
  change q-R*p ≤ s*(p+powerFloor) ↔ q-(R+s)*p-((R+s)-R)*powerFloor ≤ 0
  constructor <;> intro h <;> nlinarith only [h]

def deficit (e : ℝ) : ℝ := min 0 e

/-- Endpoint selection makes an upward threshold crossing cost no selected
    energy, and a downward crossing never charges the prior positive excess. -/
theorem selected_step {a e e' w : ℝ} (h1 : a ≤ 1)
    (step : e' = (1-a)*e+a*w) :
    a*(if e' ≤ 0 then w else 0) ≤ deficit e'-(1-a)*deficit e := by
  have leak : 0 ≤ 1-a := sub_nonneg.mpr h1
  unfold deficit
  split_ifs with h
  · rw [min_eq_right h]
    linarith only [step, mul_le_mul_of_nonneg_left (min_le_right 0 e) leak]
  · rw [min_eq_left (le_of_not_ge h)]
    linarith only [mul_nonpos_of_nonneg_of_nonpos leak (min_le_left 0 e)]

theorem selected_sum (e w : ℕ → ℝ) {a : ℝ} (ha : 0 < a) (h1 : a ≤ 1)
    (step : ∀ t, e (t+1) = (1-a)*e t+a*w t) (n : ℕ) :
    (∑ t ∈ range n, if e (t+1) ≤ 0 then w t else 0) ≤
      (deficit (e n)-deficit (e 0))/a + ∑ t ∈ range n, deficit (e t) := by
  have led (m : ℕ) :
      a*(∑ t ∈ range m, if e (t+1) ≤ 0 then w t else 0) ≤
        deficit (e m)-deficit (e 0) + a*(∑ t ∈ range m, deficit (e t)) := by
    induction m with
    | zero => simp
    | succ m ih =>
      simp only [sum_range_succ]
      linarith only [ih,selected_step h1 (step m)]
  apply (le_of_mul_le_mul_left _ ha)
  rw [mul_add, mul_div_cancel₀ _ ha.ne']
  exact led n

/-- A gate may disagree with the exact comparison near the threshold. Only
    its error at the updated power enters the potential inequality. -/
theorem rounded_step {a e e' w err guardError : ℝ} (h1 : a ≤ 1)
    (gate : Prop) [Decidable gate]
    (step : e' = (1-a)*e+a*w+err)
    (guard : (if gate then e' else 0)-deficit e' ≤ guardError) :
    a*(if gate then w else 0) ≤ deficit e'-(1-a)*deficit e +
      guardError + (if gate then -err else 0) := by
  have leak := sub_nonneg.mpr h1
  have lo : deficit e ≤ e := min_le_right 0 e
  have hi : deficit e ≤ 0 := min_le_left 0 e
  split_ifs at guard ⊢ with h
  · linarith only [step, guard, mul_le_mul_of_nonneg_left lo leak]
  · linarith only [guard, mul_nonpos_of_nonneg_of_nonpos leak hi]

/-- Two one-sided comparisons suffice for a rounded threshold gate. -/
theorem guard_of_bounds (gate : Prop) [Decidable gate] {e allowance : ℝ}
    (hn : 0 ≤ allowance) (inside : gate → e ≤ allowance)
    (outside : ¬gate → -allowance ≤ e) :
    (if gate then e else 0)-deficit e ≤ allowance := by
  unfold deficit
  by_cases he : e ≤ 0
  · rw [min_eq_right he]
    split_ifs with h
    · simpa using hn
    · linarith only [outside h]
  · rw [min_eq_left (le_of_not_ge he)]
    split_ifs with h
    · simpa using inside h
    · simpa using hn

/-- Rounded-comparison allowances and signed EMA update defects accumulate
    linearly. Both can be bounded in terms of observed powers and tap squares. -/
theorem rounded_sum (e w err guardError : ℕ → ℝ) (gate : ℕ → Prop)
    [∀ t, Decidable (gate t)] {a : ℝ} (ha : 0 < a) (h1 : a ≤ 1)
    (step : ∀ t, e (t+1) = (1-a)*e t+a*w t+err t)
    (guard : ∀ t, (if gate t then e (t+1) else 0)-deficit (e (t+1)) ≤ guardError t)
    (n : ℕ) :
    (∑ t ∈ range n, if gate t then w t else 0) ≤
      (deficit (e n)-deficit (e 0) +
        ∑ t ∈ range n, (guardError t + (if gate t then -err t else 0)))/a +
          ∑ t ∈ range n, deficit (e t) := by
  apply le_of_mul_le_mul_left _ ha
  rw [mul_add, mul_div_cancel₀ _ ha.ne']
  induction n with
  | zero => simp
  | succ n ih =>
    simp only [sum_range_succ]
    linarith only [ih, rounded_step h1 (gate n) (step n) (guard n)]

/-- The same signed budget applies to any actual dry and wet tap sequences
    when their power updates and high-feedback gate have certified errors. -/
theorem rounded_high_energy (p q dry wet δp δq guardError : ℕ → ℝ) (gate : ℕ → Prop)
    [∀ t, Decidable (gate t)] {T R a : ℝ} (ha : 0 < a) (h1 : a ≤ 1)
    (pd : ∀ t, p (t+1) = (1-a)*p t+a*(dry t)^2+δp t)
    (qw : ∀ t, q (t+1) = (1-a)*q t+a*(wet t)^2+δq t)
    (guard : ∀ t, let e' := excess T R (p (t+1)) (q (t+1))
      (if gate t then e' else 0)-deficit e' ≤ guardError t)
    (n : ℕ) :
    let e := fun t => excess T R (p t) (q t)
    (∑ t ∈ range n, if gate t then (wet t)^2-T*(dry t)^2-(T-R)*powerFloor else 0) ≤
      (deficit (e n)-deficit (e 0) +
        ∑ t ∈ range n, (guardError t + (if gate t then T*δp t-δq t else 0)))/a +
          ∑ t ∈ range n, deficit (e t) := by
  have update (t : ℕ) : excess T R (p (t+1)) (q (t+1)) =
      (1-a)*excess T R (p t) (q t) +
        a*((wet t)^2-T*(dry t)^2-(T-R)*powerFloor)+(δq t-T*δp t) := by
    rw [excess,pd,qw,excess]
    ring
  simpa only [neg_sub] using rounded_sum (fun t => excess T R (p t) (q t))
    (fun t => (wet t)^2-T*(dry t)^2-(T-R)*powerFloor)
    (fun t => δq t-T*δp t) guardError gate ha h1 update guard n

/-- Bandpass energy weighted by high-feedback occupation, with the gate read
    after processing the sample. Both detectors use the same smoothing rate. -/
theorem high_energy (p q dry wet : ℕ → ℝ) {K D R c a : ℝ} (hc : 0 < c)
    (hcK : c ≤ K) (hD : 0 < D) (ha : 0 < a) (h1 : a ≤ 1)
    (hp : ∀ t, 0 ≤ p t)
    (pd : ∀ t, p (t+1) = (1-a)*p t+a*(dry t)^2)
    (qw : ∀ t, q (t+1) = (1-a)*q t+a*(wet t)^2) (n : ℕ) :
    let T := threshold K D R c
    let e := fun t => excess T R (p t) (q t)
    (∑ t ∈ range n, if c ≤ reduction K D R (p (t+1)) (q (t+1))
      then (wet t)^2-T*(dry t)^2-(T-R)*powerFloor else 0) ≤
      (deficit (e n)-deficit (e 0))/a + ∑ t ∈ range n, deficit (e t) := by
  dsimp only
  simp_rw [high_iff K D R c _ _ hc hcK hD (hp _)]
  apply selected_sum (fun t => excess (threshold K D R c) R (p t) (q t))
    (fun t => (wet t)^2-threshold K D R c*(dry t)^2-(threshold K D R c-R)*powerFloor) ha h1
  intro t
  rw [excess,pd,qw,excess]
  ring


private theorem detectorUpdate (s : BandPassedEnvelopeFollower ℝ) (x : Sample ℝ) :
    (s.step x).power = (1-s.powerAlpha)*s.power + s.powerAlpha*((s.step x).stage2.bandPass)^2 := by
  simp only [BandPassedEnvelopeFollower.step, OnePoleZOHLowPass.relax]
  ring

private theorem runFields [Tanh ℝ] (s : NaiveReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) :
    let z := run s u n
    z.inputEnvelope.powerAlpha = s.inputEnvelope.powerAlpha ∧
      z.feedbackEnvelope.powerAlpha = s.feedbackEnvelope.powerAlpha := by
  induction n with
  | zero => exact ⟨rfl,rfl⟩
  | succ n ih => exact ih

set_option maxHeartbeats 800000 in
/-- Instantiation on the detector-on wet run, including arbitrary incoming
    ladder and detector memories with nonnegative starting dry power. The
    selected bandpass samples update the powers used on the next sample. -/
theorem wet_run [Tanh ℝ] (s : NaiveReuppingLadder ℝ) (u : ℕ → ℝ) (n : ℕ) {c : ℝ}
    (hc : 0 < c) (hcK : c ≤ s.baseFeedback) (hD : 0 < s.dampingScale)
    (ha : 0 < s.inputEnvelope.powerAlpha) (h1 : s.inputEnvelope.powerAlpha ≤ 1)
    (rate : s.feedbackEnvelope.powerAlpha = s.inputEnvelope.powerAlpha)
    (initial : 0 ≤ s.inputEnvelope.power) :
    let z := run s u
    let a := s.inputEnvelope.powerAlpha
    let T := threshold s.baseFeedback s.dampingScale s.referencePowerGain c
    let e := fun t => excess T s.referencePowerGain (z t).inputEnvelope.power (z t).feedbackEnvelope.power
    (∑ t ∈ range n, if c ≤ effectiveFeedback (z (t+1))
      then ((z (t+1)).feedbackEnvelope.stage2.bandPass)^2 -
        T*((z (t+1)).inputEnvelope.stage2.bandPass)^2 - (T-s.referencePowerGain)*powerFloor else 0) ≤
      (deficit (e n)-deficit (e 0))/a + ∑ t ∈ range n, deficit (e t) := by
  let z := run s u
  have pd (t : ℕ) : (z (t+1)).inputEnvelope.power =
      (1-s.inputEnvelope.powerAlpha)*(z t).inputEnvelope.power +
      s.inputEnvelope.powerAlpha*((z (t+1)).inputEnvelope.stage2.bandPass)^2 := by
    change ((z t).inputEnvelope.step ⟨u t⟩).power = _
    rw [detectorUpdate, (runFields s u t).1]
    rfl
  have qw (t : ℕ) : (z (t+1)).feedbackEnvelope.power =
      (1-s.inputEnvelope.powerAlpha)*(z t).feedbackEnvelope.power +
      s.inputEnvelope.powerAlpha*((z (t+1)).feedbackEnvelope.stage2.bandPass)^2 := by
    change ((z t).feedbackEnvelope.step ⟨(z (t+1)).stage4⟩).power = _
    rw [detectorUpdate, (runFields s u t).2, rate]
    rfl
  have hp (t : ℕ) : 0 ≤ (z t).inputEnvelope.power := by
    induction t with
    | zero => exact initial
    | succ t ih =>
      rw [pd]
      exact add_nonneg (mul_nonneg (sub_nonneg.mpr h1) ih) (mul_nonneg ha.le (sq_nonneg _))
  have kr (t : ℕ) : effectiveFeedback (z t) =
      reduction s.baseFeedback s.dampingScale s.referencePowerGain
        (z t).inputEnvelope.power (z t).feedbackEnvelope.power := by
    have h := knobs_run s u t
    rw [reduction_eq]
    change reduction (knobs (z t)).K (knobs (z t)).D (knobs (z t)).R _ _ = _
    rw [show z t = run s u t from rfl,h]
    rfl
  dsimp only
  let T := threshold s.baseFeedback s.dampingScale s.referencePowerGain c
  let weight := fun t => ((z (t+1)).feedbackEnvelope.stage2.bandPass)^2 -
    T*((z (t+1)).inputEnvelope.stage2.bandPass)^2-(T-s.referencePowerGain)*powerFloor
  let k := fun t => reduction s.baseFeedback s.dampingScale s.referencePowerGain
    (z t).inputEnvelope.power (z t).feedbackEnvelope.power
  have sums :
      (∑ t ∈ range n, if c ≤ effectiveFeedback (z (t+1)) then weight t else 0) =
        ∑ t ∈ range n, if c ≤ k (t+1) then weight t else 0 := by
    apply sum_congr rfl
    intro t _
    rw [kr]
  change (∑ t ∈ range n, if c ≤ effectiveFeedback (z (t+1)) then weight t else 0) ≤ _
  rw [sums]
  exact high_energy (fun t => (z t).inputEnvelope.power) (fun t => (z t).feedbackEnvelope.power)
    (fun t => (z (t+1)).inputEnvelope.stage2.bandPass) (fun t => (z (t+1)).feedbackEnvelope.stage2.bandPass)
    hc hcK hD ha h1 hp pd qw n

#print axioms wet_run
#print axioms rounded_high_energy

end DSP.NaiveReuppingLadder.NoiseResponse.WetFeedbackBudget

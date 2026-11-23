import DSP.Filters.NaiveReuppingLadder.DetectorDynamics
import Mathlib.Analysis.Calculus.MeanValue

noncomputable section
namespace DSP.NaiveReuppingLadder.NoiseResponse.DetectorVariation
open Finset Real DetectorDynamics

/-- With Q the wet power, P the dry power, R the reference power ratio and F the
    actual floor, the controller is a clipped function of log((Q+RF)/(P+F)). -/
def feedback (K D R z : ℝ) := K / (1+D*(Real.exp (max (Real.log R) z)-R))

theorem log_coordinates {K D R p q : ℝ} (hR : 0 < R) (hp : 0 ≤ p) (hq : 0 ≤ q) :
    reduction K D R p q = feedback K D R
      (Real.log (q+R*powerFloor)-Real.log (p+powerFloor)) := by
  have hd : 0 < p+(powerFloor:ℝ) := add_pos_of_nonneg_of_pos hp floor_pos
  have hw : 0 < q+R*(powerFloor:ℝ) := add_pos_of_nonneg_of_pos hq (mul_pos hR floor_pos)
  unfold feedback reduction
  rw [Real.exp_monotone.map_max,Real.exp_log hR,Real.exp_sub,Real.exp_log hw,Real.exp_log hd]
  congr 2
  by_cases h : q ≤ R*p
  · have bound : (q+R*powerFloor)/(p+powerFloor) ≤ R := (div_le_iff₀ hd).2 (by linarith)
    rw [max_eq_left bound,max_eq_left (sub_nonpos.mpr h)]; ring
  · have h' : R ≤ (q+R*powerFloor)/(p+powerFloor) := (le_div_iff₀ hd).2 (by linarith)
    rw [max_eq_right h',max_eq_right (by linarith : 0 ≤ q-R*p)]
    field_simp; ring

/-- M bounds the logarithmic slope without any inverse power-floor factor.
    The hypotheses allow the sharp M = 1/(4(1-DR)) when DR ≤ 1/2, and
    M = DR when DR ≥ 1/2. -/
theorem log_lipschitz {K D R M : ℝ} (hK : 0 ≤ K) (hD : 0 ≤ D) (hR : 0 < R)
    (hM : 0 ≤ M) (bound : ∀ v : ℝ, 0 ≤ v → v+D*R ≤ M*(1+v)^2) (x y : ℝ) :
    |feedback K D R y-feedback K D R x| ≤ K*M*|y-x| := by
  let f := fun t : ℝ => K/(1+D*(Real.exp t-R))
  let df := fun t : ℝ => -(K*(D*Real.exp t))/(1+D*(Real.exp t-R))^2
  have epos {t : ℝ} (ht : t ∈ Set.Ici (Real.log R)) : 0 ≤ D*(Real.exp t-R) :=
    mul_nonneg hD (sub_nonneg.mpr (by simpa [Real.exp_log hR] using Real.exp_le_exp.mpr ht))
  have deriv (t : ℝ) (ht : t ∈ Set.Ici (Real.log R)) : HasDerivWithinAt f (df t) (Set.Ici (Real.log R)) t := by
    have pos : 1+D*(Real.exp t-R)≠0 := by linarith [epos ht]
    have e := (Real.hasDerivAt_exp t).sub_const R
    have b := (e.const_mul D).const_add 1
    have der := (hasDerivAt_const t K).div b pos
    change HasDerivWithinAt (fun t : ℝ => K/(1+D*(Real.exp t-R))) _ _ _
    apply HasDerivAt.hasDerivWithinAt
    convert der using 1 <;> first | rfl | ring
  have bd (t : ℝ) (ht : t ∈ Set.Ici (Real.log R)) : ‖df t‖ ≤ K*M := by
    dsimp [df]; rw [abs_div,abs_neg,abs_of_nonneg (by positivity),
      abs_of_nonneg (sq_nonneg _)]
    apply (div_le_iff₀ (sq_pos_of_pos (by linarith [epos ht]))).2
    have h := bound _ (epos ht)
    nlinarith only [mul_le_mul_of_nonneg_left h hK]
  let X := max (Real.log R) x
  let Y := max (Real.log R) y
  have h := (convex_Ici (Real.log R)).norm_image_sub_le_of_norm_hasDerivWithin_le deriv bd
    (show X ∈ Set.Ici (Real.log R) from le_max_left (Real.log R) x)
    (show Y ∈ Set.Ici (Real.log R) from le_max_left (Real.log R) y)
  have hc : |Y-X| ≤ |y-x| := by simpa [Y,X,max_comm] using abs_max_sub_max_le_abs y x (Real.log R)
  exact h.trans (mul_le_mul_of_nonneg_left hc (mul_nonneg hK hM))

theorem low_slope {d : ℝ} (hd : d ≤ 1/2) {v : ℝ} :
    v+d ≤ (1/(4*(1-d)))*(1+v)^2 := by
  rw [one_div_mul_eq_div]
  apply (le_div_iff₀ (by linarith : 0 < 4*(1-d))).mpr
  nlinarith [sq_nonneg (v-(1-2*d))]

theorem high_slope {d : ℝ} (hd : 1/2 ≤ d) {v : ℝ} (hv : 0 ≤ v) :
    v+d ≤ d*(1+v)^2 := by
  nlinarith [mul_nonneg (by linarith : 0 ≤ 2*d-1) hv,
    mul_nonneg (by linarith : 0 ≤ d) (sq_nonneg v)]

/-- Each EMA can decay by at most its leak in one sample. Upward increments
    need no pointwise bound; their total is paid for by the log endpoint change. -/
theorem variation (u : ℕ → ℝ) {a : ℝ} (ha0 : 0 ≤ a) (ha : a < 1) (pos : ∀ t, 0 < u t)
    (leak : ∀ t, (1-a)*u t ≤ u (t+1)) (n : ℕ) :
    ∑ t ∈ range n, |Real.log (u (t+1))-Real.log (u t)| ≤
      Real.log (u n)-Real.log (u 0)-2*n*Real.log (1-a) := by
  induction n with
  | zero => simp
  | succ n ih =>
    have h := Real.log_le_log (mul_pos (sub_pos.mpr ha) (pos n)) (leak n)
    rw [Real.log_mul (sub_pos.mpr ha).ne' (pos n).ne'] at h
    have nonpos := Real.log_nonpos (sub_pos.mpr ha).le (by linarith : 1-a ≤ 1)
    rw [sum_range_succ,Nat.cast_succ]
    rcases le_total (Real.log (u n)) (Real.log (u (n+1))) with up|down
    · rw [abs_of_nonneg (sub_nonneg.mpr up)]; linarith
    · rw [abs_of_nonpos (sub_nonpos.mpr down)]; linarith

theorem ema_leak {p nextP value a F : ℝ} (ha : 0 ≤ a) (hF : 0 ≤ F)
    (update : nextP=p+a*(value^2-p)) :
    (1-a)*(p+F) ≤ nextP+F := by
  rw [update]; nlinarith [mul_nonneg ha (sq_nonneg value),mul_nonneg ha hF]

/-- Both ladder detectors implement this same squared-bandpass EMA. -/
theorem detector_step_leak (s : BandPassedEnvelopeFollower ℝ) (x : Sample ℝ)
    {F : ℝ} (ha : 0 ≤ s.powerAlpha) (hF : 0 ≤ F) :
    (1-s.powerAlpha)*(s.power+F) ≤ (s.step x).power+F := by
  apply ema_leak (value:=(TPTSVF4Pole.stepBandPass ⟨s.stage1,s.stage2⟩ x).stage2.bandPass) ha hF
  simp [BandPassedEnvelopeFollower.step,OnePoleZOHLowPass.relax,pow_two]

/-- Total change in k over a block. For bounded shifted powers the endpoint term
    divided by n vanishes, leaving 4 K M (-log(1-a)) per sample. -/
theorem controller_variation (p q : ℕ → ℝ) {K D R M a : ℝ}
    (hK : 0 ≤ K) (hD : 0 ≤ D) (hR : 0 < R) (hM : 0 ≤ M)
    (hm : ∀ v : ℝ, 0 ≤ v → v+D*R ≤ M*(1+v)^2)
    (h0 : 0 ≤ a) (ha : a < 1) (hp : ∀ t, 0 ≤ p t) (hq : ∀ t, 0 ≤ q t)
    (pd : ∀ t, (1-a)*(p t+powerFloor) ≤ p (t+1)+powerFloor)
    (qd : ∀ t, (1-a)*(q t+R*powerFloor) ≤ q (t+1)+R*powerFloor) (n : ℕ) :
    let k := fun t => reduction K D R (p t) (q t)
    ∑ t ∈ range n, |k (t+1)-k t| ≤ K*M*(
      (Real.log (p n+powerFloor)-Real.log (p 0+powerFloor)) +
      (Real.log (q n+R*powerFloor)-Real.log (q 0+R*powerFloor))-4*n*Real.log (1-a)) := by
  let dry := fun t => Real.log (p t+powerFloor)
  let wet := fun t => Real.log (q t+R*powerFloor)
  have k0 := mul_nonneg hK hM
  have point (t : ℕ) :
      |reduction K D R (p (t+1)) (q (t+1))-reduction K D R (p t) (q t)| ≤
        K*M*(|dry (t+1)-dry t|+|wet (t+1)-wet t|) := by
    rw [log_coordinates hR (hp (t+1)) (hq (t+1)),log_coordinates hR (hp t) (hq t)]
    apply (log_lipschitz hK hD hR hM hm _ _).trans
    apply mul_le_mul_of_nonneg_left _ k0
    change |wet (t+1)-dry (t+1)-(wet t-dry t)| ≤ _
    rw [show wet (t+1)-dry (t+1)-(wet t-dry t)=(wet (t+1)-wet t)-(dry (t+1)-dry t) by ring]
    exact (abs_sub (wet (t+1)-wet t) (dry (t+1)-dry t)).trans_eq (add_comm ..)
  have d := variation (fun t => p t+powerFloor) h0 ha (fun t => add_pos_of_nonneg_of_pos (hp t) floor_pos) pd n
  have w := variation (fun t => q t+R*powerFloor) h0 ha
    (fun t => add_pos_of_nonneg_of_pos (hq t) (mul_pos hR floor_pos)) qd n
  dsimp only
  apply (sum_le_sum (fun t _ => point t)).trans
  rw [←mul_sum,sum_add_distrib]
  apply mul_le_mul_of_nonneg_left _ k0
  change (∑ t ∈ range n,|Real.log (p (t+1)+powerFloor)-Real.log (p t+powerFloor)|)+
    (∑ t ∈ range n,|Real.log (q (t+1)+R*powerFloor)-Real.log (q t+R*powerFloor)|) ≤ _
  linarith

end DSP.NaiveReuppingLadder.NoiseResponse.DetectorVariation

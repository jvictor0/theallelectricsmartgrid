import DSP.Filters.BandPassedEnvelopeFollower.PeriodicTracking

noncomputable section
namespace DSP.BandPassedEnvelopeFollower.Periodic
open TPTSVF.Convergence Filter

/-- The maximum over a period also sees its predecessor, even at phase zero. -/
private theorem periodic_bound (e : ℕ → ℝ) (N : ℕ) (hN : 0 < N) (he : Function.Periodic e N)
    (r U B : ℝ) (hr : 0 ≤ r) (hr1 : r < 1) (hB : U ≤ (1-r)*B)
    (h : ∀ i, e (i+1) ≤ r*e i+U) (i : ℕ) : e i ≤ B := by
  obtain ⟨j,_,hm⟩ := (Finset.range N).exists_max_image e (Finset.nonempty_range_iff.mpr hN.ne')
  have hmax (n : ℕ) : e n ≤ e j := by
    rw [← he.map_mod_nat n]
    exact hm _ (Finset.mem_range.mpr (Nat.mod_lt n hN))
  have hprev := h (j+N-1)
  rw [show j+N-1+1 = j+N by omega, he j] at hprev
  have hh := mul_le_mul_of_nonneg_left (hmax (j+N-1)) hr
  refine (hmax i).trans ?_
  by_contra! hbad
  nlinarith [mul_pos (sub_pos.mpr hr1) (sub_pos.mpr hbad)]

/-- A periodic one-pole smoother has sup gain at most one for every positive
new-sample weight up to one, independent of period length. -/
theorem smooth_bound (a : ℝ) (ha : 0 < a) (ha1 : a ≤ 1) (v : ℕ → ℝ) (N : ℕ)
    (hN : 0 < N) (hv : Function.Periodic v N) (B : ℝ)
    (hstep : ∀ i, |v (i+1)| ≤ (1-a)*|v i|+a*B) (i : ℕ) : |v i| ≤ B :=
  periodic_bound (fun n => |v n|) N hN (fun n => congrArg abs (hv n)) (1-a) (a*B) B (by linarith) (by linarith)
    (by ring_nf; rfl) hstep i

/-- Conservative analytic bound on the induced sup gain of one tuned Q=1
bandpass, including its observation of both old and new modal states. -/
def tapGain (g : ℝ) := 2*‖drive g‖/(1-‖pole g‖)

theorem tapGain_nonneg (g : ℝ) (hg : 0 < g) : 0 ≤ tapGain g :=
  div_nonneg (by positivity) (sub_pos.mpr (pole_norm_lt_one _ hg)).le

theorem first_abs (z : ℂ) : |firstState z| ≤ 2*‖z‖ := by
  have hs : 1 ≤ Real.sqrt 3 := by norm_num
  calc
    _ ≤ |z.re| + |z.im / Real.sqrt 3| := abs_sub _ _
    _ ≤ ‖z‖ + ‖z‖ := by
      apply add_le_add (Complex.abs_re_le_norm z)
      rw [abs_div,abs_of_nonneg (Real.sqrt_nonneg _)]
      exact (div_le_self (abs_nonneg _) hs).trans (Complex.abs_im_le_norm z)
    _ = _ := by ring

private theorem band_difference (g : ℝ) (hg : 0 < g) (N : ℕ) (hN : 0 < N)
    (v w : ℕ → ℂ) (x y : ℕ → ℝ) (B : ℝ) (hB : ∀ i, |x i-y i| ≤ B)
    (hv : Function.Periodic v N) (hw : Function.Periodic w N)
    (hx : ∀ i, v (i+1) = modalStep g (v i) (x i))
    (hy : ∀ i, w (i+1) = modalStep g (w i) (y i)) (i : ℕ) :
    |tap (v i) (v (i+1)) - tap (w i) (w (i+1))| ≤ tapGain g * B := by
  let b := ‖drive g‖*B/(1-‖pole g‖)
  have hd := sub_pos.mpr (pole_norm_lt_one g hg)
  have hstate (n : ℕ) : ‖v n-w n‖ ≤ b := by
    apply periodic_bound (fun j => ‖v j-w j‖) N hN (fun j => by simp only [hv j,hw j]) ‖pole g‖ (‖drive g‖*B)
      b (norm_nonneg _) (pole_norm_lt_one _ hg) (by dsimp [b]; field_simp; rfl) _ n
    intro j
    rw [hx,hy]
    have eq : modalStep g (v j) (x j) - modalStep g (w j) (y j) =
        pole g*(v j-w j) + drive g*((x j-y j : ℝ) : ℂ) := by
      simp only [modalStep,Complex.ofReal_sub]; ring
    rw [eq]
    refine (norm_add_le _ _).trans ?_
    simp only [norm_mul,Complex.norm_real,Real.norm_eq_abs]
    exact add_le_add le_rfl (mul_le_mul_of_nonneg_left (hB _) (norm_nonneg _))
  have ht : tap (v i) (v (i+1))-tap (w i) (w (i+1)) =
      (firstState (v (i+1)-w (i+1)) + firstState (v i-w i))/2 := by
    simp only [tap,firstState_sub]; ring
  rw [ht,abs_div,abs_of_pos (by norm_num : (0:ℝ)<2)]
  apply (div_le_iff₀ (by norm_num : (0:ℝ)<2)).mpr
  have h₁ := first_abs (v (i+1)-w (i+1))
  have h₂ := first_abs (v i-w i)
  have he : tapGain g * B = 2*b := by unfold tapGain b; ring
  rw [he]
  linarith [abs_add_le (firstState (v (i+1)-w (i+1))) (firstState (v i-w i)),hstate i,hstate (i+1)]

def bandOutput (g a : ℝ) (u : ℕ → ℝ) (N n : ℕ) :=
  tap (orbit g a u N n).2.1 (orbit g a u N (n+1)).2.1

theorem band_difference_bound (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (N : ℕ) (hN : 0 < N) (x y : ℕ → ℝ) (hx : Function.Periodic x N) (hy : Function.Periodic y N)
    (B : ℝ) (hB : ∀ i, |x i-y i| ≤ B) (n : ℕ) :
    |bandOutput g a x N n - bandOutput g a y N n| ≤ tapGain g ^ 2 * B := by
  have hX := orbit_periodic g a hg ha ha1 x N hN hx
  have hY := orbit_periodic g a hg ha ha1 y N hN hy
  have h₁ := band_difference g hg N hN (fun j => (orbit g a x N j).1) (fun j => (orbit g a y N j).1) x y B hB (fun i => congrArg Prod.fst (hX i))
    (fun i => congrArg Prod.fst (hY i)) (fun _ => rfl) (fun _ => rfl)
  have h₂ := band_difference g hg N hN (fun j => (orbit g a x N j).2.1) (fun j => (orbit g a y N j).2.1) _ _ (tapGain g * B) h₁
    (fun i => congrArg (fun t : Memory => t.2.1) (hX i))
    (fun i => congrArg (fun t : Memory => t.2.1) (hY i)) (fun _ => rfl) (fun _ => rfl) n
  simpa only [bandOutput,pow_two,mul_assoc] using h₂

private theorem zero_orbit (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (N : ℕ) (hN : 0 < N) (n : ℕ) : orbit g a (fun _ => 0) N n = 0 := by
  have hr := orbit_periodic g a hg ha ha1 (fun _ => 0) N hN (fun _ => rfl)
  have hs (j : ℕ) : Step.states (advance g a) (fun _ => 0) 0 j = 0 := by
    induction j with
    | zero => rfl
    | succ j ih => simp only [Step.states_succ,ih]; simp [advance,modalStep,tap,firstState]
  have ht := tracks_orbit g a hg ha ha1 (fun _ => 0) N hN (fun _ => rfl) 0
  have hj : Tendsto (fun j : ℕ => j*N+n) Filter.atTop Filter.atTop :=
    Filter.tendsto_atTop_mono (fun j => (Nat.le_mul_of_pos_right j hN).trans (Nat.le_add_right _ _)) Filter.tendsto_id
  have he (j : ℕ) : orbit g a (fun _ => 0) N (j*N+n) = orbit g a (fun _ => 0) N n := by
    simpa [Nat.add_comm] using hr.nat_mul j n
  have ht' := ht.comp hj
  simp only [Function.comp_def,hs,he,zero_sub] at ht'
  simpa using (tendsto_nhds_unique tendsto_const_nhds ht')

/-- Uniform wet-power sensitivity on waveforms bounded by M. The detector
sees the fourth ladder stage; the input and the stage need not be sinusoids. -/
theorem power_difference_bound (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (N : ℕ) (hN : 0 < N) (x y : ℕ → ℝ) (hx : Function.Periodic x N) (hy : Function.Periodic y N)
    (M B : ℝ) (hm : ∀ i, |x i| ≤ M) (hm' : ∀ i, |y i| ≤ M) (hB : ∀ i, |x i-y i| ≤ B)
    (n : ℕ) : |(orbit g a x N n).2.2 - (orbit g a y N n).2.2| ≤ 2*tapGain g ^ 4*M*B := by
  have hX := orbit_periodic g a hg ha ha1 x N hN hx
  have hY := orbit_periodic g a hg ha ha1 y N hN hy
  have hsingle (v : ℕ → ℝ) (hp : Function.Periodic v N) (hb : ∀ i, |v i| ≤ M) (i : ℕ) :
      |bandOutput g a v N i| ≤ tapGain g^2*M := by
    have h := band_difference_bound g a hg ha ha1 N hN v (fun _ => 0) hp (fun _ => rfl)
      M (by simpa using hb) i
    have hz : bandOutput g a (fun _ => 0) N i = 0 := by simp [bandOutput,zero_orbit g a hg ha ha1 N hN,tap,firstState]
    simpa only [hz,sub_zero] using h
  apply smooth_bound a ha ha1 (fun i => (orbit g a x N i).2.2-(orbit g a y N i).2.2) N hN
    (fun i => by simp only [hX i,hY i]) _ _ n
  intro i
  let X := bandOutput g a x N i
  let Y := bandOutput g a y N i
  have hb : |X^2-Y^2| ≤ 2*tapGain g^4*M*B := by
    rw [sq_sub_sq,abs_mul,mul_comm |X+Y|]
    calc
      _ ≤ (tapGain g^2*B)*(tapGain g^2*M+tapGain g^2*M) := mul_le_mul
        (band_difference_bound g a hg ha ha1 N hN x y hx hy B hB i)
        ((abs_add_le _ _).trans (add_le_add (hsingle _ hx hm i) (hsingle _ hy hm' i)))
        (abs_nonneg _) (mul_nonneg (sq_nonneg _) ((abs_nonneg _).trans (hB i)))
      _ = _ := by ring
  change |((1-a)*(orbit g a x N i).2.2+a*X^2)-((1-a)*(orbit g a y N i).2.2+a*Y^2)| ≤ _
  rw [show ((1-a)*(orbit g a x N i).2.2+a*X^2)-((1-a)*(orbit g a y N i).2.2+a*Y^2) =
      (1-a)*((orbit g a x N i).2.2-(orbit g a y N i).2.2)+a*(X^2-Y^2) by ring]
  refine (abs_add_le _ _).trans ?_
  simpa only [abs_mul,abs_of_nonneg (sub_nonneg.mpr ha1),abs_of_nonneg ha.le] using
    add_le_add (le_refl ((1-a)*|(orbit g a x N i).2.2-(orbit g a y N i).2.2|))
      (mul_le_mul_of_nonneg_left hb ha.le)

end DSP.BandPassedEnvelopeFollower.Periodic

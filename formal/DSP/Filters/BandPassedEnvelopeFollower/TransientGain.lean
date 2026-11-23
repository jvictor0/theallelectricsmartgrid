import DSP.Filters.BandPassedEnvelopeFollower.PeriodicGain
import DSP.Analysis.AsymptoticBound

noncomputable section
namespace DSP.BandPassedEnvelopeFollower.Periodic
open TPTSVF.Convergence Asymptotic

abbrev difference (x y : ℕ → ℝ) := fun i => |x i-y i|

private theorem modal_gain (g : ℝ) (hg : 0 < g)
    (v w : ℕ → ℂ) (x y : ℕ → ℝ) (B : ℝ) (hb : Bound (difference x y) B)
    (hv : ∀ i, v (i+1) = modalStep g (v i) (x i)) (hw : ∀ i, w (i+1) = modalStep g (w i) (y i)) :
    Bound (difference (fun i => tap (v i) (v (i+1))) (fun i => tap (w i) (w (i+1)))) (tapGain g * B) := by
  let P := ‖drive g‖*B/(1-‖pole g‖)
  have hr := pole_norm_lt_one g hg
  have he : Bound (fun i => ‖v i-w i‖) P := by
    apply Bound.recurrence (norm_nonneg _) hr (y := fun i => ‖drive g‖*|x i-y i|)
    · have h := hb.scale (norm_nonneg (drive g))
      rwa [show (1-‖pole g‖)*P = ‖drive g‖*B by dsimp [P]; field_simp [(sub_pos.mpr hr).ne']]
    · intro i
      rw [hv,hw,show modalStep g (v i) (x i)-modalStep g (w i) (y i) =
        pole g*(v i-w i)+drive g*((x i-y i : ℝ) : ℂ) by simp only [modalStep,Complex.ofReal_sub]; ring]
      simpa only [norm_mul,Complex.norm_real,Real.norm_eq_abs] using norm_add_le (pole g*(v i-w i))
        (drive g*((x i-y i : ℝ) : ℂ))
  have hfirst : Bound (difference (fun i => firstState (v i)) (fun i => firstState (w i))) (2*P) :=
    (he.scale (by norm_num : (0:ℝ) ≤ 2)).mono (fun i => by
      simpa only [difference,firstState_sub] using first_abs (v i-w i))
  have hsum := (hfirst.shift.add hfirst).scale (by norm_num : (0:ℝ) ≤ 1/2)
  rw [show tapGain g * B = 1/2*(2*P + 2*P) by unfold tapGain P; ring]
  apply hsum.mono
  intro i
  dsimp only [difference,tap]
  rw [show (firstState (v (i+1))+firstState (v i))/2-(firstState (w (i+1))+firstState (w i))/2 =
      (1/2)*((firstState (v (i+1))-firstState (w (i+1)))+(firstState (v i)-firstState (w i))) by ring,
    abs_mul,abs_of_pos (by norm_num : (0:ℝ)<1/2)]
  exact mul_le_mul_of_nonneg_left (abs_add_le _ _) (by norm_num)

def bandTrace (t : ℕ → Memory) (i : ℕ) := tap (t i).2.1 (t (i+1)).2.1

private theorem cascade_gain (g a : ℝ) (hg : 0 < g) (t r : ℕ → Memory) (x y : ℕ → ℝ)
    (ht : ∀ i, t (i+1) = advance g a (t i) (x i)) (hr : ∀ i, r (i+1) = advance g a (r i) (y i))
    (B : ℝ) (hb : Bound (difference x y) B) : Bound (difference (bandTrace t) (bandTrace r)) (tapGain g^2 * B) := by
  let b := fun (m : ℕ → Memory) i => tap (m i).1 (m (i+1)).1
  have h1 := modal_gain g hg (fun i => (t i).1) (fun i => (r i).1) x y B hb
    (fun i => congrArg Prod.fst (ht i)) (fun i => congrArg Prod.fst (hr i))
  have h2 := modal_gain g hg (fun i => (t i).2.1) (fun i => (r i).2.1) (b t) (b r) _ h1
    (fun i => by dsimp only [b]; rw [ht i]; rfl) (fun i => by dsimp only [b]; rw [hr i]; rfl)
  rw [show tapGain g^2*B = tapGain g*(tapGain g*B) by ring]
  exact h2

theorem transient_power_difference (g a : ℝ) (hg : 0 < g) (ha : 0 < a) (ha1 : a ≤ 1)
    (t r : ℕ → Memory) (x y : ℕ → ℝ)
    (ht : ∀ i, t (i+1) = advance g a (t i) (x i)) (hr : ∀ i, r (i+1) = advance g a (r i) (y i))
    (M B : ℝ) (hx : Bound (fun i => |x i|) M) (hy : Bound (fun i => |y i|) M)
    (hb : Bound (difference x y) B) :
    Bound (difference (fun i => (t i).2.2) (fun i => (r i).2.2)) (2*tapGain g^4*M*B) := by
  have z (_ : ℕ) : (0 : Memory) = advance g a 0 0 := by simp [advance,modalStep,tap,firstState]; rfl
  have hs (m : ℕ → Memory) (v : ℕ → ℝ) (hm : ∀ i, m (i+1) = advance g a (m i) (v i))
      (h : Bound (fun i => |v i|) M) : Bound (fun i => |bandTrace m i|) (tapGain g^2*M) := by
    have h₁ := cascade_gain g a hg m (fun _ => 0) v (fun _ => 0) hm z M (by apply h.mono; intro i; simp [difference])
    apply h₁.mono
    intro i
    change |bandTrace m i| ≤ |bandTrace m i-tap 0 0|
    simp [tap,firstState]
  have hdiff := cascade_gain g a hg t r x y ht hr B hb
  have hsum := (hs t x ht hx).add (hs r y hr hy)
  have hprod := hsum.mul hdiff (fun i => add_nonneg (abs_nonneg _) (abs_nonneg _)) (fun i => abs_nonneg _)
  have hsq : Bound (fun i => |bandTrace t i^2-bandTrace r i^2|) (2*tapGain g^4*M*B) := by
    rw [show 2*tapGain g^4*M*B = (tapGain g^2*M+tapGain g^2*M)*(tapGain g^2*B) by ring]
    apply hprod.mono
    intro i
    rw [sq_sub_sq,abs_mul]
    exact mul_le_mul_of_nonneg_right (abs_add_le _ _) (abs_nonneg _)
  apply Bound.smooth a ha ha1 hsq
  intro i
  have hpow (m : ℕ → Memory) (v : ℕ → ℝ) (hm : ∀ i, m (i+1) = advance g a (m i) (v i)) :
      (m (i+1)).2.2 = (1-a)*(m i).2.2+a*(bandTrace m i)^2 := by rw [bandTrace,hm i]; rfl
  dsimp only [difference]
  rw [hpow t x ht,hpow r y hr]
  rw [show (1-a)*(t i).2.2+a*(bandTrace t i)^2-((1-a)*(r i).2.2+a*(bandTrace r i)^2) =
    (1-a)*((t i).2.2-(r i).2.2)+a*(bandTrace t i^2-bandTrace r i^2) by ring]
  refine (abs_add_le _ _).trans ?_
  rw [abs_mul,abs_mul,abs_of_nonneg (sub_nonneg.mpr ha1),abs_of_nonneg ha.le]

end DSP.BandPassedEnvelopeFollower.Periodic

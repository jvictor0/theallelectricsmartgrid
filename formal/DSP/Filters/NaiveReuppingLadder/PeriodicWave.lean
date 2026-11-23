import DSP.Filters.NaiveReuppingLadder.DetectorAttraction
import DSP.Filters.BandPassedEnvelopeFollower.PeriodicGain

/-! Close the physical feedback loop around the entire periodic fourth-stage
waveform. Iterating this map solves a boundary-value problem; its iteration
counter is independent of audio time. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.PeriodicWave
open DetectorDynamics BandPassedEnvelopeFollower.Periodic Metric
open scoped NNReal Topology

structure Setup where
  k : Knobs
  g : ℝ
  a : ℝ
  u : ℕ → ℝ
  N : ℕ
  positivePeriod : 0 < N
  periodic : Function.Periodic u N
  detectorGain : 0 < g
  detectorAlpha : 0 < a
  detectorAlpha_le : a ≤ 1
  alpha : 0 < k.alpha
  alpha_le : k.alpha ≤ 1
  feedback : 0 ≤ k.K
  damping : 0 ≤ k.D
  satDen : 0 < k.sat.tanhGain

namespace Setup
variable (s : Setup)

abbrev Wave := Fin s.N → ℝ
def waveAt (b : s.Wave) (n : ℕ) := b ⟨n%s.N,Nat.mod_lt _ s.positivePeriod⟩
theorem waveAt_period (b : s.Wave) : Function.Periodic (s.waveAt b) s.N :=
  fun n => by simp [waveAt]
theorem waveAt_fin (b : s.Wave) (j : Fin s.N) : s.waveAt b j = b j := by simp [waveAt, Nat.mod_eq_of_lt j.isLt]

def smooth (x : ℕ → ℝ) := affineOrbit (1-s.k.alpha) (fun i => s.k.alpha*x i) s.N
theorem smooth_period (x : ℕ → ℝ) (hx : Function.Periodic x s.N) :
    Function.Periodic (s.smooth x) s.N := by
  apply affine_period _ _ _ (fun i => by rw [hx i])
  exact (sub_pos.mpr (pow_lt_one₀ (sub_nonneg.mpr s.alpha_le)
    (by linarith [s.alpha]) s.positivePeriod.ne')).ne'

theorem smooth_max (x : ℕ → ℝ) (hx : Function.Periodic x s.N) (B : ℝ) (hb : ∀ i, |x i| ≤ B)
    (n : ℕ) : |s.smooth x n| ≤ B := by
  apply smooth_bound _ s.alpha s.alpha_le _ _ s.positivePeriod (s.smooth_period x hx) B _ n
  intro i
  change |(1-s.k.alpha)*s.smooth x i + s.k.alpha*x i| ≤ _
  refine (abs_add_le _ _).trans ?_
  simp only [abs_mul,abs_of_nonneg (sub_nonneg.mpr s.alpha_le),abs_of_nonneg s.alpha.le]
  exact add_le_add le_rfl (mul_le_mul_of_nonneg_left (hb i) s.alpha.le)

theorem smooth_difference (x y : ℕ → ℝ) (hx : Function.Periodic x s.N) (hy : Function.Periodic y s.N)
    (B : ℝ) (hb : ∀ i, |x i-y i| ≤ B) (n : ℕ) : |s.smooth x n-s.smooth y n| ≤ B := by
  apply smooth_bound _ s.alpha s.alpha_le (fun i => s.smooth x i-s.smooth y i) _ s.positivePeriod
    (fun i => by simp only [s.smooth_period x hx i,s.smooth_period y hy i]) B _ n
  intro i
  change |((1-s.k.alpha)*s.smooth x i + s.k.alpha*x i)-((1-s.k.alpha)*s.smooth y i + s.k.alpha*y i)| ≤ _
  rw [show ((1-s.k.alpha)*s.smooth x i + s.k.alpha*x i)-((1-s.k.alpha)*s.smooth y i + s.k.alpha*y i) =
    (1-s.k.alpha)*(s.smooth x i-s.smooth y i) + s.k.alpha*(x i-y i) by ring]
  refine (abs_add_le _ _).trans ?_
  simp only [abs_mul,abs_of_nonneg (sub_nonneg.mpr s.alpha_le),abs_of_nonneg s.alpha.le]
  exact add_le_add le_rfl (mul_le_mul_of_nonneg_left (hb i) s.alpha.le)

def M := 1/s.k.sat.tanhGain
def S := |s.k.sat.gain|/s.k.sat.tanhGain
theorem M_pos : 0 < s.M := div_pos zero_lt_one s.satDen
theorem S_nonneg : 0 ≤ s.S := div_nonneg (abs_nonneg _) s.satDen.le

theorem sat_difference (x y : ℝ) :
    |NormalizedTanh.apply s.k.sat x-NormalizedTanh.apply s.k.sat y| ≤ s.S*|x-y| := by
  obtain ⟨z,_,he⟩ := NormalizedTanh.exists_secant s.k.sat (min x y) (max x y) x y
    ⟨min_le_left ..,le_max_left ..⟩ ⟨min_le_right ..,le_max_right ..⟩
  rw [he,abs_mul]
  apply mul_le_mul_of_nonneg_right _ (abs_nonneg _)
  rw [NormalizedTanh.slope,abs_div,abs_mul,abs_of_pos s.satDen,
    abs_of_nonneg (sub_nonneg.mpr (Real.tanh_sq_lt_one _).le)]
  apply div_le_div_of_nonneg_right _ s.satDen.le
  exact mul_le_of_le_one_right (abs_nonneg _) (by linarith [sq_nonneg (Real.tanh (z*s.k.sat.gain))])

def chain (v : ℕ → ℝ) := s.smooth (fun i => NormalizedTanh.apply s.k.sat (v (i+1)))

theorem shift_period {T : Type} {v : ℕ → T} (hv : Function.Periodic v s.N) :
    Function.Periodic (fun i => v (i+1)) s.N := fun i => by
  change v (i+s.N+1) = _
  rw [show i+s.N+1=i+1+s.N by omega,hv (i+1)]

theorem chain_period (v : ℕ → ℝ) (hv : Function.Periodic v s.N) : Function.Periodic (s.chain v) s.N :=
  s.smooth_period _ (fun i => congrArg (NormalizedTanh.apply s.k.sat) (s.shift_period hv i))

theorem chain_difference (v w : ℕ → ℝ) (hv : Function.Periodic v s.N) (hw : Function.Periodic w s.N)
    (B : ℝ) (hb : ∀ n, |v n-w n| ≤ B) (n : ℕ) : |s.chain v n-s.chain w n| ≤ s.S*B :=
  s.smooth_difference (fun i => NormalizedTanh.apply s.k.sat (v (i+1)))
    (fun i => NormalizedTanh.apply s.k.sat (w (i+1)))
    (fun i => congrArg (NormalizedTanh.apply s.k.sat) (s.shift_period hv i))
    (fun i => congrArg (NormalizedTanh.apply s.k.sat) (s.shift_period hw i))
    _ (fun i => (s.sat_difference _ _).trans (mul_le_mul_of_nonneg_left (hb (i+1)) s.S_nonneg)) n

def dry := orbit s.g s.a s.u s.N
def detector (b : s.Wave) := orbit s.g s.a (fun n => s.waveAt b (n+1)) s.N
def feedbackAt (b : s.Wave) (n : ℕ) :=
  reduction s.k.K s.k.D s.k.R (s.dry n).2.2 (s.detector b n).2.2
def driveIn (b : s.Wave) (n : ℕ) := s.u n - s.feedbackAt b n * s.waveAt b n
def first (b : s.Wave) := s.smooth (s.driveIn b)
def fourth (b : s.Wave) := s.chain (s.chain (s.chain (s.first b)))
def update (b : s.Wave) : s.Wave := fun i => s.fourth b i

theorem dry_period : Function.Periodic s.dry s.N := orbit_periodic _ _ s.detectorGain
  s.detectorAlpha s.detectorAlpha_le _ _ s.positivePeriod s.periodic
theorem detector_period (b : s.Wave) : Function.Periodic (s.detector b) s.N :=
  orbit_periodic _ _ s.detectorGain s.detectorAlpha s.detectorAlpha_le _ _ s.positivePeriod
    (s.shift_period (s.waveAt_period b))

theorem drive_period (b : s.Wave) : Function.Periodic (s.driveIn b) s.N := fun i => by
  simp only [driveIn,feedbackAt,s.periodic i,s.dry_period i,s.detector_period b i,s.waveAt_period b i]

theorem first_period (b : s.Wave) : Function.Periodic (s.first b) s.N :=
  s.smooth_period _ (s.drive_period b)
theorem fourth_period (b : s.Wave) : Function.Periodic (s.fourth b) s.N :=
  s.chain_period _ (s.chain_period _ (s.chain_period _ (s.first_period b)))

theorem update_bound (b : s.Wave) (i : Fin s.N) : |s.update b i| ≤ s.M :=
  s.smooth_max _ (fun i => congrArg (NormalizedTanh.apply s.k.sat) (s.shift_period
    (s.chain_period _ (s.chain_period _ (s.first_period b))) i))
    _ (fun _ => Exploration.saturation_bound _ s.satDen _) i

theorem dry_nonneg (n : ℕ) : 0 ≤ (s.dry n).2.2 := power_nonnegative _ _ s.detectorGain
  s.detectorAlpha s.detectorAlpha_le _ _ s.positivePeriod s.periodic n

/-- A lower bound on dry power sharpens this estimate. Zero always works. -/
def L (pMin : ℝ) := s.S^3*(s.k.K+s.M*(s.k.K*s.k.D/(pMin+powerFloor))*(2*tapGain s.g^4*s.M))

def denominator (n : ℕ) (q : ℝ) :=
  let p := (s.dry n).2.2
  1+s.k.D*(max 0 (q-s.k.R*p)/(p+powerFloor))

theorem denominator_ge_one (i : ℕ) (q : ℝ) : 1 ≤ s.denominator i q := by
  unfold denominator
  exact le_add_of_nonneg_right (mul_nonneg s.damping (div_nonneg (le_max_left _ _)
    (add_pos_of_nonneg_of_pos (s.dry_nonneg i) floor_pos).le))

def envelopeGain (M : ℝ) := 2*tapGain s.g^4*M

theorem envelopeGain_nonneg (M : ℝ) (hM : 0 ≤ M) : 0 ≤ s.envelopeGain M :=
  mul_nonneg (mul_nonneg (by norm_num) (pow_nonneg (tapGain_nonneg _ s.detectorGain) _)) hM

theorem detector_difference (b c : s.Wave) (M : ℝ) (hb : ∀ i, |b i| ≤ M)
    (hc : ∀ i, |c i| ≤ M) (i : ℕ) :
    |(s.detector b i).2.2-(s.detector c i).2.2| ≤ s.envelopeGain M * dist b c :=
  power_difference_bound s.g s.a s.detectorGain s.detectorAlpha s.detectorAlpha_le s.N
    s.positivePeriod _ _ (s.shift_period (s.waveAt_period b)) (s.shift_period (s.waveAt_period c))
    M _ (fun _ => hb _) (fun _ => hc _) (fun j => (dist_pi_le_iff (f := b) (g := c) dist_nonneg).mp
      le_rfl ⟨(j+1)%s.N,Nat.mod_lt _ s.positivePeriod⟩) i

def localGain (M pMin dMin : ℝ) :=
  s.S^3*(s.k.K/dMin + M*(s.k.K*s.k.D/((pMin+powerFloor)*dMin^2))*s.envelopeGain M)

theorem update_difference (M pMin dMin : ℝ) (hM : 0 ≤ M) (hp : 0 ≤ pMin) (hd : 0 < dMin)
    (hlow : ∀ i, pMin ≤ (s.dry i).2.2)
    (b c : s.Wave) (hb : ∀ i, |b i| ≤ M) (hc : ∀ i, |c i| ≤ M)
    (hdb : ∀ i, dMin ≤ s.denominator i (s.detector b i).2.2)
    (hdc : ∀ i, dMin ≤ s.denominator i (s.detector c i).2.2) :
    dist (s.update b) (s.update c) ≤ s.localGain M pMin dMin * dist b c := by
  let B := dist b c
  let Lq := s.k.K*s.k.D/((pMin+powerFloor)*dMin^2)
  let E := s.envelopeGain M
  let H := s.k.K/dMin+M*Lq*E
  have hf := add_pos_of_nonneg_of_pos hp floor_pos
  have hLq : 0 ≤ Lq := div_nonneg (mul_nonneg s.feedback s.damping) (mul_pos hf (sq_pos_of_pos hd)).le
  have hE := s.envelopeGain_nonneg M hM
  have hK := div_nonneg s.feedback hd.le
  have hH : 0 ≤ H := add_nonneg hK (mul_nonneg (mul_nonneg hM hLq) hE)
  have hbc (i : ℕ) : |s.waveAt b i-s.waveAt c i| ≤ B :=
    (dist_pi_le_iff (f := b) (g := c) dist_nonneg).mp le_rfl ⟨i%s.N,Nat.mod_lt _ s.positivePeriod⟩
  have hq := s.detector_difference b c M hb hc
  have hdrive (i : ℕ) : |s.driveIn b i-s.driveIn c i| ≤ H*B := by
    let p := (s.dry i).2.2
    have hk : |s.feedbackAt b i| ≤ s.k.K/dMin := by
      change |s.k.K/s.denominator i _| ≤ _
      rw [abs_of_nonneg (div_nonneg s.feedback (hd.le.trans (hdb i)))]
      exact div_le_div_of_nonneg_left s.feedback hd (hdb i)
    have hchange := reduction_wet_lipschitz s.k.K s.k.D s.k.R p _ _ dMin s.feedback s.damping
      (s.dry_nonneg i) hd (hdb i) (hdc i)
    have hqq : |s.feedbackAt b i-s.feedbackAt c i| ≤ Lq*(E*B) := hchange.trans (mul_le_mul
      (div_le_div_of_nonneg_left (mul_nonneg s.feedback s.damping) (mul_pos hf (sq_pos_of_pos hd))
        (mul_le_mul_of_nonneg_right (add_le_add (hlow i) le_rfl) (sq_nonneg dMin)))
      (hq i) (abs_nonneg _) hLq)
    have he : s.driveIn b i-s.driveIn c i =
        -(s.feedbackAt b i*(s.waveAt b i-s.waveAt c i) +
          (s.feedbackAt b i-s.feedbackAt c i)*s.waveAt c i) := by unfold driveIn; ring
    rw [he,abs_neg]
    refine (abs_add_le _ _).trans ?_
    rw [abs_mul,abs_mul]
    calc
      _ ≤ s.k.K/dMin*B+(Lq*(E*B))*M := add_le_add
        (mul_le_mul hk (hbc i) (abs_nonneg _) hK)
        (mul_le_mul hqq (hc _) (abs_nonneg _) (mul_nonneg hLq (mul_nonneg hE dist_nonneg)))
      _ = H*B := by dsimp [H]; ring
  have h1 := s.smooth_difference _ _ (s.drive_period b) (s.drive_period c) _ hdrive
  have h2 := s.chain_difference _ _ (s.first_period b) (s.first_period c) _ h1
  have h3 := s.chain_difference _ _ (s.chain_period _ (s.first_period b)) (s.chain_period _ (s.first_period c)) _ h2
  have h4 := s.chain_difference _ _ (s.chain_period _ (s.chain_period _ (s.first_period b)))
    (s.chain_period _ (s.chain_period _ (s.first_period c))) _ h3
  change dist _ _ ≤ s.S^3*H*B
  apply (dist_pi_le_iff (mul_nonneg (mul_nonneg (pow_nonneg s.S_nonneg _) hH) dist_nonneg)).mpr
  intro i
  calc
    _ ≤ s.S*(s.S*(s.S*(H*B))) := h4 i
    _ = _ := by ring

theorem update_lipschitz (pMin : ℝ) (hp : 0 ≤ pMin) (hlow : ∀ i, pMin ≤ (s.dry i).2.2)
    (b c : s.Wave) (hb : ∀ i, |b i| ≤ s.M) (hc : ∀ i, |c i| ≤ s.M) :
    dist (s.update b) (s.update c) ≤ s.L pMin * dist b c := by
  simpa only [L,localGain,envelopeGain,div_one,one_pow,mul_one] using
    s.update_difference s.M pMin 1 s.M_pos.le hp zero_lt_one hlow b c hb hc
      (fun i => s.denominator_ge_one i _) (fun i => s.denominator_ge_one i _)

end Setup
end DSP.NaiveReuppingLadder.PeriodicWave

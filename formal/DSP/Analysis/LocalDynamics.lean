import DSP.Analysis.StabilityGauge
import Mathlib.Analysis.Calculus.ContDiff.RCLike

/-! Local forced dynamics, with constants uniform in the number of samples. -/

noncomputable section
namespace DSP.LocalDynamics
open Filter
open scoped Topology NNReal

variable {E : Type*} [NormedAddCommGroup E] [NormedSpace ℝ E]

def orbit (f : E × ℝ → E) (u : ℕ → ℝ) (initial : E) : ℕ → E
  | 0 => initial
  | n+1 => f (orbit f u initial n, u n)

structure LocalContraction (f : E × ℝ → E) (p : Seminorm ℝ E) where
  radius : ℝ
  radius_pos : 0 < radius
  rate : ℝ
  rate_nonneg : 0 ≤ rate
  rate_lt_one : rate < 1
  forcing : ℝ
  forcing_nonneg : 0 ≤ forcing
  estimate : ∀ x y u v, p x ≤ radius → p y ≤ radius → |u| ≤ radius → |v| ≤ radius →
    p (f (x,u)-f (y,v)) ≤ rate*p (x-y)+forcing*|u-v|

def statePart (D : E × ℝ →L[ℝ] E) : E →L[ℝ] E :=
  D.comp (ContinuousLinearMap.inl ℝ E ℝ)

theorem derivative_split (D : E × ℝ →L[ℝ] E) (x : E) (u : ℝ) :
    D (x,u) = statePart D x + u • D (0,1) := by
  have h : (x,u) = (x,(0:ℝ)) + u • (0,1) := by simp
  rw [h,map_add,map_smul]
  rfl

theorem exists_localContraction {f : E × ℝ → E} (D : E × ℝ →L[ℝ] E)
    (hD : HasStrictFDerivAt f D 0) (g : StabilityGauge (statePart D).toLinearMap) :
    Nonempty (LocalContraction f g.size) := by
  have hmargin := sub_pos.mpr g.rate_lt_one
  have hbound := g.bound_pos
  have hrate := g.rate_nonneg
  let delta : ℝ≥0 := ⟨(1-g.rate)/(2*g.bound), by positivity⟩
  have hd : 0 < (delta : ℝ) := div_pos (sub_pos.mpr g.rate_lt_one) (by positivity)
  have hres : HasStrictFDerivAt (fun z => f z-D z) (0 : E × ℝ →L[ℝ] E) 0 := by
    simpa only [sub_self] using hD.fun_sub D.hasStrictFDerivAt
  obtain ⟨U,hU,hLip⟩ := hres.exists_lipschitzOnWith_of_nnnorm_lt delta (by simpa using hd)
  obtain ⟨r,hr,hball⟩ := Metric.mem_nhds_iff.mp hU
  let q := g.rate+g.bound*delta
  have hq : q < 1 := by
    have he : g.bound*(delta:ℝ) = (1-g.rate)/2 := by
      change g.bound*((1-g.rate)/(2*g.bound)) = (1-g.rate)/2
      field_simp
    dsimp only [q]
    rw [he]
    linarith [g.rate_lt_one]
  refine ⟨⟨r/2,by positivity,q,by dsimp only [q]; positivity,hq,
    g.size (D (0,1))+g.bound*delta,by positivity,?_⟩⟩
  intro x y u v hx hy hu hv
  have hmem (w : E) (a : ℝ) (hw : g.size w ≤ r/2) (ha : |a| ≤ r/2) : (w,a) ∈ U := by
    apply hball
    rw [Metric.mem_ball,dist_zero_right,Prod.norm_def,Real.norm_eq_abs]
    apply max_lt
    · exact ((g.norm_le w).trans hw).trans_lt (by linarith)
    · linarith
  have hl := (lipschitzOnWith_iff_norm_sub_le.mp hLip) (hmem x u hx hu) (hmem y v hy hv)
  have hn : ‖(x,u)-(y,v)‖ ≤ g.size (x-y)+|u-v| := by
    rw [Prod.norm_def]
    change max ‖x-y‖ |u-v| ≤ _
    exact max_le ((g.norm_le _).trans (le_add_of_nonneg_right (abs_nonneg _)))
      (le_add_of_nonneg_left (apply_nonneg g.size _))
  have hresbound := (g.le_norm ((f (x,u)-D (x,u))-(f (y,v)-D (y,v)))).trans
    (mul_le_mul_of_nonneg_left (hl.trans (mul_le_mul_of_nonneg_left hn hd.le)) g.bound_pos.le)
  have hsum : f (x,u)-f (y,v) = statePart D (x-y)+(u-v) • D (0,1)+
      ((f (x,u)-D (x,u))-(f (y,v)-D (y,v))) := by
    rw [← derivative_split]
    change _ = D ((x,u)-(y,v))+_
    rw [map_sub]
    abel
  rw [hsum]
  have hlinear := (map_add_le_add g.size (statePart D (x-y)) ((u-v) • D (0,1))).trans
    (add_le_add (g.contracts (x-y)) (le_of_eq (map_smul_eq_mul g.size (u-v) (D (0,1)))))
  have h := (map_add_le_add g.size _ _).trans (add_le_add hlinear hresbound)
  simp only [Real.norm_eq_abs] at h
  exact h.trans_eq (by dsimp only [q]; ring)

def localContraction {f : E × ℝ → E} (D : E × ℝ →L[ℝ] E)
    (hD : HasStrictFDerivAt f D 0) (g : StabilityGauge (statePart D).toLinearMap) :
    LocalContraction f g.size := (exists_localContraction D hD g).some

namespace LocalContraction
variable {f : E × ℝ → E} {p : Seminorm ℝ E} (m : LocalContraction f p)

def gain : ℝ := 1+m.forcing/(1-m.rate)

theorem gain_ge_one : 1 ≤ m.gain := by
  have h := div_nonneg m.forcing_nonneg (sub_pos.mpr m.rate_lt_one).le
  dsimp only [gain]
  linarith

theorem gain_pos : 0 < m.gain := lt_of_lt_of_le (by norm_num) m.gain_ge_one

theorem gain_step : m.rate*m.gain+m.forcing ≤ m.gain := by
  have hn := ne_of_gt (sub_pos.mpr m.rate_lt_one)
  have he : (1-m.rate)*m.gain = (1-m.rate)+m.forcing := by
    dsimp only [gain]
    field_simp
  nlinarith [m.rate_lt_one]

theorem orbit_bound (hf : f (0,0) = 0) (epsilon : ℝ) (he : 0 ≤ epsilon)
    (hsmall : m.gain*epsilon ≤ m.radius) (u : ℕ → ℝ) (hu : ∀ n, |u n| ≤ epsilon)
    (n : ℕ) : p (orbit f u 0 n) ≤ m.gain*epsilon := by
  have hinput : epsilon ≤ m.radius :=
    (le_mul_of_one_le_left he m.gain_ge_one).trans hsmall
  induction n with
  | zero => simpa only [orbit,map_zero] using mul_nonneg m.gain_pos.le he
  | succ n ih =>
    have h := m.estimate (orbit f u 0 n) 0 (u n) 0 (ih.trans hsmall)
      (by simpa only [map_zero] using m.radius_pos.le) ((hu n).trans hinput)
      (by simpa using m.radius_pos.le)
    simp only [hf,sub_zero] at h
    exact h.trans ((add_le_add (mul_le_mul_of_nonneg_left ih m.rate_nonneg)
      (mul_le_mul_of_nonneg_left (hu n) m.forcing_nonneg)).trans
        (by nlinarith [mul_le_mul_of_nonneg_right m.gain_step he]))

theorem orbit_distance (u : ℕ → ℝ) (x y : E)
    (hx : ∀ n, p (orbit f u x n) ≤ m.radius)
    (hy : ∀ n, p (orbit f u y n) ≤ m.radius)
    (hu : ∀ n, |u n| ≤ m.radius) (n : ℕ) :
    p (orbit f u x n-orbit f u y n) ≤ m.rate^n*p (x-y) := by
  induction n with
  | zero => simp [orbit]
  | succ n ih =>
    have h := m.estimate (orbit f u x n) (orbit f u y n) (u n) (u n) (hx n) (hy n) (hu n) (hu n)
    simp only [sub_self,abs_zero,mul_zero,add_zero] at h
    exact h.trans ((mul_le_mul_of_nonneg_left ih m.rate_nonneg).trans_eq (by rw [pow_succ]; ring))

end LocalContraction

/-- Uniform differentiation of the complete zero-state response, over every
sample and every input bounded by one. This is stronger than a derivative at
each separately fixed sample. -/
theorem uniform_linearization {f : E × ℝ → E} (hf : f (0,0) = 0)
    (D : E × ℝ →L[ℝ] E) (hD : HasStrictFDerivAt f D 0)
    (g : StabilityGauge (statePart D).toLinearMap) :
    ∀ eta > 0, ∃ delta > 0, ∀ epsilon : ℝ, |epsilon| < delta →
      ∀ u : ℕ → ℝ, (∀ n, |u n| ≤ 1) → ∀ n,
        ‖orbit f (fun i => epsilon*u i) 0 n-epsilon • orbit D u 0 n‖ ≤ eta*|epsilon| := by
  intro eta heta
  let m := localContraction D hD g
  let theta := eta*(1-g.rate)/(g.bound*m.gain)
  have hM := m.gain_pos
  have hC := g.bound_pos
  have hmargin := sub_pos.mpr g.rate_lt_one
  have ht : 0 < theta := by dsimp only [theta]; positivity
  have hf0 : f 0 = 0 := hf
  have hre : ∀ᶠ v in 𝓝 (0 : E × ℝ), ‖f v-D v‖ ≤ theta*‖v‖ := by
    simpa only [hf0,sub_zero] using hD.hasFDerivAt.isLittleO.def ht
  obtain ⟨r,hr,hball⟩ := Metric.mem_nhds_iff.mp hre
  refine ⟨min (m.radius/m.gain) (r/m.gain),
    lt_min (div_pos m.radius_pos hM) (div_pos hr hM),?_⟩
  intro epsilon he u hu
  have hsize : m.gain*|epsilon| ≤ m.radius := by
    have h := (lt_div_iff₀ hM).mp (lt_of_lt_of_le he (min_le_left _ _))
    nlinarith
  have hlocal : m.gain*|epsilon| < r := by
    have h := (lt_div_iff₀ hM).mp (lt_of_lt_of_le he (min_le_right _ _))
    nlinarith
  have hin (n : ℕ) : |epsilon*u n| ≤ |epsilon| := by
    rw [abs_mul]
    exact mul_le_of_le_one_right (abs_nonneg _) (hu n)
  have hx (n : ℕ) := m.orbit_bound hf |epsilon| (abs_nonneg _) hsize
    (fun i => epsilon*u i) hin n
  let x := orbit f (fun i => epsilon*u i) 0
  let t := orbit D u 0
  have hv (n : ℕ) : ‖(x n,epsilon*u n)‖ ≤ m.gain*|epsilon| := by
    rw [Prod.norm_def,Real.norm_eq_abs]
    apply max_le
    · exact (g.norm_le _).trans (hx n)
    · exact (hin n).trans (le_mul_of_one_le_left (abs_nonneg _) m.gain_ge_one)
  have hrest (n : ℕ) :
      g.size (f (x n,epsilon*u n)-D (x n,epsilon*u n)) ≤ eta*(1-g.rate)*|epsilon| := by
    have h := hball (show (x n,epsilon*u n) ∈ Metric.ball 0 r by
      rw [Metric.mem_ball,dist_zero_right]
      exact (hv n).trans_lt hlocal)
    have hp := (g.le_norm _).trans (mul_le_mul_of_nonneg_left
      (h.trans (mul_le_mul_of_nonneg_left (hv n) ht.le)) hC.le)
    have heq : g.bound*theta*m.gain = eta*(1-g.rate) := by
      dsimp only [theta]
      field_simp
    calc
      _ ≤ g.bound*(theta*(m.gain*|epsilon|)) := hp
      _ = eta*(1-g.rate)*|epsilon| := by rw [← heq]; ring
  have herr (n : ℕ) : g.size (x n-epsilon • t n) ≤ eta*|epsilon| := by
    induction n with
    | zero => simpa only [x,t,orbit,smul_zero,sub_self,map_zero] using mul_nonneg heta.le (abs_nonneg epsilon)
    | succ n ih =>
      have heq : x (n+1)-epsilon • t (n+1) =
          statePart D (x n-epsilon • t n)+(f (x n,epsilon*u n)-D (x n,epsilon*u n)) := by
        change f (x n,epsilon*u n)-epsilon • D (t n,u n) = _
        rw [derivative_split D (t n) (u n),derivative_split D (x n) (epsilon*u n)]
        simp only [map_sub,map_smul,smul_add,smul_smul]
        abel
      rw [heq]
      have h := (map_add_le_add g.size _ _).trans (add_le_add
        ((g.contracts _).trans (mul_le_mul_of_nonneg_left ih g.rate_nonneg)) (hrest n))
      exact h.trans_eq (by ring)
  intro n
  exact (g.norm_le _).trans (herr n)

/-- A differentiable output observation preserves uniform small-signal
linearization. The observation can contain additional saturation. -/
theorem uniform_observation {f : E × ℝ → E} (hf : f (0,0) = 0)
    (D : E × ℝ →L[ℝ] E) (hD : HasStrictFDerivAt f D 0)
    (g : StabilityGauge (statePart D).toLinearMap)
    {out : E → ℝ} (hout : out 0 = 0) (L : E →L[ℝ] ℝ) (hL : HasFDerivAt out L 0) :
    ∀ eta > 0, ∃ delta > 0, ∀ epsilon : ℝ, |epsilon| < delta →
      ∀ u : ℕ → ℝ, (∀ n, |u n| ≤ 1) → ∀ n,
        |out (orbit f (fun i => epsilon*u i) 0 n)-epsilon*L (orbit D u 0 n)| ≤ eta*|epsilon| := by
  intro eta heta
  let m := localContraction D hD g
  let tol := eta/(1+‖L‖)
  have ht : 0 < tol := by dsimp only [tol]; positivity
  have hM := m.gain_pos
  have htheta : 0 < tol/m.gain := div_pos ht hM
  have hre : ∀ᶠ x in 𝓝 (0 : E), |out x-L x| ≤ (tol/m.gain)*‖x‖ := by
    simpa only [hout,sub_zero,Real.norm_eq_abs] using hL.isLittleO.def htheta
  obtain ⟨r,hr,hball⟩ := Metric.mem_nhds_iff.mp hre
  obtain ⟨d,hd,hstate⟩ := uniform_linearization hf D hD g tol ht
  refine ⟨min d (min (m.radius/m.gain) (r/m.gain)),
    lt_min hd (lt_min (div_pos m.radius_pos hM) (div_pos hr hM)),?_⟩
  intro epsilon he u hu n
  have he1 := lt_of_lt_of_le he (min_le_left _ _)
  have he2 := lt_of_lt_of_le he (min_le_right _ _)
  have hsize : m.gain*|epsilon| ≤ m.radius := by
    have h := (lt_div_iff₀ hM).mp (lt_of_lt_of_le he2 (min_le_left _ _))
    nlinarith
  have hlocal : m.gain*|epsilon| < r := by
    have h := (lt_div_iff₀ hM).mp (lt_of_lt_of_le he2 (min_le_right _ _))
    nlinarith
  let x := orbit f (fun i => epsilon*u i) 0 n
  let t := orbit D u 0 n
  have hx : ‖x‖ ≤ m.gain*|epsilon| := by
    apply (g.norm_le x).trans
    apply m.orbit_bound hf |epsilon| (abs_nonneg _) hsize
    intro i
    rw [abs_mul]
    exact mul_le_of_le_one_right (abs_nonneg _) (hu i)
  have hrem : |out x-L x| ≤ tol*|epsilon| := by
    have h := hball (show x ∈ Metric.ball 0 r by
      rw [Metric.mem_ball,dist_zero_right]; exact hx.trans_lt hlocal)
    calc
      _ ≤ (tol/m.gain)*(m.gain*|epsilon|) := h.trans (mul_le_mul_of_nonneg_left hx htheta.le)
      _ = tol*|epsilon| := by field_simp
  have heq : out x-epsilon*L t = (out x-L x)+L (x-epsilon • t) := by
    simp only [map_sub,map_smul,smul_eq_mul]
    ring
  rw [heq]
  have hnorm : |L (x-epsilon • t)| ≤ ‖L‖*(tol*|epsilon|) := by
    exact (L.le_opNorm _).trans (mul_le_mul_of_nonneg_left (hstate epsilon he1 u hu n) (norm_nonneg _))
  have h := (abs_add_le _ _).trans (add_le_add hrem hnorm)
  have heq' : tol+‖L‖*tol = eta := by dsimp only [tol]; field_simp
  exact h.trans_eq (by rw [← heq']; ring)

theorem linear_tracks (D : E × ℝ →L[ℝ] E) (g : StabilityGauge (statePart D).toLinearMap)
    (u : ℕ → ℝ) (initial : E) (reference : ℕ → E)
    (href : ∀ n, D (reference n,u n) = reference (n+1)) :
    Tendsto (fun n => orbit D u initial n-reference n) atTop (𝓝 0) := by
  have hb (n : ℕ) : g.size (orbit D u initial n-reference n) ≤
      g.rate^n*g.size (initial-reference 0) := by
    induction n with
    | zero => simp [orbit]
    | succ n ih =>
      have he : orbit D u initial (n+1)-reference (n+1) =
          statePart D (orbit D u initial n-reference n) := by
        rw [orbit,← href n,← map_sub]
        simp only [Prod.mk_sub_mk,sub_self,statePart,ContinuousLinearMap.comp_apply,
          ContinuousLinearMap.inl_apply]
      rw [he]
      exact (g.contracts _).trans ((mul_le_mul_of_nonneg_left ih g.rate_nonneg).trans_eq (by rw [pow_succ]; ring))
  have ht := (tendsto_pow_atTop_nhds_zero_of_lt_one g.rate_nonneg g.rate_lt_one).mul_const
    (g.size (initial-reference 0))
  simp only [zero_mul] at ht
  apply tendsto_zero_iff_norm_tendsto_zero.mpr
  exact squeeze_zero (fun _ => norm_nonneg _) (fun n => (g.norm_le _).trans (hb n)) ht

end DSP.LocalDynamics

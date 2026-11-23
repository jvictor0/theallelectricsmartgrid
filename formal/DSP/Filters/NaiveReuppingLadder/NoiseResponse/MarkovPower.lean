import Mathlib.LinearAlgebra.FiniteDimensional.Lemmas
import Mathlib.Tactic

noncomputable section
namespace NoiseDisplay.MarkovPower
open Finset
variable {S : Type*} [Fintype S]

/-- The transition operator acts on functions of the complete digital state.
    Randomness within an analysis block is averaged before applying this operator. -/
def transition (p : S → S → ℝ) : (S → ℝ) →ₗ[ℝ] (S → ℝ) where
  toFun f i := ∑ j, p i j * f j
  map_add' f g := by ext i; simp [mul_add, sum_add_distrib]
  map_smul' c f := by ext i; simp [Finset.mul_sum,mul_left_comm]

variable (P : (S → ℝ) →ₗ[ℝ] (S → ℝ))
def atTime : ℕ → (S → ℝ) → (S → ℝ)
  | 0, v => v
  | n+1, v => P (atTime n v)

def BoundedStep := ∀ (v : S → ℝ) (b : ℝ), (∀ i, |v i| ≤ b) → ∀ i, |P v i| ≤ b

theorem stochastic_bounded (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j)
    (one : ∀ i, ∑ j, p i j = 1) : BoundedStep (transition p) := by
  intro v b hv i
  change |∑ j, p i j*v j| ≤ b
  calc
    _ ≤ ∑ j, |p i j*v j| := Finset.abs_sum_le_sum_abs _ _
    _ ≤ ∑ j, p i j*b := sum_le_sum (fun j _ => by
      rw [abs_mul,abs_of_nonneg (hp i j)];exact mul_le_mul_of_nonneg_left (hv j) (hp i j))
    _ = b := by rw [←sum_mul,one,one_mul]

omit [Fintype S] in
theorem time_bound (hP : BoundedStep P) (v : S → ℝ) (b : ℝ) (hv : ∀ i, |v i| ≤ b)
    (n : ℕ) : ∀ i, |atTime P n v i| ≤ b := by
  induction n with
  | zero => exact hv
  | succ n ih => exact hP _ _ ih

private theorem max_bound (v : S → ℝ) (i : S) : |v i| ≤ ∑ j, |v j| :=
  Finset.single_le_sum (fun j _ => abs_nonneg (v j)) (mem_univ i)

omit [Fintype S] in
private theorem affine_iterates {h v : S → ℝ} (fixed : P h = h) (diff : v-P v=h) (n : ℕ) :
    atTime P n v = v - (n:ℝ) • h := by
  induction n with
  | zero => simp [atTime]
  | succ n ih =>
    simp only [atTime,ih,map_sub,map_smul,fixed,Nat.cast_succ,add_smul,one_smul]
    rw [←diff];abel

/-- Every finite stochastic system has a harmonic mean reward h and a potential v.
    Solving these two linear equations also covers several recurrent classes and periodic ones. -/
theorem poisson_solution (hP : BoundedStep P) (r : S → ℝ) :
    ∃ h v : S → ℝ, P h = h ∧ r = h + (v-P v) := by
  let A := LinearMap.id (R:=ℝ) (M:=S → ℝ) - P
  have hd : Disjoint (LinearMap.range A) (LinearMap.ker A) := by
    rw [Submodule.disjoint_def]
    rintro x ⟨v,hv⟩ hx
    change v-P v=x at hv
    change x-P x=0 at hx
    have fixed := (sub_eq_zero.mp hx).symm
    ext i
    change x i = 0
    by_contra ne
    let b := ∑ j, |v j|
    have pos := abs_pos.mpr ne
    obtain ⟨n,hn⟩ := exists_nat_gt ((b+|v i|)/|x i|)
    have large := (div_lt_iff₀ pos).mp hn
    have bd := time_bound P hP v b (max_bound v) n i
    rw [affine_iterates P fixed hv] at bd
    change |v i - (n:ℝ)*x i| ≤ b at bd
    have tri := abs_sub_le (v i) 0 (v i-(n:ℝ)*x i)
    rw [sub_zero,zero_sub,abs_neg,sub_sub_cancel,abs_mul,abs_of_nonneg (Nat.cast_nonneg n)] at tri
    linarith
  have ht := Submodule.eq_top_of_disjoint (LinearMap.range A) (LinearMap.ker A)
    (le_of_eq A.finrank_range_add_finrank_ker.symm) hd
  have hr : r ∈ LinearMap.range A ⊔ LinearMap.ker A := by rw [ht]; trivial
  obtain ⟨_,⟨v,rfl⟩,h,hh,eq⟩ := Submodule.mem_sup.mp hr
  change h-P h=0 at hh
  exact ⟨h,v,(sub_eq_zero.mp hh).symm,by simpa [A,add_comm] using eq.symm⟩

omit [Fintype S] in
/-- The finite-time expected sum equals n times h plus an explicit boundary term. -/
theorem expected_sum {h v r : S → ℝ} (fixed : P h=h) (decomp : r=h+(v-P v)) (n : ℕ) :
    ∑ j ∈ range n, atTime P j r = (n:ℝ) • h + (v-atTime P n v) := by
  have step (k : ℕ) : atTime P k r = h+(atTime P k v-atTime P (k+1) v) := by
    induction k with
    | zero => exact decomp
    | succ k ih => simp only [atTime] at ih ⊢;rw [ih,map_add,map_sub,fixed]
  induction n with
  | zero => simp [atTime]
  | succ n ih =>
    rw [sum_range_succ,ih,step,Nat.cast_succ,add_smul,one_smul];abel

omit [Fintype S] in
theorem mean_error {h v r : S → ℝ} (hP : BoundedStep P) (fixed : P h=h)
    (decomp : r=h+(v-P v)) {n : ℕ} (hn : 0<n) (i : S)
    {b : ℝ} (hv : ∀ j, |v j| ≤ b) :
    |(∑ j ∈ range n, atTime P j r i)/(n:ℝ)-h i| ≤ 2*b/n := by
  have eq := congrFun (expected_sum P fixed decomp n) i
  simp only [Finset.sum_apply,Pi.add_apply,Pi.sub_apply,Pi.smul_apply,smul_eq_mul] at eq
  have np : (0:ℝ)<n := by exact_mod_cast hn
  have dif : (∑ j ∈ range n, atTime P j r i)/(n:ℝ)-h i=(v i-atTime P n v i)/n := by
    rw [eq];field_simp;ring
  rw [dif,abs_div,abs_of_pos np]
  apply div_le_div_of_nonneg_right _ np.le
  have tri := abs_sub_le (v i) 0 (atTime P n v i)
  rw [sub_zero,zero_sub,abs_neg] at tri
  linarith [hv i,time_bound P hP v b hv n i]

omit [Fintype S] in
private theorem time_sub (a b : S → ℝ) (n : ℕ) :
    atTime P n (a-b) = atTime P n a - atTime P n b := by
  induction n with
  | zero => rfl
  | succ n ih => rw [atTime,ih,map_sub];rfl

omit [Fintype S] in
/-- A bound on the one-block residual is already a bound on the long-run mean error.
    The unknown initial condition costs only the explicit boundary term. -/
theorem approximate_mean {h v r : S → ℝ} (hP : BoundedStep P) (fixed : P h=h)
    {b δ : ℝ} (hv : ∀ j, |v j| ≤ b) (residual : ∀ j, |(r-(h+(v-P v))) j| ≤ δ)
    {n : ℕ} (hn : 0<n) (i : S) :
    |(∑ j ∈ range n, atTime P j r i)/(n:ℝ)-h i| ≤ δ+2*b/n := by
  let u := h+(v-P v)
  let a := (∑ j ∈ range n, atTime P j r i)/(n:ℝ)
  let c := (∑ j ∈ range n, atTime P j u i)/(n:ℝ)
  have np : (0:ℝ)<n := by exact_mod_cast hn
  have le : |a-c| ≤ δ := by
    dsimp [a,c]
    rw [←sub_div,←sum_sub_distrib,abs_div,abs_of_pos np]
    apply (div_le_iff₀ np).mpr
    calc
      _ ≤ ∑ j ∈ range n, |atTime P j r i - atTime P j u i| := Finset.abs_sum_le_sum_abs _ _
      _ ≤ ∑ _j ∈ range n, δ := by
        apply sum_le_sum;intro j _
        simpa only [time_sub,Pi.sub_apply] using time_bound P hP (r-u) δ residual j i
      _ = δ*n := by simp [mul_comm]
  have bd : |c-h i| ≤ 2*b/n := mean_error P hP fixed rfl hn i hv
  exact (abs_sub_le a c (h i)).trans (add_le_add le bd)

/-- The ensemble mean exists from each reset state, including in reducible chains. -/
theorem population_mean (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j)
    (one : ∀ i, ∑ j, p i j=1) (r : S → ℝ) :
    ∃ h v : S → ℝ, transition p h=h ∧ r=h+(v-transition p v) ∧ ∀ i,
      Filter.Tendsto (fun n : ℕ => (∑ j ∈ range n, atTime (transition p) j r i)/(n:ℝ))
        Filter.atTop (nhds (h i)) := by
  have bounded := stochastic_bounded p hp one
  obtain ⟨h,v,hf,hd⟩ := poisson_solution _ bounded r
  refine ⟨h,v,hf,hd,fun i => Metric.tendsto_atTop.mpr ?_⟩
  intro ε pos
  let b := ∑ j, |v j|
  obtain ⟨m,hm⟩ := exists_nat_gt (2*b/ε)
  refine ⟨m+1,fun n hn => ?_⟩
  have hn' : 0<n := by omega
  have large : 2*b/ε < (n:ℝ) := hm.trans_le (by exact_mod_cast (show m≤n by omega))
  have db := mean_error _ bounded hf hd hn' i (max_bound v)
  rw [Real.dist_eq]
  exact db.trans_lt ((div_lt_iff₀ (by exact_mod_cast hn' : (0:ℝ)<n)).mpr
    (by linarith [(div_lt_iff₀ pos).mp large]))

end NoiseDisplay.MarkovPower

import DSP.Filters.NaiveReuppingLadder.NoiseResponse.ResetOccupancy
import Mathlib.Analysis.Normed.Group.InfiniteSum

noncomputable section
open Filter Finset
open scoped Topology

namespace NoiseDisplay.MarkovPower
variable {S : Type*} [Fintype S] (p : S → S → ℝ)
  (hp : ∀ i j, 0 ≤ p i j) (one : ∀ i, ∑ j, p i j = 1)

private theorem reset_after_steps (i : S) (f : S → ℝ) (m : ℕ) :
    fromReset p hp one i (atTime (transition p) m f) = fromReset p hp one i f := by
  induction m with
  | zero => rfl
  | succ m ih =>
    let g := atTime (transition p) m f
    have e := fromReset_boundary p hp one i g
    have decomp : g = (g-transition p g) + transition p g := by abel
    have a := fromReset_add p hp one i (g-transition p g) (transition p g)
    rw [←decomp,e,zero_add] at a
    exact a.symm.trans ih

/-- A single probability vector works for every block reward at once, at every
    forward depth. In particular each entire expected spectrum is a mixture of
    complete forward spectra. A band, covariance entry or signed linear
    combination uses exactly the same weights. -/
theorem common_spectral_weights (i : S) :
    ∃ w : S → ℝ, (∀ j, 0 ≤ w j) ∧ ∑ j, w j = 1 ∧
      ∀ (m : ℕ) (f : S → ℝ),
        fromReset p hp one i f = ∑ j, w j * atTime (transition p) m f j := by
  classical
  let atom (k j : S) : ℝ := if j=k then 1 else 0
  let w := fun k => fromReset p hp one i (atom k)
  have sum_map (t : Finset S) (v : S → S → ℝ) :
      fromReset p hp one i (∑ k ∈ t, v k) = ∑ k ∈ t, fromReset p hp one i (v k) := by
    induction t using Finset.induction_on with
    | empty => simp only [sum_empty]; exact fromReset_const p hp one i 0
    | insert k t hk ih => simp [sum_insert hk,fromReset_add,ih]
  have repr (g : S → ℝ) : fromReset p hp one i g = ∑ j, w j*g j := by
    have ex : (∑ k, g k • atom k) = g := by
      ext j
      simp [Finset.sum_apply, atom, mul_ite]
    rw [←ex] at ⊢
    rw [sum_map,ex]
    exact sum_congr rfl fun k _ => (fromReset_smul p hp one i (g k) (atom k)).trans (mul_comm ..)
  refine ⟨w,?_,?_,?_⟩
  · intro k
    have h := fromReset_mono p hp one i (f:=fun _ => 0) (g:=atom k)
      (fun j => by simp [atom]; split <;> norm_num)
    simpa [w,fromReset_const] using h
  · simpa [fromReset_const] using (repr (fun _ => 1)).symm
  · intro m f
    rw [←repr,reset_after_steps]

/-- All quadratic analyzers use one second-moment matrix of the wet history.
    The coordinates may include different times and polyphase FIR memories. -/
theorem quadratic_spectrum {T : Type*} [Fintype T] (i : S) (y : S → T → ℝ) :
    ∃ M : T → T → ℝ, ∀ Q : T → T → ℝ,
      fromReset p hp one i (fun s => ∑ u, ∑ v, Q u v * (y s u * y s v)) =
        ∑ u, ∑ v, Q u v * M u v := by
  obtain ⟨w,_,_,repr⟩ := common_spectral_weights p hp one i
  refine ⟨fun u v => ∑ s, w s * (y s u * y s v),fun Q => ?_⟩
  rw [repr 0]
  simp only [atTime, mul_sum]
  rw [sum_comm]
  apply sum_congr rfl
  intro u _
  rw [sum_comm]
  exact sum_congr rfl fun v _ => sum_congr rfl fun s _ => by ring

end NoiseDisplay.MarkovPower

namespace NoiseDisplay.LaggedSpectrum
variable {E : Type*} [NormedAddCommGroup E] [CompleteSpace E]

omit [CompleteSpace E] in
/-- The expected vector difference of two copies one record apart telescopes
    when their future records are shared. No independence across bands enters. -/
theorem finite_correction (a d : ℕ → E) (m : ℕ) (lag : ∀ j, d j = a (j+1) - a j) :
    a 0 + ∑ j ∈ range m, d j = a m := by
  induction m with
  | zero => simp
  | succ m ih => rw [sum_range_succ,←add_assoc,ih,lag]; abel

/-- A summable upper bound on the norm of the expected lagged differences
    gives one long-run power vector and a warm-up bias bound at every age.
    The bound can be supplied by reward agreement or by the probability of
    failure to coalesce times a deterministic reward diameter. -/
theorem tail_error (a d : ℕ → E) (lag : ∀ j, d j = a (j+1) - a j) (b : ℕ → ℝ)
    (hs : Summable b) (hb : ∀ j, ‖d j‖ ≤ b j) :
    ∃ v, Tendsto a atTop (𝓝 v) ∧ ∀ m, ‖v - a m‖ ≤ ∑' j, b (j+m) := by
  have norms : Summable fun j => ‖d j‖ := hs.of_nonneg_of_le (fun j => norm_nonneg _) hb
  have ds : Summable d := norms.of_norm
  refine ⟨a 0 + ∑' j, d j, ?_, ?_⟩
  · have lim := ds.hasSum.tendsto_sum_nat
    simpa only [finite_correction a d _ lag] using (lim.const_add (a 0))
  · intro m
    have split := ds.sum_add_tsum_nat_add m
    rw [←split,←finite_correction a d m lag]
    rw [show a 0 + ((∑ j ∈ range m, d j) + ∑' j, d (j+m)) -
        (a 0 + ∑ j ∈ range m, d j) = ∑' j, d (j+m) by abel]
    have dn : Summable (fun j : ℕ => ‖d (j+m)‖) := (summable_nat_add_iff m).mpr norms
    have bn : Summable (fun j : ℕ => b (j+m)) := (summable_nat_add_iff m).mpr hs
    exact (norm_tsum_le_tsum_norm (f:=fun j : ℕ => d (j+m)) dn).trans
      (dn.tsum_le_tsum (fun j => hb (j+m)) bn)

/-- Any coupling with the required marginals gives the lag bound. After equal
    complete states see the same new noise, their reward difference is zero.
    The diameter is in the chosen norm on the complete power vector. -/
theorem coupled_difference {Ω : Type*} [Fintype Ω] {V : Type*}
    [NormedAddCommGroup V] [NormedSpace ℝ V]
    (mass : Ω → ℝ) (hm : ∀ t, 0 ≤ mass t) (older younger : Ω → V)
    (live : Finset Ω) (same : ∀ t, t ∉ live → older t = younger t)
    (D : ℝ) (diam : ∀ t ∈ live, ‖older t - younger t‖ ≤ D) :
    ‖(∑ t, mass t • older t) - (∑ t, mass t • younger t)‖ ≤ (∑ t ∈ live, mass t)*D := by
  classical
  rw [←sum_sub_distrib]
  simp_rw [←smul_sub]
  have cut : (∑ t, mass t • (older t - younger t)) =
      ∑ t ∈ live, mass t • (older t - younger t) := by
    apply (sum_subset (subset_univ live) ?_).symm
    intro t _ ht
    rw [same t ht,sub_self,smul_zero]
  rw [cut, sum_mul]
  apply (norm_sum_le _ _).trans
  apply sum_le_sum
  intro t ht
  rw [norm_smul,Real.norm_eq_abs,abs_of_nonneg (hm t)]
  exact mul_le_mul_of_nonneg_left (diam t ht) (hm t)

end NoiseDisplay.LaggedSpectrum

namespace NoiseDisplay.Meeting
open NoiseDisplay.MarkovPower
variable {S : Type*} [Fintype S] [Nonempty S]

/-- The kernel contains the pairs that still differ. Missing row mass is the
    probability of meeting on this step and is never reintroduced. On a finite
    set of reachable live pairs, a positive chance of eventual meeting from
    each pair suffices for a geometric bound. Each pair may use a different
    successful noise word and a different number of records. -/
theorem geometric_survival (p : S → S → ℝ) (hp : ∀ i j, 0 ≤ p i j)
    (subprob : ∀ i, ∑ j, p i j ≤ 1)
    (canMeet : ∀ i, ∃ m, atTime (transition p) m (fun _ => 1) i < 1) :
    ∃ L : ℕ, 0 < L ∧ ∃ q : ℝ, 0 ≤ q ∧ q < 1 ∧ ∀ k i,
      atTime (transition p) (k*L) (fun _ => 1) i ≤ q^k := by
  classical
  let P := transition p
  let u := fun n => atTime P n (fun _ => 1)
  have pos (n : ℕ) (i : S) : 0 ≤ u n i := by
    induction n generalizing i with
    | zero => exact zero_le_one
    | succ n ih => exact sum_nonneg fun j _ => mul_nonneg (hp i j) (ih j)
  have dec (n : ℕ) (i : S) : u (n+1) i ≤ u n i := by
    induction n generalizing i with
    | zero => simpa [u,atTime,P,transition] using subprob i
    | succ n ih => exact sum_le_sum fun j _ => mul_le_mul_of_nonneg_left (ih j) (hp i j)
  choose m hm using canMeet
  let L := 1 + ∑ j, m j
  have hL (i : S) : u L i < 1 := by
    have hi : m i ≤ ∑ j, m j := single_le_sum (fun j _ => Nat.zero_le (m j)) (mem_univ i)
    have anti : Antitone (fun n => u n i) := antitone_nat_of_succ_le (fun n => dec n i)
    exact (anti (show m i ≤ L by dsimp [L]; omega)).trans_lt (hm i)
  obtain ⟨s,_,hs⟩ := exists_max_image univ (u L) univ_nonempty
  let q := u L s
  have largest (i : S) : u L i ≤ q := hs i (mem_univ _)
  have block (n : ℕ) (i : S) : u (L+n) i ≤ q*u n i := by
    induction n generalizing i with
    | zero => simpa [u,atTime] using largest i
    | succ n ih =>
      change ∑ j, p i j*u (L+n) j ≤ q * ∑ j, p i j*u n j
      rw [mul_sum]
      apply sum_le_sum
      intro j _
      calc
        p i j * u (L+n) j ≤ p i j * (q * u n j) := mul_le_mul_of_nonneg_left (ih j) (hp i j)
        _ = q * (p i j * u n j) := by ring
  refine ⟨L,by dsimp [L]; omega,q,pos L s,hL s,?_⟩
  intro k i
  change u (k*L) i ≤ q^k
  induction k with
  | zero => simp [u,atTime]
  | succ k ih =>
    rw [Nat.succ_mul,Nat.add_comm,pow_succ']
    exact (block _ i).trans (mul_le_mul_of_nonneg_left ih (pos L s))

/-- Convert a geometric bound on shared-history discrepancies into an explicit
    warm-up error in the norm of the whole power vector, at block boundaries. -/
theorem spectral_bias {E : Type*} [NormedAddCommGroup E] [CompleteSpace E]
    (a : ℕ → E) (L : ℕ) {D q : ℝ} (hq : 0 ≤ q) (hlt : q < 1)
    (lag : ∀ k t, t < L → ‖a (k*L+t+1) - a (k*L+t)‖ ≤ D*q^k) :
    ∃ v, Tendsto (fun k : ℕ => a (k*L)) atTop (𝓝 v) ∧
      ∀ k : ℕ, ‖v - a (k*L)‖ ≤ (L:ℝ)*D*q^k/(1-q) := by
  let b := fun k : ℕ => a (k*L)
  let d := fun k : ℕ => b (k+1)-b k
  have step (k : ℕ) : ‖d k‖ ≤ ((L:ℝ)*D)*q^k := by
    have e := NoiseDisplay.LaggedSpectrum.finite_correction
      (fun t => a (k*L+t)) (fun t => a (k*L+t+1) - a (k*L+t)) L (fun _ => rfl)
    simp only [Nat.add_zero] at e
    have eq : d k = ∑ t ∈ range L, (a (k*L+t+1) - a (k*L+t)) := by
      change a ((k+1)*L) - a (k*L) = _
      rw [Nat.succ_mul,←e]
      abel
    rw [eq]
    calc
      _ ≤ ∑ t ∈ range L, ‖a (k*L+t+1) - a (k*L+t)‖ := norm_sum_le ..
      _ ≤ ∑ _t ∈ range L, D*q^k := sum_le_sum (fun t ht => lag k t (mem_range.mp ht))
      _ = _ := by simp; ring
  have hs := (summable_geometric_of_lt_one hq hlt).mul_left ((L:ℝ)*D)
  obtain ⟨v,lim,tail⟩ := NoiseDisplay.LaggedSpectrum.tail_error b d (fun _ => rfl)
    (fun k => ((L:ℝ)*D)*q^k) hs step
  refine ⟨v,lim,fun k => (tail k).trans_eq ?_⟩
  simp only [pow_add]
  simp_rw [show ∀ n : ℕ, (L:ℝ)*D*(q^n*q^k) = ((L:ℝ)*D*q^k)*q^n by intro n; ring]
  rw [tsum_mul_left,tsum_geometric_of_lt_one hq hlt,div_eq_mul_inv]

end NoiseDisplay.Meeting

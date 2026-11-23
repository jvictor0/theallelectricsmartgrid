import DSP.ResponseMeasurement.Periodic
import Mathlib.Data.Fintype.Pigeonhole
import Mathlib.Tactic

noncomputable section
namespace NoiseDisplay.Record
open Filter Finset DSP.ResponseMeasurement
open scoped Topology

/-- Observing the same state at the same phase is an exact periodicity witness.
    The state includes every feedback memory read by the next record. -/
theorem periodic_tail {S : Type*} (F : S → S) (s : S) {a p : ℕ}
    (h : F^[a+p] s = F^[a] s) :
    Function.Periodic (fun n : ℕ => F^[a+n] s) p := by
  intro i
  change F^[a+(i+p)] s = F^[a+i] s
  rw [show a+(i+p)=i+(a+p) by omega,show a+i=i+a by omega,
    Function.iterate_add_apply, h, Function.iterate_add_apply]

/-- A probability-independent existence theorem for each fixed bounded digital noise record.
    The number of records required to see a return can be enormous. -/
theorem eventually_returns {S : Type*} [Finite S] (F : S → S) (s : S) :
    ∃ a p : ℕ, 0<p ∧ F^[a+p] s=F^[a] s := by
  obtain ⟨i,j,ne,eq⟩ := Finite.exists_ne_map_eq_of_infinite (fun n : ℕ => F^[n] s)
  rcases lt_or_gt_of_ne ne with h|h
  · exact ⟨i,j-i,by omega,by simpa only [show i+(j-i)=j by omega] using eq.symm⟩
  · exact ⟨j,i-j,by omega,by simpa only [show j+(i-j)=i by omega] using eq⟩

/-- A terminating search reads at most card S whole records before the first
    duplicate boundary. Equality of all feedback bits can verify a shorter witness. -/
theorem bounded_search {S : Type*} [Fintype S] (F : S → S) (s : S) :
    ∃ a p : ℕ, 0<p ∧ a+p ≤ Fintype.card S ∧ F^[a+p] s=F^[a] s := by
  obtain ⟨i,j,h,eq⟩ := Fintype.exists_ne_map_eq_of_card_lt
    (fun i : Fin (Fintype.card S+1) => F^[(i:ℕ)] s) (by simp)
  have ne : (i:ℕ) ≠ (j:ℕ) := by exact fun hij => h (Fin.ext hij)
  rcases lt_or_gt_of_ne ne with lt|gt
  · exact ⟨i,j-i,by omega,by omega,by simpa only [show (i:ℕ)+(j-i)=j by omega] using eq.symm⟩
  · exact ⟨j,i-j,by omega,by omega,by simpa only [show (j:ℕ)+(i-j)=i by omega] using eq⟩

theorem prefix_irrelevant (z : ℕ → ℂ) (a : ℕ) {v : ℂ}
    (h : Tendsto (average (fun j => z (a+j))) atTop (𝓝 v)) :
    Tendsto (average z) atTop (𝓝 v) := by
  let head := ∑ i ∈ range a,z i
  apply (tendsto_add_atTop_iff_nat a).1
  have hi := (tendsto_inv_atTop_nhds_zero_nat (𝕜:=ℝ)).comp (tendsto_add_atTop_nat a)
  have h₁ := hi.smul_const head
  have h₂ := (tendsto_natCast_div_add_atTop (a:ℝ)).smul h
  have hh := h₁.add h₂
  simp only [zero_smul,one_smul,zero_add] at hh
  apply hh.congr'
  filter_upwards [eventually_ge_atTop 1] with n hn
  change ((n+a:ℕ):ℝ)⁻¹ • head + ((n:ℝ)/(n+a)) • average (fun j => z (a+j)) n = average z (n+a)
  have hn' : (n:ℝ) ≠ 0 := by exact_mod_cast (show n ≠ 0 by omega)
  unfold average
  rw [show range (n+a)=range (a+n) by rw [Nat.add_comm],sum_range_add,smul_add]
  congr 1
  rw [smul_smul,Nat.cast_add]
  congr 1
  field_simp

theorem cycle_average {S : Type*} (F : S → S) (s : S) (power : S → ℂ)
    {a p : ℕ} (hp : 0<p) (h : F^[a+p] s=F^[a] s) :
    Tendsto (average (fun n => power (F^[n] s))) atTop
      (𝓝 (average (fun n => power (F^[a+n] s)) p)) := by
  apply prefix_irrelevant (a:=a)
  exact average_of_periodic _ p hp (fun n => congrArg power (periodic_tail F s h n))

theorem reset_limit {S : Type*} [Finite S] (F : S → S) (s : S) (power : S → ℂ) :
    ∃ a p : ℕ, 0<p ∧ F^[a+p] s=F^[a] s ∧
      Tendsto (average (fun n => power (F^[n] s))) atTop
        (𝓝 (average (fun n => power (F^[a+n] s)) p)) := by
  obtain ⟨a,p,hp,he⟩ := eventually_returns F s
  exact ⟨a,p,hp,he,cycle_average F s power hp he⟩

end NoiseDisplay.Record

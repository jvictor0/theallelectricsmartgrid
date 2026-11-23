import DSP.Analysis.AttractingBlock

/-! A block can shrink discrepancies even when a single sample can increase them.
Apply this result to continuous sections over the forcing phase, with the uniform
metric. The closed tube can follow a different center at each phase. -/

noncomputable section
namespace DSP.TrappedGraph
open Metric
open scoped NNReal

/-- Error accumulated along a shorter initial prefix, or from the beginning of
a full block, bounds every iterate started at the proposed center. Only histories
which have stayed in the tube so far enter the block estimate. -/
theorem trapped_of_block_bounds (error : ℕ → ℝ) (length : ℕ) (hlen : 0 < length)
    (radius rate forcing : ℝ) (hrate : 0 ≤ rate) (hradius : 0 ≤ radius)
    (hclose : rate * radius + forcing ≤ radius)
    (hprefix : ∀ n < length, (∀ j < n, error j ≤ radius) → error n ≤ forcing)
    (hblock : ∀ n, length ≤ n → (∀ j < n, error j ≤ radius) →
      error n ≤ rate * error (n-length) + forcing) : ∀ n, error n ≤ radius := by
  intro n
  induction n using Nat.strong_induction_on with
  | h n ih =>
    by_cases hsmall : n < length
    · exact (hprefix n hsmall ih).trans (by linarith [mul_nonneg hrate hradius])
    · have hold := ih (n-length) (by omega)
      exact (hblock n (by omega) ih).trans
        ((add_le_add (mul_le_mul_of_nonneg_left hold hrate) (le_refl forcing)).trans hclose)

variable {X : Type*} [MetricSpace X] [CompleteSpace X]

/-- One trapped forward orbit and uniform contraction over blocks of trapped
iterates isolate a fixed point of the *single-step* graph transform. -/
theorem fixed_of_trapped_blocks (T : X → X) (hT : Continuous T)
    (tube : Set X) (hc : IsClosed tube) (center : X)
    (hcenter : ∀ n : ℕ, T^[n] center ∈ tube) (length : ℕ) (rate : ℝ≥0)
    (hrate : rate < 1)
    (hblock : ∀ x y, (∀ n : ℕ, T^[n] x ∈ tube) → (∀ n : ℕ, T^[n] y ∈ tube) →
      dist (T^[length] x) (T^[length] y) ≤ rate * dist x y) :
    ∃ p ∈ tube, T p = p ∧ ∀ y ∈ tube, T y = y → y = p := by
  let S : Set X := ⋂ n : ℕ, (T^[n]) ⁻¹' tube
  have mem (x : X) : x ∈ S ↔ ∀ n : ℕ, T^[n] x ∈ tube := Set.mem_iInter
  have hs : IsClosed S := isClosed_iInter fun n => hc.preimage (hT.iterate n)
  letI : CompleteSpace S := hs.completeSpace_coe
  letI : Nonempty S := ⟨⟨center, (mem center).mpr hcenter⟩⟩
  have stays (x : S) (n : ℕ) : T^[n] x.val ∈ S := by
    apply (mem _).mpr
    intro k
    rw [← Function.iterate_add_apply]
    exact (mem x.val).mp x.property _
  let block : S → S := fun x => ⟨T^[length] x.val, stays x length⟩
  have con : ContractingWith rate block := ⟨hrate, LipschitzWith.of_dist_le_mul
    (fun x y => hblock _ _ ((mem _).mp x.property) ((mem _).mp y.property))⟩
  let p := con.fixedPoint block
  have fp : T^[length] p.val = p.val := congrArg Subtype.val con.fixedPoint_isFixedPt
  let t : S := ⟨T p.val, by simpa using stays p 1⟩
  have ft : block t = t := by
    apply Subtype.ext
    change T^[length] (T p.val) = T p.val
    rw [← Function.iterate_succ_apply, Function.iterate_succ_apply', fp]
  have hp : T p.val = p.val := congrArg Subtype.val (con.fixedPoint_unique ft)
  refine ⟨p.val, (mem _).mp p.property 0, hp, ?_⟩
  intro y hy hfix
  have iter (n : ℕ) : T^[n] y = y := Function.IsFixedPt.iterate hfix n
  let z : S := ⟨y, (mem y).mpr (fun n => (iter n).symm ▸ hy)⟩
  have fz : block z = z := Subtype.ext (iter length)
  exact congrArg Subtype.val (con.fixedPoint_unique fz)

end DSP.TrappedGraph

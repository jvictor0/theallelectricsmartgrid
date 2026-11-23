import DSP.Analysis.TrigPolynomial.Enclosure

/-! Prepared, rounded harmonic witnesses for Fourier cell checks. Only the
base angle needs transcendental evaluation. Every later harmonic is checked
by exact rational angle-addition arithmetic, and the whole-cell width follows
from the Lipschitz bound one. Rounded carrier boxes prevent denominator growth. -/

namespace DSP.TrigPolynomial
open LeanCert.Core DSP.IntervalArithmetic

abbrev Carrier := Fin 2 → IntervalRat

def carrierCheck (base : Carrier) (carrier : ℕ → Carrier) (N : ℕ) : Bool :=
  contains (carrier 0 0) (IntervalRat.singleton 1) &&
  contains (carrier 0 1) (IntervalRat.singleton 0) &&
  decide (∀ n : Fin N,
    contains (carrier (n.val+1) 0)
      (((carrier n.val 0).mul (base 0)).sub ((carrier n.val 1).mul (base 1))) = true ∧
    contains (carrier (n.val+1) 1)
      (((carrier n.val 1).mul (base 0)).add ((carrier n.val 0).mul (base 1))) = true)

theorem carrierCheck_sound (base : Carrier) (carrier : ℕ → Carrier) (N : ℕ)
    (hc : carrierCheck base carrier N = true) (θ : ℝ)
    (hcos : Real.cos θ ∈ base 0) (hsin : Real.sin θ ∈ base 1) (n : ℕ) (hn : n ≤ N) :
    Real.cos ((n:ℝ)*θ) ∈ carrier n 0 ∧ Real.sin ((n:ℝ)*θ) ∈ carrier n 1 := by
  simp only [carrierCheck, Bool.and_eq_true, decide_eq_true_eq] at hc
  induction n with
  | zero =>
    simpa only [Nat.cast_zero, zero_mul, Real.cos_zero, Real.sin_zero, Rat.cast_one, Rat.cast_zero] using
      And.intro (contains_sound hc.1.1 (IntervalRat.mem_singleton 1))
        (contains_sound hc.1.2 (IntervalRat.mem_singleton 0))
  | succ n ih =>
    obtain ⟨hcn,hsn⟩ := ih (by omega)
    obtain ⟨hcc,hcs⟩ := hc.2 ⟨n,by omega⟩
    have he : ((n+1:ℕ):ℝ)*θ = (n:ℝ)*θ+θ := by push_cast; ring
    rw [he, Real.cos_add, Real.sin_add]
    exact ⟨contains_sound hcc (IntervalRat.mem_sub (IntervalRat.mem_mul hcn hcos)
      (IntervalRat.mem_mul hsn hsin)),
      contains_sound hcs (IntervalRat.mem_add (IntervalRat.mem_mul hsn hcos)
        (IntervalRat.mem_mul hcn hsin))⟩

def harmonicEnclosure {count : ℕ} (p : TrigPolynomial count) (angle : IntervalRat)
    (carrier : ℕ → Carrier) : IntervalRat :=
  IntervalArithmetic.sum (fun i =>
    ((IntervalTrig.widen (carrier i.val 0) (angle.scale i.val)).scale (p.cosine i)).add
      ((IntervalTrig.widen (carrier i.val 1) (angle.scale i.val)).scale (p.sine i)))

theorem mem_harmonicEnclosure {count : ℕ} (p : TrigPolynomial count) (angle : IntervalRat)
    (carrier : ℕ → Carrier)
    (hc : ∀ i : Fin count, Real.cos ((i.val:ℝ)*(angle.midpoint:ℝ)) ∈ carrier i.val 0 ∧
      Real.sin ((i.val:ℝ)*(angle.midpoint:ℝ)) ∈ carrier i.val 1)
    (θ : ℝ) (hθ : θ ∈ angle) : p.eval (θ:Real.Angle) ∈ p.harmonicEnclosure angle carrier := by
  rw [p.eval_coe]
  apply mem_sum
  intro i
  have hi : (i.val:ℝ)*θ ∈ angle.scale i.val := by
    simpa only [Rat.cast_natCast] using IntervalRat.mem_scale (i.val:ℚ) hθ
  have hmid : ((angle.scale i.val).midpoint:ℝ) = (i.val:ℝ)*(angle.midpoint:ℝ) := by
    simp only [IntervalRat.scale, Nat.cast_nonneg, ↓reduceDIte, IntervalRat.midpoint]
    push_cast
    ring
  have hd : |(i.val:ℝ)*θ-(i.val:ℝ)*(angle.midpoint:ℝ)| ≤ ((angle.scale i.val).width:ℝ)/2 := by
    simpa only [hmid] using IntervalTrig.midpoint_distance (angle.scale i.val) _ hi
  apply IntervalRat.mem_add
  · apply IntervalRat.mem_scale
    exact IntervalTrig.widen_mem _ _ _ _ (hc i).1 ((Real.abs_cos_sub_cos_le _ _).trans hd)
  · apply IntervalRat.mem_scale
    exact IntervalTrig.widen_mem _ _ _ _ (hc i).2 ((Real.abs_sin_sub_sin_le _ _).trans hd)

end DSP.TrigPolynomial

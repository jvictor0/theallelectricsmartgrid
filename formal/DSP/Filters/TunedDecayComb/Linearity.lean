import DSP.Filters.TunedDecayComb.Proofs

namespace DSP.TunedDecayComb

variable {capacity : ℕ} {K : Type} [CommRing K]

def Branch.combine (a b : K) (s t : Branch K) : Branch K :=
  ⟨a * s.allpass + b * t.allpass, a * s.ic1 + b * t.ic1,
    a * s.ic2 + b * t.ic2, a * s.y + b * t.y⟩

theorem branch_superposition (c : Coefficients capacity K) (a b x y d e : K)
    (s t : Branch K) :
    branchStep c (Branch.combine a b s t) (a * x + b * y) (a * d + b * e) =
      Branch.combine a b (branchStep c s x d) (branchStep c t y e) := by
  cases hb : c.bypass <;>
    simp only [branchStep, hb, Bool.false_eq_true, if_false, if_true,
      NormalizedAllPass.step, Branch.combine]
  all_goals congr 1 <;> ring

/-- Linearity concerns numerical histories, not addition of write-head indices.
The three rings may have different physical heads representing these histories. -/
def State.Combination (a b : K) (s t v : State capacity K) : Prop :=
  (∀ i, v.buffer.read i = a * s.buffer.read i + b * t.buffer.read i) ∧
  v.first = Branch.combine a b s.first t.first ∧
  v.second = Branch.combine a b s.second t.second ∧ v.y = a * s.y + b * t.y

theorem buffer_superposition (a b x y : K) (s t v : DelayBuffer capacity K)
    (h : ∀ i, v.read i = a * s.read i + b * t.read i) :
    ∀ i, (v.push (a * x + b * y)).read i =
      a * (s.push x).read i + b * (t.push y).read i := by
  intro i
  by_cases hi : i.val = 0
  · have he : i = ⟨0, by omega⟩ := Fin.ext hi
    rw [he, DelayBuffer.read_push_zero, DelayBuffer.read_push_zero, DelayBuffer.read_push_zero]
  · rw [DelayBuffer.read_push_succ _ _ _ (by omega),
      DelayBuffer.read_push_succ _ _ _ (by omega), DelayBuffer.read_push_succ _ _ _ (by omega)]
    exact h _

theorem step_superposition (c₀ c₁ : Coefficients capacity K) (mix : Option K)
    (a b x y : K) (s t v : State capacity K) (h : State.Combination a b s t v) :
    State.Combination a b (step c₀ c₁ mix s ⟨x⟩) (step c₀ c₁ mix t ⟨y⟩)
      (step c₀ c₁ mix v ⟨a * x + b * y⟩) := by
  obtain ⟨hd, hf, hg, _⟩ := h
  have h₀ : branchStep c₀ v.first (a * x + b * y) (v.buffer.read c₀.age) =
      Branch.combine a b (branchStep c₀ s.first x (s.buffer.read c₀.age))
        (branchStep c₀ t.first y (t.buffer.read c₀.age)) := by
    rw [hf, hd, branch_superposition]
  have h₁ : branchStep c₁ v.second (a * x + b * y) (v.buffer.read c₁.age) =
      Branch.combine a b (branchStep c₁ s.second x (s.buffer.read c₁.age))
        (branchStep c₁ t.second y (t.buffer.read c₁.age)) := by
    rw [hg, hd, branch_superposition]
  have hy : (step c₀ c₁ mix v ⟨a * x + b * y⟩).y =
      a * (step c₀ c₁ mix s ⟨x⟩).y + b * (step c₀ c₁ mix t ⟨y⟩).y := by
    cases mix with
    | none => exact congrArg Branch.y h₀
    | some m => simp only [step, h₀, h₁, Branch.combine]; ring
  refine ⟨?_, ?_, ?_, hy⟩
  · rw [step_buffer, step_buffer, step_buffer, hy]
    exact buffer_superposition a b _ _ _ _ _ hd
  · cases mix <;> exact h₀
  · cases mix with
    | none => exact hg
    | some _ => exact h₁

/-- Superposition for any common external coefficient, tap and mix schedule,
including arbitrary initial memory. No stability or normalization is needed. -/
theorem trajectory_superposition (c₀ c₁ : ℕ → Coefficients capacity K)
    (mix : ℕ → Option K) (a b : K) (x y : ℕ → K) (s t v : ℕ → State capacity K)
    (h0 : State.Combination a b (s 0) (t 0) (v 0))
    (hs : ∀ n, s (n + 1) = step (c₀ n) (c₁ n) (mix n) (s n) ⟨x n⟩)
    (ht : ∀ n, t (n + 1) = step (c₀ n) (c₁ n) (mix n) (t n) ⟨y n⟩)
    (hv : ∀ n, v (n + 1) = step (c₀ n) (c₁ n) (mix n) (v n) ⟨a * x n + b * y n⟩)
    (n : ℕ) : State.Combination a b (s n) (t n) (v n) := by
  induction n with
  | zero => exact h0
  | succ n ih => rw [hs, ht, hv]; exact step_superposition _ _ _ _ _ _ _ _ _ _ ih

end DSP.TunedDecayComb

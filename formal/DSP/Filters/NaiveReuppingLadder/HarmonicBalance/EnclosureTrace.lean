import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Enclosure

/-! Compose checked single-sample enclosures over the actual sample run. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure
open LeanCert.Core DSP.IntervalArithmetic
open scoped Matrix

theorem values_run (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : ℕ → IntervalRat)
    (history saturation : ℕ → Box) (depth N : ℕ)
    (hcheck : ∀ n < N, valuesCheck c (input n) (history n) (history (n+1)) (saturation n) depth = true)
    (u : ℕ → Sample ℝ) (hu : ∀ n < N, (u n).x ∈ input n)
    (x : Fin 4 → ℝ) (hx : ∀ i, x i ∈ history 0 i) :
    ∀ n ≤ N, ∀ i, Step.states (audioStep s) u x n i ∈ history n i := by
  intro n hn
  induction n with
  | zero => exact hx
  | succ n ih =>
    rw [Step.states_succ]
    exact valuesCheck_sound s hd c hc (input n) (history n) (history (n+1)) (saturation n)
      depth (hcheck n (by omega)) _ (ih (by omega)) (u n) (hu n (by omega))

/-- A matrix in the final box represents the exact difference of any two
trajectories whose initial difference is represented by the initial matrix.
This supports arbitrary rational coordinate transforms. -/
theorem pairs_run (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : ℕ → IntervalRat)
    (history saturation : ℕ → Box) (matrices : ℕ → MatrixBox) (depth N : ℕ)
    (hv : ∀ n < N, valuesCheck c (input n) (history n) (history (n+1)) (saturation n) depth = true)
    (hm : ∀ n < N, matrixCheck c (matrices n) (matrices (n+1)) (saturation n) = true)
    (u : ℕ → Sample ℝ) (hu : ∀ n < N, (u n).x ∈ input n)
    (x y delta : Fin 4 → ℝ) (hx : ∀ i, x i ∈ history 0 i) (hy : ∀ i, y i ∈ history 0 i)
    (initialMatrix : Matrix (Fin 4) (Fin 4) ℝ)
    (hi : ∀ i j, initialMatrix i j ∈ matrices 0 i j) (he : x-y = initialMatrix *ᵥ delta) :
    ∃ matrix : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, matrix i j ∈ matrices N i j) ∧
      Step.states (audioStep s) u x N-Step.states (audioStep s) u y N = matrix *ᵥ delta := by
  have hnx := values_run s hd c hc input history saturation depth N hv u hu x hx
  have hny := values_run s hd c hc input history saturation depth N hv u hu y hy
  have hresult (n : ℕ) (hn : n ≤ N) :
      ∃ matrix : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, matrix i j ∈ matrices n i j) ∧
        Step.states (audioStep s) u x n-Step.states (audioStep s) u y n = matrix *ᵥ delta := by
    induction n with
    | zero => exact ⟨initialMatrix, hi, he⟩
    | succ n ih =>
      obtain ⟨matrix, hmat, hxy⟩ := ih (by omega)
      simpa only [Step.states_succ] using pairCheck_sound s hd c hc (input n)
        (history n) (history (n+1)) (saturation n) (matrices n) (matrices (n+1)) depth
        (hv n (by omega)) (hm n (by omega)) _ _ delta (hnx n (by omega)) (hny n (by omega))
        (u n) (hu n (by omega)) matrix hmat hxy
  exact hresult N le_rfl

def outputRange (c : CoefficientBox) (saturation : IntervalRat) : IntervalRat :=
  (saturation.mul c.inverseNormalization).mul (c.feedback.add (IntervalRat.singleton 1))

theorem output_mem (s : NaiveReuppingLadder ℝ) (c : CoefficientBox) (hc : c.Valid s)
    (history saturation : Box) (depth : ℕ)
    (hcheck : IntervalTanh.check ((history 3).mul c.gain) (saturation 3) depth = true)
    (x : Fin 4 → ℝ) (hx : ∀ i, x i ∈ history i) :
    audioObservation s x ∈ outputRange c (saturation 3) := by
  exact IntervalRat.mem_mul (saturation_mem s c hc _ _ depth hcheck _ (hx 3))
    (IntervalRat.mem_add hc.2.2.1 (by simpa using IntervalRat.mem_singleton 1))

end DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure

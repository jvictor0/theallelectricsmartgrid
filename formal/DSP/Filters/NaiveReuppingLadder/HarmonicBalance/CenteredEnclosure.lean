import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.EnclosureTrace
import DSP.Analysis.IntervalTaylor

/-! Centered value propagation without interval wrapping of the histories.
An exact secant matrix carries the difference from a checked center orbit.
The four new stages are enclosed in order: each stage's enclosure validates
the tanh secant used by the following stage. No circular tube assumption is
made, and the old fourth-stage feedback remains in the first row. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure
open LeanCert.Core DSP.IntervalArithmetic
open scoped Matrix

def centeredCheck (c : CoefficientBox) (input : IntervalRat)
    (oldCenter nextCenter centerSaturation whole saturation : Box)
    (oldMatrix nextMatrix : MatrixBox) (width : Fin 4 → ℚ) (depth : ℕ) : Bool :=
  valuesCheck c input oldCenter nextCenter centerSaturation depth &&
  decide (∀ i, contains (whole i) (nextCenter i) = true) &&
  decide (∀ i, IntervalTaylor.radiusCheck (nextCenter i) (whole i)
    (IntervalTaylor.firstOrderRadius (nextMatrix i) width) = true) &&
  matrixCheck c oldMatrix nextMatrix saturation &&
  decide (∀ i, IntervalTanh.check ((whole i).mul c.gain) (saturation i) depth = true)

theorem centered_row_mem (center whole : IntervalRat) (matrix : Fin 4 → IntervalRat)
    (width : Fin 4 → ℚ)
    (hc : IntervalTaylor.radiusCheck center whole (IntervalTaylor.firstOrderRadius matrix width) = true)
    (x y : ℝ) (hy : y ∈ center) (row delta : Fin 4 → ℝ)
    (hm : ∀ j, row j ∈ matrix j) (hd : ∀ j, |delta j| ≤ (width j:ℝ))
    (he : x-y = ∑ j, row j*delta j) : x ∈ whole := by
  apply IntervalTaylor.radiusCheck_sound center whole _ hc y x hy
  rw [he]
  have h := CenteredTaylor.linear_bound row (fun j => (absoluteBound (matrix j):ℝ))
    delta (fun j => (width j:ℝ)) (fun j => norm_le_absoluteBound (hm j)) hd
  simpa [IntervalTaylor.firstOrderRadius] using h

noncomputable section

/-- Initialize the exact secant matrix from a curved chart's derivatives on
a center-to-point segment. Different output rows may choose different mean
value points; no false vector-valued single-point mean value claim is used. -/
theorem initial_secant (f : ℝ → Fin 4 → ℝ)
    (jacobian : ℝ → Matrix (Fin 4) (Fin 4) ℝ) (boxes : MatrixBox) (delta : Fin 4 → ℝ)
    (hderiv : ∀ t ∈ Set.Icc (0:ℝ) 1, ∀ i,
      HasDerivAt (fun r => f r i) (∑ j, jacobian t i j*delta j) t)
    (hmem : ∀ t ∈ Set.Icc (0:ℝ) 1, ∀ i j, jacobian t i j ∈ boxes i j) :
    ∃ matrix : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, matrix i j ∈ boxes i j) ∧
      f 1-f 0 = matrix *ᵥ delta := by
  have hrow (i : Fin 4) : ∃ t ∈ Set.Ioo (0:ℝ) 1,
      f 1 i-f 0 i = ∑ j, jacobian t i j*delta j := by
    obtain ⟨t,ht,he⟩ := exists_hasDerivAt_eq_slope (fun t => f t i)
      (fun t => ∑ j, jacobian t i j*delta j) (by norm_num : (0:ℝ) < 1)
      (fun t ht => (hderiv t ht i).continuousAt.continuousWithinAt)
      (fun t ht => hderiv t ⟨ht.1.le,ht.2.le⟩ i)
    exact ⟨t,ht,by simpa only [sub_zero, div_one] using he.symm⟩
  choose point hpoint heq using hrow
  exact ⟨fun i j => jacobian (point i) i j,
    fun i j => hmem (point i) ⟨(hpoint i).1.le,(hpoint i).2.le⟩ i j, funext heq⟩

theorem centeredCheck_sound (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : IntervalRat)
    (oldCenter nextCenter centerSaturation whole saturation : Box)
    (oldMatrix nextMatrix : MatrixBox) (width : Fin 4 → ℚ) (depth : ℕ)
    (hcheck : centeredCheck c input oldCenter nextCenter centerSaturation whole saturation
      oldMatrix nextMatrix width depth = true)
    (x y delta : Fin 4 → ℝ) (hy : ∀ i, y i ∈ oldCenter i)
    (u : Sample ℝ) (hu : u.x ∈ input)
    (matrix : Matrix (Fin 4) (Fin 4) ℝ) (hm : ∀ i j, matrix i j ∈ oldMatrix i j)
    (hdelta : ∀ j, |delta j| ≤ (width j:ℝ)) (he : x-y = matrix *ᵥ delta) :
    (∀ i, audioStep s y u i ∈ nextCenter i) ∧
    (∀ i, audioStep s x u i ∈ whole i) ∧
    ∃ m : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, m i j ∈ nextMatrix i j) ∧
      audioStep s x u-audioStep s y u = m *ᵥ delta := by
  simp only [centeredCheck, Bool.and_eq_true, decide_eq_true_eq] at hcheck
  rcases hcheck with ⟨⟨⟨⟨hvalues,hcenter⟩,hradius⟩,hmat⟩,hsat⟩
  have hny := valuesCheck_sound s hd c hc input oldCenter nextCenter centerSaturation depth
    hvalues y hy u hu
  have hnyWhole (i : Fin 4) : audioStep s y u i ∈ whole i := contains_sound (hcenter i) (hny i)
  simp only [matrixCheck, decide_eq_true_eq] at hmat
  obtain ⟨x0,x1,x2,x3⟩ := step_equations s hd x u
  obtain ⟨y0,y1,y2,y3⟩ := step_equations s hd y u
  have he' (i : Fin 4) : x i-y i = ∑ j, matrix i j*delta j := congrFun he i
  let m0 := row (1-s.alpha) (-s.alpha*s.baseFeedback) (matrix 0) (matrix 3)
  have hm0 (j : Fin 4) : m0 j ∈ nextMatrix 0 j := by
    have h := contains_sound (hmat j).1 (IntervalRat.mem_sub
      (IntervalRat.mem_mul hc.2.1 (hm 0 j))
      (IntervalRat.mem_mul (IntervalRat.mem_mul hc.1 hc.2.2.1) (hm 3 j)))
    convert h using 1
    dsimp [m0,row]
    ring
  have he0 : audioStep s x u 0-audioStep s y u 0 = ∑ j, m0 j*delta j := by
    rw [show (∑ j, m0 j*delta j) =
      (1-s.alpha)*(∑ j, matrix 0 j*delta j)+(-s.alpha*s.baseFeedback)*(∑ j, matrix 3 j*delta j)
      from row_dot .., ← he' 0, ← he' 3, x0, y0]
    ring
  have hx0 := centered_row_mem (nextCenter 0) (whole 0) (nextMatrix 0) width (hradius 0)
    _ _ (hny 0) m0 delta hm0 hdelta he0
  obtain ⟨eta0,heta0,heq0⟩ := DSP.IntervalArithmetic.exists_secant s.saturation c.gain
    c.inverseNormalization (whole 0) (saturation 0) depth hc.2.2.2.1 hc.2.2.2.2 (hsat 0)
    _ _ hx0 (hnyWhole 0)
  let m1 := row (1-s.alpha) (s.alpha*eta0) (matrix 1) m0
  have hm1 (j : Fin 4) : m1 j ∈ nextMatrix 1 j :=
    contains_sound (hmat j).2.1 (IntervalRat.mem_add (IntervalRat.mem_mul hc.2.1 (hm 1 j))
      (IntervalRat.mem_mul (IntervalRat.mem_mul hc.1 heta0) (hm0 j)))
  have he1 : audioStep s x u 1-audioStep s y u 1 = ∑ j, m1 j*delta j := by
    change _ = ∑ j, row _ _ _ _ j*delta j
    rw [row_dot, ← he0, ← he' 1, x1, y1]
    linear_combination s.alpha*heq0
  have hx1 := centered_row_mem (nextCenter 1) (whole 1) (nextMatrix 1) width (hradius 1)
    _ _ (hny 1) m1 delta hm1 hdelta he1
  obtain ⟨eta1,heta1,heq1⟩ := DSP.IntervalArithmetic.exists_secant s.saturation c.gain
    c.inverseNormalization (whole 1) (saturation 1) depth hc.2.2.2.1 hc.2.2.2.2 (hsat 1)
    _ _ hx1 (hnyWhole 1)
  let m2 := row (1-s.alpha) (s.alpha*eta1) (matrix 2) m1
  have hm2 (j : Fin 4) : m2 j ∈ nextMatrix 2 j :=
    contains_sound (hmat j).2.2.1 (IntervalRat.mem_add (IntervalRat.mem_mul hc.2.1 (hm 2 j))
      (IntervalRat.mem_mul (IntervalRat.mem_mul hc.1 heta1) (hm1 j)))
  have he2 : audioStep s x u 2-audioStep s y u 2 = ∑ j, m2 j*delta j := by
    change _ = ∑ j, row _ _ _ _ j*delta j
    rw [row_dot, ← he1, ← he' 2, x2, y2]
    linear_combination s.alpha*heq1
  have hx2 := centered_row_mem (nextCenter 2) (whole 2) (nextMatrix 2) width (hradius 2)
    _ _ (hny 2) m2 delta hm2 hdelta he2
  obtain ⟨eta2,heta2,heq2⟩ := DSP.IntervalArithmetic.exists_secant s.saturation c.gain
    c.inverseNormalization (whole 2) (saturation 2) depth hc.2.2.2.1 hc.2.2.2.2 (hsat 2)
    _ _ hx2 (hnyWhole 2)
  let m3 := row (1-s.alpha) (s.alpha*eta2) (matrix 3) m2
  have hm3 (j : Fin 4) : m3 j ∈ nextMatrix 3 j :=
    contains_sound (hmat j).2.2.2 (IntervalRat.mem_add (IntervalRat.mem_mul hc.2.1 (hm 3 j))
      (IntervalRat.mem_mul (IntervalRat.mem_mul hc.1 heta2) (hm2 j)))
  have he3 : audioStep s x u 3-audioStep s y u 3 = ∑ j, m3 j*delta j := by
    change _ = ∑ j, row _ _ _ _ j*delta j
    rw [row_dot, ← he2, ← he' 3, x3, y3]
    linear_combination s.alpha*heq2
  have hx3 := centered_row_mem (nextCenter 3) (whole 3) (nextMatrix 3) width (hradius 3)
    _ _ (hny 3) m3 delta hm3 hdelta he3
  refine ⟨hny,?_,![m0,m1,m2,m3],?_,?_⟩
  · intro i
    fin_cases i <;> assumption
  · intro i j
    fin_cases i
    · exact hm0 j
    · exact hm1 j
    · exact hm2 j
    · exact hm3 j
  · funext i
    fin_cases i <;> assumption

/-- Whole-block centered replay. The matrix is an exact finite-difference
representation for each pair, not a numerical approximation to a Jacobian. -/
theorem centered_run (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : ℕ → IntervalRat)
    (center whole centerSaturation saturation : ℕ → Box) (matrices : ℕ → MatrixBox)
    (width : Fin 4 → ℚ) (depth N : ℕ)
    (hcheck : ∀ n < N, centeredCheck c (input n) (center n) (center (n+1))
      (centerSaturation n) (whole (n+1)) (saturation n) (matrices n) (matrices (n+1)) width depth = true)
    (u : ℕ → Sample ℝ) (hu : ∀ n < N, (u n).x ∈ input n)
    (x y delta : Fin 4 → ℝ) (hx : ∀ i, x i ∈ whole 0 i) (hy : ∀ i, y i ∈ center 0 i)
    (initialMatrix : Matrix (Fin 4) (Fin 4) ℝ)
    (hm : ∀ i j, initialMatrix i j ∈ matrices 0 i j)
    (hdelta : ∀ j, |delta j| ≤ (width j:ℝ)) (he : x-y = initialMatrix *ᵥ delta) :
    ∀ n ≤ N, (∀ i, Step.states (audioStep s) u y n i ∈ center n i) ∧
      (∀ i, Step.states (audioStep s) u x n i ∈ whole n i) ∧
      ∃ m : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, m i j ∈ matrices n i j) ∧
        Step.states (audioStep s) u x n-Step.states (audioStep s) u y n = m *ᵥ delta := by
  intro n hn
  induction n with
  | zero => exact ⟨hy,hx,initialMatrix,hm,he⟩
  | succ n ih =>
    obtain ⟨hy',_,m,hm',he'⟩ := ih (by omega)
    simpa only [Step.states_succ] using centeredCheck_sound s hd c hc (input n)
      (center n) (center (n+1)) (centerSaturation n) (whole (n+1)) (saturation n)
      (matrices n) (matrices (n+1)) width depth (hcheck n (by omega))
      _ _ delta hy' (u n) (hu n (by omega)) m hm' hdelta he'

end
end DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure

import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Attraction
import DSP.Analysis.IntervalArithmetic

/-! Witness replay for the actual detector-disabled sample step. Certificate
data proposes boxes; the checks below establish containment in those boxes.
This is proof-side validation, with no new executable filter implementation. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure
open LeanCert.Core DSP.IntervalArithmetic
open scoped Matrix

abbrev Box := Fin 4 → IntervalRat
abbrev MatrixBox := Fin 4 → Fin 4 → IntervalRat

structure CoefficientBox where
  alpha : IntervalRat
  beta : IntervalRat
  feedback : IntervalRat
  gain : IntervalRat
  inverseNormalization : IntervalRat

def CoefficientBox.Valid (c : CoefficientBox) (s : NaiveReuppingLadder ℝ) : Prop :=
  s.alpha ∈ c.alpha ∧ 1-s.alpha ∈ c.beta ∧ s.baseFeedback ∈ c.feedback ∧
    s.saturation.gain ∈ c.gain ∧ s.saturation.tanhGain⁻¹ ∈ c.inverseNormalization

def firstRange (c : CoefficientBox) (input : IntervalRat) (old : Box) : IntervalRat :=
  (c.alpha.mul (input.sub (c.feedback.mul (old 3)))).add (c.beta.mul (old 0))

def stageRange (c : CoefficientBox) (previousSaturation old : IntervalRat) : IntervalRat :=
  (c.alpha.mul (previousSaturation.mul c.inverseNormalization)).add (c.beta.mul old)

def valuesCheck (c : CoefficientBox) (input : IntervalRat) (old next saturation : Box)
    (depth : ℕ) : Bool :=
  contains (next 0) (firstRange c input old) &&
    (decide (∀ i, IntervalTanh.check ((next i).mul c.gain) (saturation i) depth = true)) &&
    contains (next 1) (stageRange c (saturation 0) (old 1)) &&
    contains (next 2) (stageRange c (saturation 1) (old 2)) &&
    contains (next 3) (stageRange c (saturation 2) (old 3))

theorem step_equations (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (x : Fin 4 → ℝ) (u : Sample ℝ) :
    let next := audioStep s x u
    next 0 = s.alpha*(u.x-s.baseFeedback*x 3)+(1-s.alpha)*x 0 ∧
    next 1 = s.alpha*NormalizedTanh.apply s.saturation (next 0)+(1-s.alpha)*x 1 ∧
    next 2 = s.alpha*NormalizedTanh.apply s.saturation (next 1)+(1-s.alpha)*x 2 ∧
    next 3 = s.alpha*NormalizedTanh.apply s.saturation (next 2)+(1-s.alpha)*x 3 := by
  simp [audioStep, audioHistory, step, withHistory, effectiveFeedback, hd,
    OnePoleZOHLowPass.onePoleStep_eq, fma_eq]

theorem saturation_mem (s : NaiveReuppingLadder ℝ) (c : CoefficientBox)
    (hc : c.Valid s) (input output : IntervalRat) (depth : ℕ)
    (hcheck : IntervalTanh.check (input.mul c.gain) output depth = true)
    (x : ℝ) (hx : x ∈ input) :
    NormalizedTanh.apply s.saturation x ∈ output.mul c.inverseNormalization := by
  have ht := IntervalTanh.check_sound _ _ _ hcheck (x*s.saturation.gain)
    (IntervalRat.mem_mul hx hc.2.2.2.1)
  have h := IntervalRat.mem_mul ht hc.2.2.2.2
  simpa only [NormalizedTanh.apply, DSP.Tanh.tanh, div_eq_mul_inv] using h

/-- Every proposed output history encloses the actual sample update. The
fourth tanh witness also covers the output saturator used in measurements. -/
theorem valuesCheck_sound (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : IntervalRat) (old next saturation : Box)
    (depth : ℕ) (hcheck : valuesCheck c input old next saturation depth = true)
    (x : Fin 4 → ℝ) (hx : ∀ i, x i ∈ old i) (u : Sample ℝ) (hu : u.x ∈ input) :
    ∀ i, audioStep s x u i ∈ next i := by
  simp only [valuesCheck, Bool.and_eq_true, decide_eq_true_eq] at hcheck
  rcases hcheck with ⟨⟨⟨⟨h0, hs⟩, h1⟩, h2⟩, h3⟩
  obtain ⟨e0,e1,e2,e3⟩ := step_equations s hd x u
  have hn0 : audioStep s x u 0 ∈ next 0 := by
    rw [e0]
    exact contains_sound h0 (IntervalRat.mem_add
      (IntervalRat.mem_mul hc.1 (IntervalRat.mem_sub hu (IntervalRat.mem_mul hc.2.2.1 (hx 3))))
      (IntervalRat.mem_mul hc.2.1 (hx 0)))
  have hn1 : audioStep s x u 1 ∈ next 1 := by
    rw [e1]
    exact contains_sound h1 (IntervalRat.mem_add
      (IntervalRat.mem_mul hc.1 (saturation_mem s c hc _ _ depth (hs 0) _ hn0))
      (IntervalRat.mem_mul hc.2.1 (hx 1)))
  have hn2 : audioStep s x u 2 ∈ next 2 := by
    rw [e2]
    exact contains_sound h2 (IntervalRat.mem_add
      (IntervalRat.mem_mul hc.1 (saturation_mem s c hc _ _ depth (hs 1) _ hn1))
      (IntervalRat.mem_mul hc.2.1 (hx 2)))
  have hn3 : audioStep s x u 3 ∈ next 3 := by
    rw [e3]
    exact contains_sound h3 (IntervalRat.mem_add
      (IntervalRat.mem_mul hc.1 (saturation_mem s c hc _ _ depth (hs 2) _ hn2))
      (IntervalRat.mem_mul hc.2.1 (hx 3)))
  intro i
  fin_cases i <;> assumption

def matrixCheck (c : CoefficientBox) (old next : MatrixBox) (saturation : Box) : Bool :=
  decide (∀ j,
    contains (next 0 j) ((c.beta.mul (old 0 j)).sub ((c.alpha.mul c.feedback).mul (old 3 j))) = true ∧
    contains (next 1 j) ((c.beta.mul (old 1 j)).add
      ((c.alpha.mul (slope c.gain c.inverseNormalization (saturation 0))).mul (next 0 j))) = true ∧
    contains (next 2 j) ((c.beta.mul (old 2 j)).add
      ((c.alpha.mul (slope c.gain c.inverseNormalization (saturation 1))).mul (next 1 j))) = true ∧
    contains (next 3 j) ((c.beta.mul (old 3 j)).add
      ((c.alpha.mul (slope c.gain c.inverseNormalization (saturation 2))).mul (next 2 j))) = true)

noncomputable section

def row (a b : ℝ) (u v : Fin 4 → ℝ) : Fin 4 → ℝ := fun j => a*u j+b*v j

theorem row_dot (a b : ℝ) (u v w : Fin 4 → ℝ) :
    ∑ j, row a b u v j*w j = a*(∑ j, u j*w j)+b*(∑ j, v j*w j) := by
  simp only [row, add_mul, mul_assoc, Finset.sum_add_distrib, Finset.mul_sum]

def secantMatrix (s : NaiveReuppingLadder ℝ) (eta : Fin 3 → ℝ)
    (old : Matrix (Fin 4) (Fin 4) ℝ) : Matrix (Fin 4) (Fin 4) ℝ :=
  let m0 := row (1-s.alpha) (-s.alpha*s.baseFeedback) (old 0) (old 3)
  let m1 := row (1-s.alpha) (s.alpha*eta 0) (old 1) m0
  let m2 := row (1-s.alpha) (s.alpha*eta 1) (old 2) m1
  let m3 := row (1-s.alpha) (s.alpha*eta 2) (old 3) m2
  ![m0,m1,m2,m3]

theorem matrixCheck_sound (s : NaiveReuppingLadder ℝ) (c : CoefficientBox) (hc : c.Valid s)
    (old next : MatrixBox) (saturation : Box) (hcheck : matrixCheck c old next saturation = true)
    (eta : Fin 3 → ℝ) (he : ∀ i, eta i ∈ slope c.gain c.inverseNormalization (saturation i.castSucc))
    (matrix : Matrix (Fin 4) (Fin 4) ℝ) (hm : ∀ i j, matrix i j ∈ old i j) :
    ∀ i j, secantMatrix s eta matrix i j ∈ next i j := by
  simp only [matrixCheck, decide_eq_true_eq] at hcheck
  intro i j
  have hn0 : secantMatrix s eta matrix 0 j ∈ next 0 j := by
    have h := contains_sound (hcheck j).1 (IntervalRat.mem_sub
      (IntervalRat.mem_mul hc.2.1 (hm 0 j)) (IntervalRat.mem_mul
        (IntervalRat.mem_mul hc.1 hc.2.2.1) (hm 3 j)))
    convert h using 1
    simp [secantMatrix, row]
    ring
  have hn1 : secantMatrix s eta matrix 1 j ∈ next 1 j :=
    contains_sound (hcheck j).2.1 (IntervalRat.mem_add
      (IntervalRat.mem_mul hc.2.1 (hm 1 j))
      (IntervalRat.mem_mul (IntervalRat.mem_mul hc.1 (he 0)) hn0))
  have hn2 : secantMatrix s eta matrix 2 j ∈ next 2 j :=
    contains_sound (hcheck j).2.2.1 (IntervalRat.mem_add
      (IntervalRat.mem_mul hc.2.1 (hm 2 j))
      (IntervalRat.mem_mul (IntervalRat.mem_mul hc.1 (he 1)) hn1))
  have hn3 : secantMatrix s eta matrix 3 j ∈ next 3 j :=
    contains_sound (hcheck j).2.2.2 (IntervalRat.mem_add
      (IntervalRat.mem_mul hc.2.1 (hm 3 j))
      (IntervalRat.mem_mul (IntervalRat.mem_mul hc.1 (he 2)) hn2))
  fin_cases i <;> assumption

theorem secantMatrix_difference (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (x y delta : Fin 4 → ℝ) (u : Sample ℝ) (matrix : Matrix (Fin 4) (Fin 4) ℝ)
    (hm : x-y = matrix *ᵥ delta) (eta : Fin 3 → ℝ)
    (he : ∀ i : Fin 3, NormalizedTanh.apply s.saturation (audioStep s x u i.castSucc)-
      NormalizedTanh.apply s.saturation (audioStep s y u i.castSucc) =
      eta i*(audioStep s x u i.castSucc-audioStep s y u i.castSucc)) :
    audioStep s x u-audioStep s y u = secantMatrix s eta matrix *ᵥ delta := by
  obtain ⟨x0,x1,x2,x3⟩ := step_equations s hd x u
  obtain ⟨y0,y1,y2,y3⟩ := step_equations s hd y u
  have hm' (j : Fin 4) : x j-y j = ∑ k, matrix j k*delta k := congrFun hm j
  have hn0 : audioStep s x u 0-audioStep s y u 0 = ∑ j, secantMatrix s eta matrix 0 j*delta j := by
    change _ = ∑ j, row _ _ _ _ j*delta j
    rw [row_dot, x0, y0, ← hm' 0, ← hm' 3]
    ring
  have hn1 : audioStep s x u 1-audioStep s y u 1 = ∑ j, secantMatrix s eta matrix 1 j*delta j := by
    change _ = ∑ j, row (1-s.alpha) (s.alpha*eta 0) (matrix 1) (secantMatrix s eta matrix 0) j*delta j
    rw [row_dot, ← hn0, ← hm' 1]
    have h := he 0
    rw [show (0 : Fin 3).castSucc = (0 : Fin 4) by decide] at h
    rw [x1, y1]
    linear_combination s.alpha*h
  have hn2 : audioStep s x u 2-audioStep s y u 2 = ∑ j, secantMatrix s eta matrix 2 j*delta j := by
    change _ = ∑ j, row (1-s.alpha) (s.alpha*eta 1) (matrix 2) (secantMatrix s eta matrix 1) j*delta j
    rw [row_dot, ← hn1, ← hm' 2]
    have h := he 1
    rw [show (1 : Fin 3).castSucc = (1 : Fin 4) by decide] at h
    rw [x2, y2]
    linear_combination s.alpha*h
  have hn3 : audioStep s x u 3-audioStep s y u 3 = ∑ j, secantMatrix s eta matrix 3 j*delta j := by
    change _ = ∑ j, row (1-s.alpha) (s.alpha*eta 2) (matrix 3) (secantMatrix s eta matrix 2) j*delta j
    rw [row_dot, ← hn2, ← hm' 3]
    have h := he 2
    rw [show (2 : Fin 3).castSucc = (2 : Fin 4) by decide] at h
    rw [x3, y3]
    linear_combination s.alpha*h
  funext i
  fin_cases i <;> assumption

/-- For ANY two initial histories in the old box, a matrix in the next matrix
box represents their exact finite difference after the sample. -/
theorem pairCheck_sound (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : IntervalRat) (old next saturation : Box)
    (oldMatrix nextMatrix : MatrixBox) (depth : ℕ)
    (hv : valuesCheck c input old next saturation depth = true)
    (hm : matrixCheck c oldMatrix nextMatrix saturation = true)
    (x y delta : Fin 4 → ℝ) (hx : ∀ i, x i ∈ old i) (hy : ∀ i, y i ∈ old i)
    (u : Sample ℝ) (hu : u.x ∈ input)
    (matrix : Matrix (Fin 4) (Fin 4) ℝ) (hmat : ∀ i j, matrix i j ∈ oldMatrix i j)
    (hxy : x-y = matrix *ᵥ delta) :
    ∃ m : Matrix (Fin 4) (Fin 4) ℝ, (∀ i j, m i j ∈ nextMatrix i j) ∧
      audioStep s x u-audioStep s y u = m *ᵥ delta := by
  have hnx := valuesCheck_sound s hd c hc input old next saturation depth hv x hx u hu
  have hny := valuesCheck_sound s hd c hc input old next saturation depth hv y hy u hu
  have hs : ∀ i, IntervalTanh.check ((next i).mul c.gain) (saturation i) depth = true := by
    have hv' := hv
    simp only [valuesCheck, Bool.and_eq_true, decide_eq_true_eq] at hv'
    exact hv'.1.1.1.2
  have he (i : Fin 3) := DSP.IntervalArithmetic.exists_secant s.saturation c.gain c.inverseNormalization
    (next i.castSucc) (saturation i.castSucc) depth hc.2.2.2.1 hc.2.2.2.2 (hs i.castSucc)
    _ _ (hnx i.castSucc) (hny i.castSucc)
  choose eta heta heq using he
  exact ⟨secantMatrix s eta matrix, matrixCheck_sound s c hc _ _ _ hm eta heta matrix hmat,
    secantMatrix_difference s hd x y delta u matrix hxy eta heq⟩

end
end DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure

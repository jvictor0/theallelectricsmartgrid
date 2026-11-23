import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.EnclosureTrace
import DSP.Analysis.AttractingBlock

/-! Turn finite interval traces into a contraction of the existing audio block
in rational affine coordinates. No eigenvalue or linearization assumption is
used: the matrices represent exact differences between pairs of trajectories. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure
open LeanCert.Core DSP.IntervalArithmetic Metric
open scoped Matrix NNReal

abbrev Vector := Fin 4 → ℝ
abbrev RealMatrix := Matrix (Fin 4) (Fin 4) ℝ

def decode (center : Vector) (inverse : RealMatrix) (w : Vector) : Vector :=
  center+inverse *ᵥ w

def encode (center : Vector) (transform : RealMatrix) (x : Vector) : Vector :=
  transform *ᵥ (x-center)

theorem decode_continuous (center : Vector) (inverse : RealMatrix) : Continuous (decode center inverse) := by
  unfold decode Matrix.mulVec dotProduct
  fun_prop

theorem decode_sub (center : Vector) (inverse : RealMatrix) (x y : Vector) :
    decode center inverse x-decode center inverse y = inverse *ᵥ (x-y) := by
  simp [decode, Matrix.mulVec_sub]

theorem encode_sub (center : Vector) (transform : RealMatrix) (x y : Vector) :
    encode center transform x-encode center transform y = transform *ᵥ (x-y) := by
  simp only [encode, ← Matrix.mulVec_sub, sub_sub_sub_cancel_right]

theorem decode_encode (center : Vector) (transform inverse : RealMatrix)
    (hi : inverse*transform = 1) (x : Vector) : decode center inverse (encode center transform x) = x := by
  simp [decode, encode, Matrix.mulVec_mulVec, hi]

def coordinateBlock (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ)
    (center : Vector) (transform inverse : RealMatrix) (w : Vector) : Vector :=
  encode center transform (Step.states (audioStep s) u (decode center inverse w) N)

theorem coordinateBlock_commutes (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ)
    (center : Vector) (transform inverse : RealMatrix) (hi : inverse*transform = 1) (w : Vector) :
    decode center inverse (coordinateBlock s u N center transform inverse w) =
      Step.states (audioStep s) u (decode center inverse w) N :=
  decode_encode center transform inverse hi _

/-- A uniform secant-matrix enclosure proves contraction in the transformed
infinity norm. Coefficient, input and initial-box obligations stay explicit. -/
theorem coordinateBlock_lipschitz (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : ℕ → IntervalRat)
    (history saturation : ℕ → Box) (matrices : ℕ → MatrixBox) (depth N : ℕ)
    (hv : ∀ n < N, valuesCheck c (input n) (history n) (history (n+1)) (saturation n) depth = true)
    (hm : ∀ n < N, matrixCheck c (matrices n) (matrices (n+1)) (saturation n) = true)
    (u : ℕ → Sample ℝ) (hu : ∀ n < N, (u n).x ∈ input n)
    (center : Vector) (transform inverse : RealMatrix) (transformBox : MatrixBox)
    (ht : ∀ i j, transform i j ∈ transformBox i j) (hi : ∀ i j, inverse i j ∈ matrices 0 i j)
    (radius : ℝ) (hstart : ∀ w ∈ closedBall (0 : Vector) radius, ∀ i, decode center inverse w i ∈ history 0 i)
    (bound : ℚ) (hb : 0 ≤ bound)
    (hbound : matrixBound (matrixProduct transformBox (matrices N)) bound = true) :
    LipschitzOnWith ⟨bound, by exact_mod_cast hb⟩
      (coordinateBlock s u N center transform inverse) (closedBall 0 radius) := by
  apply LipschitzOnWith.of_dist_le_mul
  intro x hx y hy
  obtain ⟨matrix, hmat, hdiff⟩ := pairs_run s hd c hc input history saturation matrices depth N
    hv hm u hu (decode center inverse x) (decode center inverse y) (x-y)
    (hstart x hx) (hstart y hy) inverse hi (decode_sub center inverse x y)
  have htmat := mem_matrixProduct transform matrix transformBox (matrices N) ht hmat
  have hnorm := matrixBound_sound _ bound hbound (transform*matrix) htmat (x-y)
  rw [dist_eq_norm, dist_eq_norm]
  change ‖encode center transform _-encode center transform _‖ ≤ (bound:ℝ)*‖x-y‖
  rw [encode_sub, hdiff, Matrix.mulVec_mulVec]
  exact hnorm

/-- Interval membership implies a vector norm bound. -/
theorem vector_norm_bound (x : Vector) (box : Box) (bound : ℝ)
    (hb : 0 ≤ bound) (hx : ∀ i, x i ∈ box i) (hbox : ∀ i, (absoluteBound (box i):ℝ) ≤ bound) :
    ‖x‖ ≤ bound := by
  apply (pi_norm_le_iff_of_nonneg hb).mpr
  exact fun i => (norm_le_absoluteBound (hx i)).trans (hbox i)

/-- Initial coordinates in a norm ball are included using the proposed inverse
matrix box. The radius is rational so this last inclusion is decidable. -/
theorem decode_mem_box (center : Vector) (inverse : RealMatrix) (centerBox : Box) (inverseBox : MatrixBox)
    (hc : ∀ i, center i ∈ centerBox i) (hi : ∀ i j, inverse i j ∈ inverseBox i j)
    (radius : ℚ) (hr : 0 ≤ radius) (initial : Box)
    (hcheck : ∀ i, contains (initial i) ((centerBox i).add
      (matrixVector inverseBox (fun _ => ⟨-radius, radius, by linarith⟩) i)) = true)
    (w : Vector) (hw : w ∈ closedBall (0 : Vector) (radius:ℝ)) :
    ∀ i, decode center inverse w i ∈ initial i := by
  have hw' : ‖w‖ ≤ (radius:ℝ) := by simpa only [mem_closedBall, dist_zero_right] using hw
  have hwi (i : Fin 4) : w i ∈ (⟨-radius, radius, by linarith⟩ : IntervalRat) := by
    have h := (norm_le_pi_norm w i).trans hw'
    simpa only [IntervalRat.mem_def, Rat.cast_neg, Real.norm_eq_abs, abs_le] using h
  have hmul := mem_matrixVector inverse w inverseBox _ hi hwi
  exact fun i => contains_sound (hcheck i) (IntervalRat.mem_add (hc i) (hmul i))

/-- Replay the candidate center separately from the whole-ball trace to bound
the fixed-point residual without the uncertainty of the surrounding ball. -/
theorem coordinateBlock_residual (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (c : CoefficientBox) (hc : c.Valid s) (input : ℕ → IntervalRat)
    (history saturation : ℕ → Box) (depth N : ℕ)
    (hv : ∀ n < N, valuesCheck c (input n) (history n) (history (n+1)) (saturation n) depth = true)
    (u : ℕ → Sample ℝ) (hu : ∀ n < N, (u n).x ∈ input n)
    (center : Vector) (transform inverse : RealMatrix) (centerBox : Box) (transformBox : MatrixBox)
    (hcenter : ∀ i, center i ∈ centerBox i) (hstart : ∀ i, center i ∈ history 0 i)
    (ht : ∀ i j, transform i j ∈ transformBox i j)
    (bound : ℝ) (hb : 0 ≤ bound)
    (hbound : ∀ i, (absoluteBound
      (matrixVector transformBox (fun j => (history N j).sub (centerBox j)) i):ℝ) ≤ bound) :
    dist (coordinateBlock s u N center transform inverse 0) 0 ≤ bound := by
  have hf := values_run s hd c hc input history saturation depth N hv u hu center hstart N le_rfl
  have hsub (i : Fin 4) := IntervalRat.mem_sub (hf i) (hcenter i)
  have hm := mem_matrixVector transform (Step.states (audioStep s) u center N-center)
    transformBox _ ht hsub
  simpa only [coordinateBlock, decode, encode, Matrix.mulVec_zero, add_zero, dist_zero_right] using
    vector_norm_bound _ _ bound hb hm hbound

end DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure

import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.SecondEnclosure

/-! Replay derivative witnesses over an arbitrary ladder block. Value boxes
are supplied by the value/centered-cell replay; they are not inferred from
sampled values. The same derivative checks work for each Jacobian column and
each pair of columns in the Hessian. -/

namespace DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure
open LeanCert.Core DSP.IntervalArithmetic
open scoped Matrix

def firstCheck (c : CoefficientBox) (old next saturation : Box) : Bool :=
  matrixCheck c (fun i _ => old i) (fun i _ => next i) saturation

theorem matrixCheck_column (c : CoefficientBox) (old next : MatrixBox) (saturation : Box)
    (h : matrixCheck c old next saturation = true) (j : Fin 4) :
    firstCheck c (fun i => old i j) (fun i => next i j) saturation = true := by
  simp only [firstCheck,matrixCheck,decide_eq_true_eq] at h ⊢
  exact fun _ => h j

def VariationBox.history (box : VariationBox) : Box :=
  ![box.stage1,box.stage2,box.stage3,box.stage4]

theorem VariationBox.history_mem (box : VariationBox) (v : Variation) (h : box.Mem v) :
    ∀ i, v.history i ∈ box.history i := by
  intro i
  fin_cases i
  · exact h.1
  · exact h.2.1
  · exact h.2.2.1
  · exact h.2.2.2.1

def mixedCheck (c : CoefficientBox) (saturation left right old next : Box) : Bool :=
  decide (∀ i, contains (next i)
    ((mixedRanges c saturation left right old (IntervalRat.singleton 0)).history i) = true)

structure FrameBox where
  left : Box
  right : Box
  mixed : Box

def FrameBox.Mem (box : FrameBox) (frame : DerivativeFrame) : Prop :=
  (∀ i, frame.left i ∈ box.left i) ∧ (∀ i, frame.right i ∈ box.right i) ∧
    (∀ i, frame.mixed i ∈ box.mixed i)

def frameCheck (c : CoefficientBox) (old next : FrameBox) (state saturation : Box)
    (depth : ℕ) : Bool :=
  decide (∀ i, IntervalTanh.check ((state i).mul c.gain) (saturation i) depth = true) &&
  firstCheck c old.left next.left saturation && firstCheck c old.right next.right saturation &&
  mixedCheck c saturation next.left next.right old.mixed next.mixed

noncomputable section

theorem firstCheck_sound (s next : NaiveReuppingLadder ℝ)
    (c : CoefficientBox) (hc : c.Valid s) (state saturation old result : Box) (depth : ℕ)
    (hs : ∀ i, audioHistory next i ∈ state i)
    (ht : ∀ i, IntervalTanh.check ((state i).mul c.gain) (saturation i) depth = true)
    (hcheck : firstCheck c old result saturation = true)
    (v : Fin 4 → ℝ) (hv : ∀ i, v i ∈ old i) :
    ∀ i, (firstVariation s next v 0).history i ∈ result i := by
  let eta : Fin 3 → ℝ := fun i => NormalizedTanh.slope s.saturation (audioHistory next i.castSucc)
  have he (i : Fin 3) : eta i ∈ slope c.gain c.inverseNormalization (saturation i.castSucc) :=
    mem_slope s.saturation c.gain c.inverseNormalization (state i.castSucc)
      (saturation i.castSucc) depth hc.2.2.2.1 hc.2.2.2.2 (ht i.castSucc) _ (hs i.castSucc)
  have h := matrixCheck_sound s c hc (fun i _ => old i) (fun i _ => result i)
    saturation hcheck eta he (fun i _ => v i) (fun i _ => hv i)
  intro i
  have hid : (firstVariation s next v 0).history i = secantMatrix s eta (fun i _ => v i) i 0 := by
    fin_cases i <;> simp [Variation.history, firstVariation, secantMatrix, row, eta, audioHistory] <;> ring
  rw [hid]
  exact h i 0

/-- State membership is an explicit input from the value replay. The
derivative checker does not assume that its proposed state boxes are valid. -/
theorem frameCheck_sound (s : NaiveReuppingLadder ℝ) (c : CoefficientBox) (hc : c.Valid s)
    (old next : FrameBox) (state saturation : Box) (depth : ℕ)
    (hcheck : frameCheck c old next state saturation depth = true)
    (frame : DerivativeFrame) (hm : old.Mem frame) (u : Sample ℝ)
    (hs : ∀ i, audioStep s frame.value u i ∈ state i) :
    next.Mem (DerivativeFrame.advance s frame u) := by
  simp only [frameCheck, Bool.and_eq_true, decide_eq_true_eq] at hcheck
  rcases hcheck with ⟨⟨⟨ht,hl⟩,hr⟩,hsecond⟩
  let result := step (withHistory s frame.value) u
  let left := firstVariation s result frame.left 0
  let right := firstVariation s result frame.right 0
  have hl' : ∀ i, left.history i ∈ next.left i :=
    firstCheck_sound s result c hc state saturation old.left next.left depth hs ht hl frame.left hm.1
  have hr' : ∀ i, right.history i ∈ next.right i :=
    firstCheck_sound s result c hc state saturation old.right next.right depth hs ht hr frame.right hm.2.1
  refine ⟨hl',hr',?_⟩
  have hmix := mixedRanges_sound s result c hc state saturation next.left next.right old.mixed
    (IntervalRat.singleton 0) depth hs ht left right hl' hr' frame.mixed hm.2.2 0
    (by simpa using IntervalRat.mem_singleton 0)
  simp only [mixedCheck, decide_eq_true_eq] at hsecond
  exact fun i => contains_sound (hsecond i) (VariationBox.history_mem _ _ hmix i)

theorem frames_run (s : NaiveReuppingLadder ℝ) (c : CoefficientBox) (hc : c.Valid s)
    (boxes : ℕ → FrameBox) (state saturation : ℕ → Box) (depth N : ℕ)
    (hcheck : ∀ n < N, frameCheck c (boxes n) (boxes (n+1)) (state (n+1)) (saturation n) depth = true)
    (u : ℕ → Sample ℝ) (frame : DerivativeFrame) (hinit : (boxes 0).Mem frame)
    (hs : ∀ n ≤ N, ∀ i, Step.states (audioStep s) u frame.value n i ∈ state n i) :
    ∀ n ≤ N, (boxes n).Mem (Step.states (DerivativeFrame.advance s) u frame n) := by
  intro n hn
  induction n with
  | zero => exact hinit
  | succ n ih =>
    apply frameCheck_sound s c hc (boxes n) (boxes (n+1)) (state (n+1)) (saturation n) depth
      (hcheck n (by omega)) _ (ih (by omega)) (u n)
    intro i
    rw [DerivativeFrame.run_value]
    exact hs (n+1) hn i

/-- Reuse a checked secant-matrix trace to enclose each actual first
derivative column. This requires the separately verified whole-state trace;
the secant representation alone would not identify a derivative. -/
theorem first_run (s : NaiveReuppingLadder ℝ) (c : CoefficientBox) (hc : c.Valid s)
    (matrices : ℕ → MatrixBox) (state saturation : ℕ → Box) (depth N : ℕ)
    (hm : ∀ n < N, matrixCheck c (matrices n) (matrices (n+1)) (saturation n) = true)
    (ht : ∀ n < N, ∀ i, IntervalTanh.check ((state (n+1) i).mul c.gain) (saturation n i) depth = true)
    (u : ℕ → Sample ℝ) (frame : DerivativeFrame) (j : Fin 4)
    (hinit : ∀ i, frame.left i ∈ matrices 0 i j)
    (hs : ∀ n ≤ N, ∀ i, Step.states (audioStep s) u frame.value n i ∈ state n i) :
    ∀ n ≤ N, ∀ i, (Step.states (DerivativeFrame.advance s) u frame n).left i ∈ matrices n i j := by
  intro n hn
  induction n with
  | zero => exact hinit
  | succ n ih =>
    apply firstCheck_sound s _ c hc (state (n+1)) (saturation n)
      (fun i => matrices n i j) (fun i => matrices (n+1) i j) depth _
      (ht n (by omega)) (matrixCheck_column c _ _ _ (hm n (by omega)) j) _ (ih (by omega))
    intro i
    change audioStep s (Step.states (DerivativeFrame.advance s) u frame n).value (u n) i ∈ _
    rw [DerivativeFrame.run_value]
    exact hs (n+1) hn i

end
end DSP.NaiveReuppingLadder.HarmonicBalance.Enclosure

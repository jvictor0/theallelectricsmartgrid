import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.CoordinateVariation

/-! Identify the derivative frames with partial derivatives of one common
coordinate map. This justifies applying mean-value bounds to Jacobian
entries; the frames are not independent numerical surrogate functions. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.DerivativeFrame
open PolarTube

theorem run_first_congr (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ)
    (a b : DerivativeFrame) (hv : a.value = b.value) (hd : a.right = b.left) (n : ℕ) :
    (Step.states (advance s) u a n).value = (Step.states (advance s) u b n).value ∧
    (Step.states (advance s) u a n).right = (Step.states (advance s) u b n).left := by
  induction n with
  | zero => exact ⟨hv,hd⟩
  | succ n ih =>
    simp only [Step.states_succ,advance,ih.1,ih.2]
    trivial

theorem block_first_congr (chart : Chart) (dp ddp : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ)
    (a b : DerivativeFrame) (hv : a.value = b.value) (hd : a.right = b.left) :
    (block chart dp ddp s u N anchor a).value = (block chart dp ddp s u N anchor b).value ∧
    (block chart dp ddp s u N anchor a).right = (block chart dp ddp s u N anchor b).left := by
  have h := run_first_congr s u
    (linear chart.linear.symm (decode chart.profile dp ddp a))
    (linear chart.linear.symm (decode chart.profile dp ddp b))
    (by simp only [linear,decode,hv]) (by simp only [linear,decode,hv,hd]) N
  simp only [linear] at h
  simp only [block,encode,linear,h.1,h.2]
  trivial

def coordinateMap (chart : Chart) (dp ddp : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ) (x : State) : State :=
  (block chart dp ddp s u N anchor (affine x 0 0 0)).value

def coordinatePartial (chart : Chart) (dp ddp : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ)
    (x : State) (j : Fin 4) : State :=
  (block chart dp ddp s u N anchor (affine x (Pi.single j 1) 0 0)).left

theorem affine_value (chart : Chart) (dp ddp : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ)
    (x : State) (j k : Fin 4) (t : ℝ) :
    (block chart dp ddp s u N anchor (affine x (Pi.single j 1) (Pi.single k 1) t)).value =
      coordinateMap chart dp ddp s u N anchor (x+t • Pi.single j 1) := by
  simp only [block,encode,linear,run_value,decode,affine,coordinateMap,zero_smul,add_zero]

theorem affine_right (chart : Chart) (dp ddp : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ)
    (x : State) (j k : Fin 4) (t : ℝ) :
    (block chart dp ddp s u N anchor (affine x (Pi.single j 1) (Pi.single k 1) t)).right =
      coordinatePartial chart dp ddp s u N anchor (x+t • Pi.single j 1) k := by
  exact (block_first_congr chart dp ddp s u N anchor _ _
    (by simp [affine]) (by rfl)).2

theorem affine_left (chart : Chart) (dp ddp : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ)
    (x : State) (j k : Fin 4) :
    (block chart dp ddp s u N anchor (affine x (Pi.single j 1) (Pi.single k 1) 0)).left =
      coordinatePartial chart dp ddp s u N anchor x j := by
  have h := block_first_congr chart dp ddp s u N anchor
    (affine x (Pi.single j 1) (Pi.single j 1) 0) (affine x (Pi.single j 1) (Pi.single k 1) 0)
    (by rfl) (by rfl)
  rw [← h.2,affine_right]
  simp

theorem shifted_axis (x : State) (j : Fin 4) (t : ℝ) :
    x+(t-x j) • Pi.single j 1 = Function.update x j t := by
  funext k
  by_cases hk : k = j
  · subst k; simp
  · simp [hk]

/-- Convert a derivative at zero along a translated basis line to the usual
partial derivative, whose scalar argument is the actual coordinate. -/
theorem axis_derivative (f : State → ℝ) (x : State) (j : Fin 4) (d : ℝ)
    (h : HasDerivAt (fun t : ℝ => f (x+t • Pi.single j 1)) d 0) :
    HasDerivAt (fun t => f (Function.update x j t)) d (x j) := by
  simpa only [Function.comp_def,id_eq,shifted_axis,mul_one] using
    h.comp_of_eq (x j) ((hasDerivAt_id (x j)).sub_const (x j)) (by simp)

end DSP.NaiveReuppingLadder.HarmonicBalance.DerivativeFrame

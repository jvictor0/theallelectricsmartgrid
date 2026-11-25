import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Transfer
import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Coherent

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Filter Matrix ResponseMeasurement
open scoped Topology

noncomputable section

def Tap.complex {n : ℕ} (p : Tap n ℝ) : Tap n ℂ :=
  ⟨p.age, fun i => (p.fraction i : ℂ)⟩

def Spec.complex (p : Spec ℝ) : Spec ℂ :=
  ⟨fun j => (p.intro j).complex, p.main.complex, p.first.complex, p.second.complex,
    fun i => (p.feedback i : ℂ), fun i => (p.satGain i : ℂ),
    fun i => (p.highAlpha i : ℂ), fun i => (p.lowAlpha i : ℂ)⟩

def lanesRe (x : Lanes ℂ) : Lanes ℝ := fun i => (x i).re

def tapeRe {n : ℕ} (h : Tape n ℂ) : Tape n ℝ := fun j => lanesRe (h j)

def historyRe (h : History ℂ) : History ℝ :=
  (fun j => tapeRe (h.1 j), tapeRe h.2.1, tapeRe h.2.2.1,
    (lanesRe h.2.2.2.1.1, lanesRe h.2.2.2.1.2), tapeRe h.2.2.2.2.1, lanesRe h.wet)

private theorem lanesRe_smul (c : ℝ) (x : Lanes ℂ) : lanesRe (c • x) = c • lanesRe x := by
  funext i
  exact Complex.smul_re c (x i)

private theorem tapeRe_smul {n : ℕ} (c : ℝ) (x : Tape n ℂ) : tapeRe (c • x) = c • tapeRe x := by
  funext i
  exact lanesRe_smul c (x i)

def realHistory : History ℂ →ₗ[ℝ] History ℝ where
  toFun := historyRe
  map_add' _ _ := rfl
  map_smul' c h := by
    simp [historyRe, tapeRe_smul, lanesRe_smul, History.wet]
    rfl

theorem realHistory_wet (h : History ℂ) : (realHistory h).wet = lanesRe h.wet := rfl

/-- Each constituent denominator is nonzero at the requested frequency. -/
structure BlockDenominators (p : Spec ℂ) (q : ℂ) : Prop where
  intro : ∀ j i, 1 - allPassGain * tapResponse (p.intro j) q i ≠ 0
  first : ∀ i, 1 - allPassGain * tapResponse p.first q i ≠ 0
  second : ∀ i, 1 - allPassGain * tapResponse p.second q i ≠ 0
  high : ∀ i, 1 + (2 * p.highAlpha i - 1) * q ≠ 0
  low : ∀ i, 1 + (2 * p.lowAlpha i - 1) * q ≠ 0

def basisLane {K : Type} [Field K] (j : Fin 4) : Lanes K :=
  fun i => if i = j then 1 else 0

def columnInput (j : Fin 4) : History ℝ × ℝ →L[ℝ] History ℝ × Lanes ℝ :=
  ((ContinuousLinearMap.fst ℝ (History ℝ) ℝ).prod
    (((1 : ℝ →L[ℝ] ℝ).smulRight (basisLane j)).comp (ContinuousLinearMap.snd ℝ (History ℝ) ℝ)))

@[simp] theorem columnInput_apply (j : Fin 4) (s : History ℝ) (u : ℝ) :
    columnInput j (s, u) = (s, u • basisLane j) := rfl

def columnUpdate (p : Spec ℝ) (j : Fin 4) : History ℝ × ℝ → History ℝ :=
  fun v => update p (columnInput j v)

def columnTangent (p : Spec ℝ) (j : Fin 4) : History ℝ × ℝ →L[ℝ] History ℝ :=
  (tangent p).toContinuousLinearMap.comp (columnInput j)

def wetLane (i : Fin 4) : History ℝ →L[ℝ] ℝ :=
  (ContinuousLinearMap.proj i : Lanes ℝ →L[ℝ] ℝ).comp (History.wetMap.toContinuousLinearMap)

@[simp] theorem wetLane_apply (i : Fin 4) (s : History ℝ) : wetLane i s = s.wet i := rfl

theorem columnUpdate_zero (p : Spec ℝ) (j : Fin 4) : columnUpdate p j 0 = 0 := by
  simp [columnUpdate, update_zero]

theorem columnUpdate_continuous (p : Spec ℝ) (j : Fin 4) : Continuous (columnUpdate p j) :=
  (update_contDiff p).continuous.comp (columnInput j).continuous

theorem columnUpdate_hasStrictFDerivAt (p : Spec ℝ) (j : Fin 4) :
    HasStrictFDerivAt (columnUpdate p j) (columnTangent p j) 0 := by
  have h := update_hasStrictFDerivAt p
  rw [← (columnInput j).map_zero] at h
  exact h.comp 0 (columnInput j).hasStrictFDerivAt

end

end DSP.SquiggleBoyQuadReverb.Frozen

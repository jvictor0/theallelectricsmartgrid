import DSP.Filters.SquiggleBoyQuadReverb.Frozen.Model
import Mathlib.LinearAlgebra.Matrix.NonsingularInverse
import Mathlib.LinearAlgebra.Matrix.ToLin

namespace DSP.SquiggleBoyQuadReverb.Frozen

open Matrix
open scoped BigOperators

noncomputable section

def tapResponse {n : ℕ} (p : Tap n ℂ) (q : ℂ) : Lanes ℂ :=
  fun i => ∑ j, cubicWeights (p.fraction i) j * q ^ ((p.age i j).val + 1)

def allPassResponse {n : ℕ} (p : Tap n ℂ) (q : ℂ) : Lanes ℂ :=
  fun i => SchroederAllPass.response allPassGain (tapResponse p q i) 1

def introResponse (q : ℂ) : (order : List (Fin 8)) → IntroTaps order ℂ → Lanes ℂ
  | [], _ => 1
  | _ :: order, p => allPassResponse (p 0) q * introResponse q order (fun j => p j.succ)

def lowResponse (alpha q : ℂ) : ℂ := alpha * (1 + q) / (1 + (2*alpha - 1)*q)

def highResponse (alpha q : ℂ) : ℂ := (1 - alpha) * (1 - q) / (1 + (2*alpha - 1)*q)

def dampingResponse (p : Spec ℂ) (q : ℂ) : Lanes ℂ :=
  fun i => lowResponse (p.lowAlpha i) q * highResponse (p.highAlpha i) q

abbrev QuadMatrix := Matrix (Fin 4) (Fin 4) ℂ

/-- The delay write to wet-output path of the tangent. The main tap reads
after the current write, hence its extra factor of `q⁻¹`. -/
def forward (p : Spec ℂ) (q : ℂ) : QuadMatrix :=
  diagonal (fun i => allPassResponse p.second q i * dampingResponse p q i *
    p.satGain i * allPassResponse p.first q i) *
    LinearMap.toMatrix' mix * diagonal (q⁻¹ • tapResponse p.main q)

def returnDen (p : Spec ℂ) (q : ℂ) : QuadMatrix :=
  1 - q • (forward p q * diagonal p.feedback)

/-- Closed, coherent wet transfer with all eight intro stages in the input path. -/
def transfer (p : Spec ℂ) (q : ℂ) : QuadMatrix :=
  (returnDen p q)⁻¹ * (forward p q * diagonal (introResponse q _ p.intro))

theorem diagonal_mulLanes (v w : Lanes ℂ) : diagonal v *ᵥ w = v * w := by
  funext i
  exact mulVec_diagonal v w i

theorem transfer_solves (p : Spec ℂ) (q : ℂ) (h : IsUnit (returnDen p q).det) (x : Lanes ℂ) :
    let y := transfer p q *ᵥ x
    y = forward p q *ᵥ ((introResponse q _ p.intro) * x + q • (p.feedback * y)) := by
  dsimp only
  let y := transfer p q *ᵥ x
  have hy : returnDen p q *ᵥ y = forward p q *ᵥ (introResponse q _ p.intro * x) := by
    change returnDen p q *ᵥ (transfer p q *ᵥ x) = _
    rw [mulVec_mulVec, transfer, mul_nonsing_inv_cancel_left _ _ h, ← mulVec_mulVec, diagonal_mulLanes]
  rw [returnDen, sub_mulVec, one_mulVec, smul_mulVec, ← mulVec_mulVec] at hy
  rw [diagonal_mulLanes] at hy
  change y = forward p q *ᵥ (_ + q • (p.feedback * y))
  rw [mulVec_add, mulVec_smul, ← hy]
  abel

end

end DSP.SquiggleBoyQuadReverb.Frozen

import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockCell
import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.DerivativeEnclosure

/-! The same checked trace also bounds actual first derivatives throughout
the curved input cell. A finite-difference secant is not identified with a
Jacobian: the derivative recurrence is proved and checked separately, using
the already established whole-state boxes and saturation bounds. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockDerivatives
open LeanCert.Core PolarTube Enclosure DriftBlockCellData DriftBlockInitial
open scoped Matrix

def unit (j : Fin 4) : State := Pi.single j 1
def line (x : State) (j : Fin 4) (t : ℝ) : State := x+t • unit j
def frame (x : State) (j : Fin 4) (t : ℝ) : DerivativeFrame :=
  let z := line x j t
  ⟨DriftChart.inverse *ᵥ liftDecode DriftChart.profile z,
    fun i => (DriftChart.inverse*decodeJacobian DriftChart.profile
      (fun θ => DriftChart.profileDerivative θ) z) i j,0,0⟩

theorem frame_differentiates (x : State) (j : Fin 4) (t : ℝ) :
    DerivativeFrame.Differentiates (frame x j) t := by
  have hx (i : Fin 4) : HasDerivAt (fun r => line x j r i) (unit j i) t := by
    simpa only [line,Pi.add_apply,Pi.smul_apply,smul_eq_mul,id_eq,one_mul] using
      ((hasDerivAt_id t).mul_const (unit j i)).const_add (x i)
  have hd := hasDerivAt_matrix_decode DriftChart.profile (fun θ => DriftChart.profileDerivative θ)
    DriftChart.inverse (line x j) t (unit j) hx
    (hasDerivAt_pi.mp (DriftChart.profile_hasDerivAt (line x j t 0)))
  constructor
  · intro i
    simpa [frame,unit,Matrix.mulVec,dotProduct,Pi.single_apply] using hd i
  · exact fun i => hasDerivAt_const t 0

theorem initial_mem (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j i : Fin 4) :
    (frame (coordinates θ r) j 0).left i ∈ matrices 0 i j := by
  have h := jacobian_mem θ r hθ hr 1 (by norm_num) i j
  simpa [frame,line,path,displacement] using h

theorem derivative_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j : Fin 4) :
    ∀ n ≤ 162, ∀ i,
      (Step.states (DerivativeFrame.advance s) DriftBlockParameters.forcing
        (frame (coordinates θ r) j 0) n).left i ∈ matrices n i j := by
  have hc (n : ℕ) (hn : n < 162) := DriftBlockChecks.steps n hn
  simp only [centeredCheck,Bool.and_eq_true,decide_eq_true_eq] at hc
  apply first_run s LockedData.coefficients (LockedParameters.valid s h) matrices whole
    DriftBlockCellData.saturation 24 162 (fun n hn => (hc n hn).1.2) (fun n hn => (hc n hn).2)
    DriftBlockParameters.forcing (frame (coordinates θ r) j 0) j (initial_mem θ r hθ hr j)
  intro n hn i
  simpa only [frame,line,zero_smul,add_zero,point] using (DriftBlockCell.replay s h θ r hθ hr n hn).2.1 i

/-- Each physical state component has its true directional derivative in
the stored interval. The four directions are phase and the three transverse
coordinates; the derivative is taken through the exact curved decode. -/
theorem hasDerivAt_mem (s : NaiveReuppingLadder ℝ) (h : LockedParameters.Parameters s)
    (θ : ℝ) (r : Normal) (hθ : θ ∈ DriftProfileCellData.angle)
    (hr : ∀ j, |r j| ≤ (1/500000:ℝ)) (j : Fin 4) (n : ℕ) (hn : n ≤ 162) (i : Fin 4) :
    ∃ d ∈ matrices n i j,
      HasDerivAt (fun t => Step.states (audioStep s) DriftBlockParameters.forcing
        (DriftChart.inverse *ᵥ liftDecode DriftChart.profile (line (coordinates θ r) j t)) n i) d 0 := by
  exact ⟨_,derivative_mem s h θ r hθ hr j n hn i,
    DerivativeFrame.run_hasDerivAt s h.damping DriftBlockParameters.forcing
      (frame (coordinates θ r) j) 0 (frame_differentiates _ _ _) n i⟩

end DSP.NaiveReuppingLadder.HarmonicBalance.Certificates.DriftBlockDerivatives

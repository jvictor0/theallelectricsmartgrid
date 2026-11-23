import DSP.Filters.NaiveReuppingLadder.HarmonicBalance.BlockVariation
import DSP.Analysis.PolarTube.Decode
import DSP.Analysis.PolarTube.Encode
import Mathlib.Analysis.Calculus.Deriv.Prod

/-! First and mixed second derivatives of the complete coordinate block map:
curved input chart, exact linear inverse, original ladder block, exact linear
transform, and curved output chart. Branch safety stays an explicit local
domain obligation for the real output lift. -/

noncomputable section
namespace DSP.NaiveReuppingLadder.HarmonicBalance.DerivativeFrame
open PolarTube

def affine (center left right : State) (t : ℝ) : DerivativeFrame :=
  ⟨center+t • left,left,right,0⟩

theorem affine_differentiates (center left right : State) (t : ℝ) :
    Differentiates (affine center left right) t := by
  constructor
  · intro j
    simpa only [affine, Pi.add_apply, Pi.smul_apply, smul_eq_mul, id_eq, one_mul] using
      ((hasDerivAt_id t).mul_const (left j)).const_add (center j)
  · exact fun j => hasDerivAt_const t (right j)

def linear (A : State ≃ₗ[ℝ] State) (frame : DerivativeFrame) : DerivativeFrame :=
  ⟨A frame.value,A frame.left,A frame.right,A frame.mixed⟩

theorem linear_differentiates (A : State ≃ₗ[ℝ] State) (frame : ℝ → DerivativeFrame)
    (t : ℝ) (h : Differentiates frame t) : Differentiates (fun r => linear A (frame r)) t := by
  let a : State →L[ℝ] State := ⟨A.toLinearMap,A.toLinearMap.continuous_of_finiteDimensional⟩
  constructor
  · exact hasDerivAt_pi.mp (a.hasFDerivAt.comp_hasDerivAt t (hasDerivAt_pi.mpr h.1))
  · exact hasDerivAt_pi.mp (a.hasFDerivAt.comp_hasDerivAt t (hasDerivAt_pi.mpr h.2))

def decode (profile : Real.Angle → Normal) (dprofile ddprofile : ℝ → Normal)
    (frame : DerivativeFrame) : DerivativeFrame :=
  ⟨liftDecode profile frame.value,
    decodeVariation profile dprofile frame.value frame.left,
    decodeVariation profile dprofile frame.value frame.right,
    decodeMixedVariation profile dprofile ddprofile frame.value frame.left frame.right frame.mixed⟩

theorem decode_differentiates (profile : Real.Angle → Normal) (dprofile ddprofile : ℝ → Normal)
    (frame : ℝ → DerivativeFrame) (t : ℝ) (h : Differentiates frame t)
    (hp : ∀ θ j, HasDerivAt (fun r : ℝ => profile (r:Real.Angle) j) (dprofile θ j) θ)
    (hdp : ∀ θ j, HasDerivAt (fun r => dprofile r j) (ddprofile θ j) θ) :
    Differentiates (fun r => decode profile dprofile ddprofile (frame r)) t := by
  exact ⟨hasDerivAt_liftDecode profile dprofile (fun r => (frame r).value) t (frame t).left
    h.1 (hp _),
    hasDerivAt_decodeVariation profile dprofile ddprofile (fun r => (frame r).value)
      (fun r => (frame r).right) t (frame t).left (frame t).mixed h.1 h.2 (hp _) (hdp _)⟩

def encode (profile : Real.Angle → Normal) (dprofile ddprofile : ℝ → Normal)
    (anchor : ℂ) (frame : DerivativeFrame) : DerivativeFrame :=
  ⟨liftEncode profile anchor frame.value,
    encodeVariation dprofile anchor frame.value frame.left,
    encodeVariation dprofile anchor frame.value frame.right,
    encodeMixedVariation dprofile ddprofile anchor frame.value frame.left frame.right frame.mixed⟩

theorem encode_differentiates (profile : Real.Angle → Normal) (dprofile ddprofile : ℝ → Normal)
    (anchor : ℂ) (ha : anchor ≠ 0) (frame : ℝ → DerivativeFrame) (t : ℝ)
    (h : Differentiates frame t) (hcut : head (frame t).value/anchor ∈ Complex.slitPlane)
    (hp : ∀ θ j, HasDerivAt (fun r : ℝ => profile (r:Real.Angle) j) (dprofile θ j) θ)
    (hdp : ∀ θ j, HasDerivAt (fun r => dprofile r j) (ddprofile θ j) θ) :
    Differentiates (fun r => encode profile dprofile ddprofile anchor (frame r)) t := by
  exact ⟨hasDerivAt_liftEncode profile dprofile anchor ha (fun r => (frame r).value) t
    (frame t).left h.1 hcut (hp _),
    hasDerivAt_encodeVariation dprofile ddprofile anchor ha (fun r => (frame r).value)
      (fun r => (frame r).right) t (frame t).left (frame t).mixed h.1 h.2 hcut (hdp _)⟩

def block (chart : Chart) (dprofile ddprofile : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ)
    (frame : DerivativeFrame) : DerivativeFrame :=
  encode chart.profile dprofile ddprofile anchor (linear chart.linear
    (Step.states (advance s) u
      (linear chart.linear.symm (decode chart.profile dprofile ddprofile frame)) N))

theorem block_differentiates (chart : Chart) (dprofile ddprofile : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (hd : s.dampingScale = 0)
    (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ) (ha : anchor ≠ 0)
    (frame : ℝ → DerivativeFrame) (t : ℝ) (h : Differentiates frame t)
    (hp : ∀ θ j, HasDerivAt (fun r : ℝ => chart.profile (r:Real.Angle) j) (dprofile θ j) θ)
    (hdp : ∀ θ j, HasDerivAt (fun r => dprofile r j) (ddprofile θ j) θ)
    (hcut : head (chart.linear (Step.states (audioStep s) u
      (chart.linear.symm (liftDecode chart.profile (frame t).value)) N))/anchor ∈ Complex.slitPlane) :
    Differentiates (fun r => block chart dprofile ddprofile s u N anchor (frame r)) t := by
  have hdecode := linear_differentiates chart.linear.symm _ t
    (decode_differentiates chart.profile dprofile ddprofile frame t h hp hdp)
  have hrun := run_differentiates s hd u _ t hdecode N
  apply encode_differentiates chart.profile dprofile ddprofile anchor ha _ t
    (linear_differentiates chart.linear _ t hrun) _ hp hdp
  simpa only [linear, decode, run_value] using hcut

/-- The value of the differentiated real lift is the actual coordinate
block step, with only the local choice of phase representative forgotten. -/
theorem block_value (chart : Chart) (dprofile ddprofile : ℝ → Normal)
    (s : NaiveReuppingLadder ℝ) (u : ℕ → Sample ℝ) (N : ℕ) (anchor : ℂ) (ha : anchor ≠ 0)
    (frame : DerivativeFrame)
    (hne : head (chart.linear (Step.states (audioStep s) u
      (chart.linear.symm (liftDecode chart.profile frame.value)) N)) ≠ 0) :
    let out := (block chart dprofile ddprofile s u N anchor frame).value
    ((out 0:Real.Angle), ![out 1,out 2,out 3]) =
      chart.coordinateStep (fun x => Step.states (audioStep s) u x N)
        ((frame.value 0:Real.Angle), ![frame.value 1,frame.value 2,frame.value 3]) := by
  dsimp only [block, encode, linear, decode]
  rw [run_value]
  exact liftEncode_matches chart.profile anchor ha _ hne

end DSP.NaiveReuppingLadder.HarmonicBalance.DerivativeFrame

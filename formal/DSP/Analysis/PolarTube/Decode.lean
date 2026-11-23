import DSP.Analysis.PolarTube
import Mathlib.Analysis.SpecialFunctions.Trigonometric.Deriv

/-! First and mixed second variations of the curved initial chart. These
include both radial/phase cross terms and the curvature of its Fourier
profile; treating the chart as an affine state transform would omit them. -/

noncomputable section
namespace DSP.PolarTube

def liftDecode (profile : Real.Angle → Normal) (x : State) : State :=
  rawDecode profile ((x 0:Real.Angle), ![x 1,x 2,x 3])

def decodeVariation (profile : Real.Angle → Normal) (dprofile : ℝ → Normal)
    (x v : State) : State :=
  let θ := x 0
  let ρ := profile (θ:Real.Angle) 0+x 1
  let dρ := dprofile θ 0*v 0+v 1
  ![dρ*Real.cos θ-ρ*Real.sin θ*v 0,
    dρ*Real.sin θ+ρ*Real.cos θ*v 0,
    dprofile θ 1*v 0+v 2, dprofile θ 2*v 0+v 3]

theorem hasDerivAt_liftDecode (profile : Real.Angle → Normal) (dprofile : ℝ → Normal)
    (x : ℝ → State) (t : ℝ) (v : State)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (v j) t)
    (hp : ∀ j, HasDerivAt (fun θ : ℝ => profile (θ:Real.Angle) j) (dprofile (x t 0) j) (x t 0)) :
    ∀ j, HasDerivAt (fun r => liftDecode profile (x r) j) (decodeVariation profile dprofile (x t) v j) t := by
  have hr := ((hp 0).comp t (hx 0)).add (hx 1)
  have hc := (hx 0).cos
  have hs := (hx 0).sin
  intro j
  fin_cases j
  · exact (hr.mul hc).congr_deriv (by dsimp [decodeVariation]; ring)
  · exact (hr.mul hs).congr_deriv (by dsimp [decodeVariation]; ring)
  · exact ((hp 1).comp t (hx 0)).add (hx 2)
  · exact ((hp 2).comp t (hx 0)).add (hx 3)

def decodeMixedVariation (profile : Real.Angle → Normal) (dprofile ddprofile : ℝ → Normal)
    (x left right mixed : State) : State :=
  let θ := x 0
  let ρ := profile (θ:Real.Angle) 0+x 1
  let lρ := dprofile θ 0*left 0+left 1
  let rρ := dprofile θ 0*right 0+right 1
  let mρ := ddprofile θ 0*left 0*right 0+dprofile θ 0*mixed 0+mixed 1
  ![mρ*Real.cos θ-rρ*Real.sin θ*left 0-lρ*Real.sin θ*right 0-
      ρ*Real.cos θ*left 0*right 0-ρ*Real.sin θ*mixed 0,
    mρ*Real.sin θ+rρ*Real.cos θ*left 0+lρ*Real.cos θ*right 0-
      ρ*Real.sin θ*left 0*right 0+ρ*Real.cos θ*mixed 0,
    ddprofile θ 1*left 0*right 0+dprofile θ 1*mixed 0+mixed 2,
    ddprofile θ 2*left 0*right 0+dprofile θ 2*mixed 0+mixed 3]

theorem hasDerivAt_decodeVariation (profile : Real.Angle → Normal) (dprofile ddprofile : ℝ → Normal)
    (x v : ℝ → State) (t : ℝ) (left mixed : State)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (left j) t)
    (hv : ∀ j, HasDerivAt (fun r => v r j) (mixed j) t)
    (hp : ∀ j, HasDerivAt (fun θ : ℝ => profile (θ:Real.Angle) j) (dprofile (x t 0) j) (x t 0))
    (hdp : ∀ j, HasDerivAt (fun θ : ℝ => dprofile θ j) (ddprofile (x t 0) j) (x t 0)) :
    ∀ j, HasDerivAt (fun r => decodeVariation profile dprofile (x r) (v r) j)
      (decodeMixedVariation profile dprofile ddprofile (x t) left (v t) mixed j) t := by
  have hr := ((hp 0).comp t (hx 0)).add (hx 1)
  have hrv := (((hdp 0).comp t (hx 0)).mul (hv 0)).add (hv 1)
  have hc := (hx 0).cos
  have hs := (hx 0).sin
  intro j
  fin_cases j
  · exact ((hrv.mul hc).sub ((hr.mul hs).mul (hv 0))).congr_deriv
      (by dsimp [decodeMixedVariation]; ring)
  · exact ((hrv.mul hs).add ((hr.mul hc).mul (hv 0))).congr_deriv
      (by dsimp [decodeMixedVariation]; ring)
  · exact ((((hdp 1).comp t (hx 0)).mul (hv 0)).add (hv 2)).congr_deriv
      (by dsimp [decodeMixedVariation])
  · exact ((((hdp 2).comp t (hx 0)).mul (hv 0)).add (hv 3)).congr_deriv
      (by dsimp [decodeMixedVariation])

end DSP.PolarTube

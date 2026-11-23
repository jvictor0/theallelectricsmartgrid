import DSP.Analysis.PolarTube.Radius

/-! Derivatives of a local real lift of the entire output chart. The local
lift agrees with the global circle chart; its first and second variations
retain the profile curvature as well as the polar curvature. -/

noncomputable section
namespace DSP.PolarTube

def liftEncode (profile : Real.Angle → Normal) (anchor : ℂ) (x : State) : State :=
  let φ := localPhase anchor (head x)
  ![φ, ‖head x‖-profile (φ:Real.Angle) 0,
    x 2-profile (φ:Real.Angle) 1, x 3-profile (φ:Real.Angle) 2]

theorem liftEncode_matches (profile : Real.Angle → Normal) (anchor : ℂ) (ha : anchor ≠ 0)
    (x : State) (hx : head x ≠ 0) :
    ((liftEncode profile anchor x 0:Real.Angle),
      ![liftEncode profile anchor x 1,liftEncode profile anchor x 2,liftEncode profile anchor x 3]) =
        rawEncode profile x := by
  simp only [liftEncode, Matrix.cons_val_zero, Matrix.cons_val_one, Matrix.cons_val_two,
    Matrix.cons_val_three, localPhase_angle anchor (head x) ha hx, rawEncode]
  rfl

theorem hasDerivAt_head (x : ℝ → State) (t : ℝ) (v : State)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (v j) t) :
    HasDerivAt (fun r => head (x r)) (head v) t := by
  simpa only [head, Complex.mk_eq_add_mul_I] using!
    (hx 0).ofReal_comp.add ((hx 1).ofReal_comp.mul_const Complex.I)

def encodeVariation (dprofile : ℝ → Normal) (anchor : ℂ) (x v : State) : State :=
  let φ := localPhase anchor (head x)
  let dφ := (head v/head x).im
  ![dφ, radiusVariation (head x) (head v)-dprofile φ 0*dφ,
    v 2-dprofile φ 1*dφ, v 3-dprofile φ 2*dφ]

theorem hasDerivAt_liftEncode (profile : Real.Angle → Normal) (dprofile : ℝ → Normal)
    (anchor : ℂ) (ha : anchor ≠ 0) (x : ℝ → State) (t : ℝ) (v : State)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (v j) t)
    (hcut : head (x t)/anchor ∈ Complex.slitPlane)
    (hp : ∀ j, HasDerivAt (fun φ : ℝ => profile (φ:Real.Angle) j)
      (dprofile (localPhase anchor (head (x t))) j) (localPhase anchor (head (x t)))) :
    ∀ j, HasDerivAt (fun r => liftEncode profile anchor (x r) j)
      (encodeVariation dprofile anchor (x t) v j) t := by
  have hz := hasDerivAt_head x t v hx
  have hne : head (x t) ≠ 0 := by
    intro he
    simpa [he] using Complex.slitPlane_ne_zero hcut
  have hφ := hasDerivAt_localPhase anchor ha (fun r => head (x r)) t (head v) hz hcut
  have hr := hasDerivAt_radius (fun r => head (x r)) t (head v) hz hne
  intro j
  fin_cases j
  · exact hφ
  · convert! hr.sub ((hp 0).comp t hφ) using 1
  · convert! (hx 2).sub ((hp 1).comp t hφ) using 1
  · convert! (hx 3).sub ((hp 2).comp t hφ) using 1

def encodeMixedVariation (dprofile ddprofile : ℝ → Normal) (anchor : ℂ)
    (x left right mixed : State) : State :=
  let z := head x
  let φ := localPhase anchor z
  let lφ := (head left/z).im
  let rφ := (head right/z).im
  let mφ := (head mixed/z-head right*head left/z^2).im
  let mρ := radiusVariation z (head mixed)+radiusHessian z (head left) (head right)
  ![mφ, mρ-(ddprofile φ 0*lφ*rφ+dprofile φ 0*mφ),
    mixed 2-(ddprofile φ 1*lφ*rφ+dprofile φ 1*mφ),
    mixed 3-(ddprofile φ 2*lφ*rφ+dprofile φ 2*mφ)]

theorem hasDerivAt_encodeVariation (dprofile ddprofile : ℝ → Normal)
    (anchor : ℂ) (ha : anchor ≠ 0) (x v : ℝ → State) (t : ℝ) (left mixed : State)
    (hx : ∀ j, HasDerivAt (fun r => x r j) (left j) t)
    (hv : ∀ j, HasDerivAt (fun r => v r j) (mixed j) t)
    (hcut : head (x t)/anchor ∈ Complex.slitPlane)
    (hp : ∀ j, HasDerivAt (fun φ : ℝ => dprofile φ j)
      (ddprofile (localPhase anchor (head (x t))) j) (localPhase anchor (head (x t)))) :
    ∀ j, HasDerivAt (fun r => encodeVariation dprofile anchor (x r) (v r) j)
      (encodeMixedVariation dprofile ddprofile anchor (x t) left (v t) mixed j) t := by
  have hz := hasDerivAt_head x t left hx
  have hw := hasDerivAt_head v t mixed hv
  have hne : head (x t) ≠ 0 := by
    intro he
    simpa [he] using Complex.slitPlane_ne_zero hcut
  have hφ := hasDerivAt_localPhase anchor ha (fun r => head (x r)) t (head left) hz hcut
  have hmφ := hasDerivAt_phaseVariation (fun r => head (x r)) (fun r => head (v r)) t
    (head left) (head mixed) hz hw hne
  have hmρ := hasDerivAt_radiusVariation (fun r => head (x r)) (fun r => head (v r)) t
    (head left) (head mixed) hz hw hne
  intro j
  fin_cases j
  · exact hmφ
  · convert! hmρ.sub (((hp 0).comp t hφ).mul hmφ) using 1
  · convert! (hv 2).sub (((hp 1).comp t hφ).mul hmφ) using 1
  · convert! (hv 3).sub (((hp 2).comp t hφ).mul hmφ) using 1

end DSP.PolarTube

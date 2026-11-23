import DSP.Analysis.PolarTube.Graph
import Mathlib.Topology.Instances.AddCircle.Real

/-! Attraction to an invariant circle in the ORIGINAL state space. The
comparison point follows the orbit's current phase: the theorem does not
assert convergence to a fixed phase or an irrational rotation number. A
verified graph certificate and agreement with the state map are inputs. -/

noncomputable section
namespace DSP.PolarTube.Chart
open Metric Filter
open scoped Topology NNReal

def inverseMap (chart : Chart) : State →L[ℝ] State :=
  ⟨chart.linear.symm.toLinearMap,chart.linear.symm.toLinearMap.continuous_of_finiteDimensional⟩

theorem rawDecode_fiber_dist (profile : Real.Angle → Normal) (θ : Real.Angle) (r s : Normal) :
    dist (rawDecode profile (θ,r)) (rawDecode profile (θ,s)) ≤ dist r s := by
  have hc : |θ.cos| ≤ 1 := by
    induction θ using Real.Angle.induction_on with
    | _ θ => exact Real.abs_cos_le_one θ
  have hs : |θ.sin| ≤ 1 := by
    induction θ using Real.Angle.induction_on with
    | _ θ => exact Real.abs_sin_le_one θ
  rw [dist_eq_norm,dist_eq_norm]
  apply (pi_norm_le_iff_of_nonneg (norm_nonneg (r-s))).mpr
  have hmul (v : ℝ) (hv : |v| ≤ 1) : |(r 0-s 0)*v| ≤ ‖r-s‖ := by
    rw [abs_mul]
    have hr : |r 0-s 0| ≤ ‖r-s‖ := norm_le_pi_norm (r-s) 0
    nlinarith [abs_nonneg (r 0-s 0),norm_nonneg (r-s)]
  intro i
  fin_cases i
  · change |(profile θ 0+r 0)*θ.cos-(profile θ 0+s 0)*θ.cos| ≤ _
    convert hmul θ.cos hc using 1
    congr 1
    ring
  · change |(profile θ 0+r 0)*θ.sin-(profile θ 0+s 0)*θ.sin| ≤ _
    convert hmul θ.sin hs using 1
    congr 1
    ring
  · simpa [rawDecode] using norm_le_pi_norm (r-s) 1
  · simpa [rawDecode] using norm_le_pi_norm (r-s) 2

theorem decode_fiber_dist (chart : Chart) (θ : Real.Angle) (r s : Normal) :
    dist (chart.decode (θ,r)) (chart.decode (θ,s)) ≤ ‖chart.inverseMap‖*dist r s := by
  calc
    _ = ‖chart.inverseMap (rawDecode chart.profile (θ,r)-rawDecode chart.profile (θ,s))‖ := by
      rw [map_sub]; rfl
    _ ≤ ‖chart.inverseMap‖*‖rawDecode chart.profile (θ,r)-rawDecode chart.profile (θ,s)‖ :=
      chart.inverseMap.le_opNorm _
    _ ≤ _ := mul_le_mul_of_nonneg_left (rawDecode_fiber_dist chart.profile θ r s) (norm_nonneg _)

variable {radius : ℝ} {L : ℝ≥0}

def curve (chart : Chart) (h : AttractingGraph.Graph (B := Real.Angle) (0:Normal) radius L)
    (θ : Real.Angle) : State := chart.decode (θ,h.val θ)

theorem curve_continuous (chart : Chart) (hp : Continuous chart.profile)
    (h : AttractingGraph.Graph (B := Real.Angle) (0:Normal) radius L) : Continuous (chart.curve h) :=
  (chart.decode_continuous hp).comp (continuous_id.prodMk h.val.continuous)

theorem curve_injective (chart : Chart) (hp : ∀ θ, radius < chart.profile θ 0)
    (h : AttractingGraph.Graph (B := Real.Angle) (0:Normal) radius L) : Function.Injective (chart.curve h) := by
  have hr (θ : Real.Angle) : 0 < radial chart.profile (θ,h.val θ) :=
    radial_pos_of_tube chart.profile radius hp _ (by simpa only [mem_closedBall,dist_zero_right] using h.property.2 θ)
  intro θ φ he
  exact congrArg Prod.fst (chart.decode_injective (hr θ) (hr φ) he)

theorem curve_invariant (chart : Chart) (f : State → State)
    (cert : AttractingGraph.Certificate (B := Real.Angle) (0:Normal) radius L)
    (heq : ∀ x : Coordinates, x.2 ∈ closedBall (0:Normal) radius → cert.step x = chart.coordinateStep f x)
    (h : AttractingGraph.Graph (B := Real.Angle) (0:Normal) radius L)
    (hinv : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ))) (θ : Real.Angle) :
    f (chart.curve h θ) = chart.curve h (cert.phase θ (h.val θ)) := by
  have hm := congrArg chart.decode (heq (θ,h.val θ) (h.property.2 θ))
  rw [chart.coordinateStep_commutes] at hm
  simpa only [AttractingGraph.Certificate.step,hinv,curve] using hm.symm

theorem curve_range_invariant (chart : Chart) (f : State → State)
    (cert : AttractingGraph.Certificate (B := Real.Angle) (0:Normal) radius L)
    (heq : ∀ x : Coordinates, x.2 ∈ closedBall (0:Normal) radius → cert.step x = chart.coordinateStep f x)
    (h : AttractingGraph.Graph (B := Real.Angle) (0:Normal) radius L)
    (hinv : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ))) :
    f '' Set.range (chart.curve h) = Set.range (chart.curve h) := by
  ext y
  constructor
  · rintro ⟨_,⟨θ,rfl⟩,rfl⟩
    exact ⟨cert.phase θ (h.val θ),(chart.curve_invariant f cert heq h hinv θ).symm⟩
  · rintro ⟨φ,rfl⟩
    let θ := (cert.phase_equiv h).symm φ
    refine ⟨chart.curve h θ,⟨θ,rfl⟩,?_⟩
    rw [chart.curve_invariant f cert heq h hinv, ← cert.phase_equiv_eq]
    exact congrArg (chart.curve h) ((cert.phase_equiv h).apply_symm_apply φ)

theorem curve_dist_bound (chart : Chart) (f : State → State)
    (cert : AttractingGraph.Certificate (B := Real.Angle) (0:Normal) radius L)
    (heq : ∀ x : Coordinates, x.2 ∈ closedBall (0:Normal) radius → cert.step x = chart.coordinateStep f x)
    (h : AttractingGraph.Graph (B := Real.Angle) (0:Normal) radius L)
    (hinv : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ)))
    (x : Coordinates) (hx : x.2 ∈ closedBall (0:Normal) radius) (n : ℕ) :
    dist (f^[n] (chart.decode x)) (chart.curve h (cert.step^[n] x).1) ≤
      ‖chart.inverseMap‖*((cert.rate:ℝ)^n*dist x.2 (h.val x.1)) := by
  rw [← chart.certificate_iterate_commutes f cert heq x hx n]
  exact (chart.decode_fiber_dist _ _ _).trans
    (mul_le_mul_of_nonneg_left (cert.iterate_error h hinv x hx n) (norm_nonneg _))

theorem exists_attracting_curve (chart : Chart) (f : State → State)
    (cert : AttractingGraph.Certificate (B := Real.Angle) (0:Normal) radius L)
    (heq : ∀ x : Coordinates, x.2 ∈ closedBall (0:Normal) radius → cert.step x = chart.coordinateStep f x)
    (hp : Continuous chart.profile) (hr : ∀ θ, radius < chart.profile θ 0) :
    ∃ h : AttractingGraph.Graph (B := Real.Angle) (0:Normal) radius L,
      Continuous (chart.curve h) ∧ Function.Injective (chart.curve h) ∧
      f '' Set.range (chart.curve h) = Set.range (chart.curve h) ∧
      ∀ x : Coordinates, x.2 ∈ closedBall (0:Normal) radius →
        Tendsto (fun n => dist (f^[n] (chart.decode x)) (chart.curve h (cert.step^[n] x).1)) atTop (𝓝 0) := by
  letI : Fact (0 < 2*Real.pi) := ⟨by positivity⟩
  letI : CompactSpace Real.Angle := inferInstanceAs (CompactSpace (AddCircle (2*Real.pi)))
  obtain ⟨h,hinv⟩ := cert.exists_invariant
  refine ⟨h,chart.curve_continuous hp h,chart.curve_injective hr h,
    chart.curve_range_invariant f cert heq h hinv,?_⟩
  intro x hx
  have ht := ((tendsto_pow_atTop_nhds_zero_of_lt_one cert.rate.coe_nonneg
    (show (cert.rate:ℝ) < 1 from cert.small)).mul_const (dist x.2 (h.val x.1))).const_mul ‖chart.inverseMap‖
  simp only [zero_mul,mul_zero] at ht
  exact squeeze_zero (fun _ => dist_nonneg) (chart.curve_dist_bound f cert heq h hinv x hx) ht

end DSP.PolarTube.Chart

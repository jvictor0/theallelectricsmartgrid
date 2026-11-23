import Mathlib.Topology.MetricSpace.Contracting
import Mathlib.Topology.ContinuousMap.Compact
import Mathlib.Topology.Instances.ENNReal.Lemmas

/-! A graph-transform criterion for an attracting invariant curve. Unlike a
skew product, the phase update is allowed to depend on the transverse state.
Uniform tube, phase-invertibility and cone bounds are certificate inputs.
An invariant graph and attraction to it are conclusions. This alone does not
prove an irrational rotation number or identify the invariant time measure. -/

noncomputable section
namespace DSP.AttractingGraph
open Filter Metric
open scoped Topology NNReal

variable {B F : Type*} [MetricSpace B] [CompactSpace B] [Nonempty B]
  [MetricSpace F] [CompleteSpace F]

def graphs (center : F) (radius : ℝ) (L : ℝ≥0) : Set C(B, F) :=
  {h | LipschitzWith L h ∧ ∀ θ, h θ ∈ closedBall center radius}

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
theorem graphs_closed (center : F) (radius : ℝ) (L : ℝ≥0) :
    IsClosed (graphs (B := B) center radius L) := by
  simp only [graphs, Set.setOf_and, Set.setOf_forall]
  apply (isClosed_setOf_lipschitzWith L |>.preimage continuous_coeFun).inter
  exact isClosed_iInter (fun θ : B => isClosed_closedBall.preimage
    (show Continuous (fun h : C(B,F) => h θ) from continuous_eval_const θ))

abbrev Graph (center : F) (radius : ℝ) (L : ℝ≥0) := graphs (B := B) center radius L

instance (center : F) (radius : ℝ) (L : ℝ≥0) :
    CompleteSpace (Graph (B := B) center radius L) :=
  (graphs_closed center radius L).completeSpace_coe

/-- Bounds in phase/transverse coordinates. A is the lower separation of the
phase map along ANY admissible graph; b measures transverse-to-phase coupling;
c and d bound the two inputs of the transverse update. -/
structure Certificate (center : F) (radius : ℝ) (L : ℝ≥0) where
  phase : B → F → B
  transverse : B → F → F
  radius_nonneg : 0 ≤ radius
  A : ℝ≥0
  b : ℝ≥0
  c : ℝ≥0
  d : ℝ≥0
  A_pos : 0 < A
  phase_fiber : ∀ θ r s, r ∈ closedBall center radius → s ∈ closedBall center radius →
    dist (phase θ r) (phase θ s) ≤ b * dist r s
  transverse_lip : ∀ θ φ r s, r ∈ closedBall center radius → s ∈ closedBall center radius →
    dist (transverse θ r) (transverse φ s) ≤ c * dist θ φ + d * dist r s
  transverse_range : ∀ θ r, r ∈ closedBall center radius → transverse θ r ∈ closedBall center radius
  phase_equiv : Graph (B := B) center radius L → B ≃ B
  phase_equiv_eq : ∀ h θ, phase_equiv h θ = phase θ (h.val θ)
  phase_separation : ∀ h : Graph (B := B) center radius L, ∀ θ φ,
    A * dist θ φ ≤ dist (phase_equiv h θ) (phase_equiv h φ)
  cone : (c+d*L)/A ≤ L
  small : d+(c+d*L)*b/A < 1

namespace Certificate
variable {center : F} {radius : ℝ} {L : ℝ≥0}
variable (cert : Certificate (B := B) center radius L)

def rate : ℝ≥0 := cert.d+(cert.c+cert.d*L)*cert.b/cert.A

def transformValue (h : Graph (B := B) center radius L) (φ : B) : F :=
  let θ := (cert.phase_equiv h).symm φ
  cert.transverse θ (h.val θ)

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
theorem inverse_lipschitz (h : Graph (B := B) center radius L) (φ ψ : B) :
    dist ((cert.phase_equiv h).symm φ) ((cert.phase_equiv h).symm ψ) ≤ dist φ ψ/cert.A := by
  apply (le_div_iff₀ (show (0:ℝ)<cert.A from cert.A_pos)).mpr
  simpa only [Equiv.apply_symm_apply, mul_comm] using
    cert.phase_separation h ((cert.phase_equiv h).symm φ) ((cert.phase_equiv h).symm ψ)

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
theorem transform_lipschitz (h : Graph (B := B) center radius L) :
    LipschitzWith L (cert.transformValue h) := by
  apply LipschitzWith.of_dist_le_mul
  intro φ ψ
  let θ := (cert.phase_equiv h).symm φ
  let η := (cert.phase_equiv h).symm ψ
  have hn := cert.transverse_lip θ η (h.val θ) (h.val η) (h.property.2 θ) (h.property.2 η)
  have hh := h.property.1.dist_le_mul θ η
  have hi := cert.inverse_lipschitz h φ ψ
  have hcone : ((cert.c:ℝ)+cert.d*L)/cert.A ≤ L := by exact_mod_cast cert.cone
  change dist (cert.transverse θ (h.val θ)) (cert.transverse η (h.val η)) ≤ _
  calc
    _ ≤ ((cert.c:ℝ)+cert.d*L) * dist θ η := by nlinarith [cert.d.coe_nonneg]
    _ ≤ ((cert.c:ℝ)+cert.d*L) * (dist φ ψ/cert.A) := by gcongr
    _ = (((cert.c:ℝ)+cert.d*L)/cert.A) * dist φ ψ := by ring
    _ ≤ _ := mul_le_mul_of_nonneg_right hcone dist_nonneg

def transform (h : Graph (B := B) center radius L) : Graph (B := B) center radius L :=
  ⟨⟨cert.transformValue h, (cert.transform_lipschitz h).continuous⟩,
    cert.transform_lipschitz h, fun _ =>
      cert.transverse_range _ _ (h.property.2 _)⟩

omit [Nonempty B] [CompleteSpace F] in
/-- Reparameterizing the two graphs to the SAME final phase costs b/A times
their vertical separation. Dropping this term would incorrectly assume that
the phase update ignores the transverse displacement. -/
theorem inverse_graph_error (h k : Graph (B := B) center radius L) (φ : B) :
    dist ((cert.phase_equiv h).symm φ) ((cert.phase_equiv k).symm φ) ≤
      cert.b * dist h k / cert.A := by
  let θ := (cert.phase_equiv h).symm φ
  let η := (cert.phase_equiv k).symm φ
  have hs := cert.phase_separation h θ η
  have hf := cert.phase_fiber η (k.val η) (h.val η) (k.property.2 η) (h.property.2 η)
  have hh : dist (k.val η) (h.val η) ≤ dist h k := by
    simpa only [dist_comm, Subtype.dist_eq] using ContinuousMap.dist_apply_le_dist (f := k.val) (g := h.val) η
  have he : cert.phase η (k.val η) = φ := by
    rw [← cert.phase_equiv_eq]; exact (cert.phase_equiv k).apply_symm_apply φ
  have he' : cert.phase_equiv h θ = φ := (cert.phase_equiv h).apply_symm_apply φ
  rw [he', cert.phase_equiv_eq] at hs
  rw [he] at hf
  apply (le_div_iff₀ (show (0:ℝ)<cert.A from cert.A_pos)).mpr
  change dist θ η * _ ≤ _
  nlinarith [mul_le_mul_of_nonneg_left hh cert.b.coe_nonneg]

omit [Nonempty B] [CompleteSpace F] in
theorem transform_contracting : ContractingWith cert.rate cert.transform := by
  refine ⟨cert.small, LipschitzWith.of_dist_le_mul ?_⟩
  intro h k
  change dist (cert.transform h).val (cert.transform k).val ≤ _
  apply (ContinuousMap.dist_le (by positivity)).mpr
  intro φ
  let θ := (cert.phase_equiv h).symm φ
  let η := (cert.phase_equiv k).symm φ
  have hi := cert.inverse_graph_error h k φ
  have ht := dist_triangle (h.val θ) (h.val η) (k.val η)
  have hh := h.property.1.dist_le_mul θ η
  have hv : dist (h.val η) (k.val η) ≤ dist h k :=
    ContinuousMap.dist_apply_le_dist η
  have hn := cert.transverse_lip θ η (h.val θ) (k.val η) (h.property.2 θ) (k.property.2 η)
  change dist (cert.transverse θ (h.val θ)) (cert.transverse η (k.val η)) ≤ _
  have hr : (cert.rate:ℝ) = cert.d+(cert.c+cert.d*L)*cert.b/cert.A := by simp [rate]
  rw [hr]
  calc
    _ ≤ ((cert.c:ℝ)+cert.d*L)*dist θ η + cert.d*dist h k := by
      nlinarith [mul_le_mul_of_nonneg_left ht cert.d.coe_nonneg,
        mul_le_mul_of_nonneg_left hh cert.d.coe_nonneg,
        mul_le_mul_of_nonneg_left hv cert.d.coe_nonneg]
    _ ≤ ((cert.c:ℝ)+cert.d*L)*(cert.b*dist h k/cert.A)+cert.d*dist h k := by gcongr
    _ = _ := by ring

omit [Nonempty B] in
/-- Existence and uniqueness among the graphs satisfying the stated tube and
slope bounds. The phase along the graph may continue to drift. -/
theorem exists_invariant :
    ∃ h : Graph (B := B) center radius L, ∀ θ,
      cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ)) := by
  let zeroGraph : Graph (B := B) center radius L :=
    ⟨ContinuousMap.const B center, (LipschitzWith.const center).weaken (by positivity),
      fun _ => mem_closedBall_self cert.radius_nonneg⟩
  letI : Nonempty (Graph (B := B) center radius L) := ⟨zeroGraph⟩
  let h := cert.transform_contracting.fixedPoint cert.transform
  have hf : cert.transform h = h := cert.transform_contracting.fixedPoint_isFixedPt
  refine ⟨h, fun θ => ?_⟩
  have hv := congrArg (fun g : Graph (B := B) center radius L => g.val (cert.phase_equiv h θ)) hf
  change cert.transformValue h (cert.phase_equiv h θ) = h.val (cert.phase_equiv h θ) at hv
  dsimp only [transformValue] at hv
  rw [Equiv.symm_apply_apply] at hv
  simpa only [cert.phase_equiv_eq] using hv

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
theorem fixed_of_invariant (h : Graph (B := B) center radius L)
    (hinv : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ))) :
    cert.transform h = h := by
  apply Subtype.ext
  apply ContinuousMap.ext
  intro φ
  change cert.transverse ((cert.phase_equiv h).symm φ) (h.val ((cert.phase_equiv h).symm φ)) = h.val φ
  rw [hinv, ← cert.phase_equiv_eq, Equiv.apply_symm_apply]

omit [Nonempty B] [CompleteSpace F] in
theorem invariant_unique (h k : Graph (B := B) center radius L)
    (hh : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ)))
    (hk : ∀ θ, cert.transverse θ (k.val θ) = k.val (cert.phase θ (k.val θ))) : h = k :=
  cert.transform_contracting.eq_or_edist_eq_top_of_fixedPoints
    (cert.fixed_of_invariant h hh) (cert.fixed_of_invariant k hk) |>.resolve_right (edist_ne_top _ _)

omit [Nonempty B] [CompleteSpace F] in
/-- A posteriori distance from an approximate graph to the invariant graph.
The residual is a UNIFORM graph residual after reparameterizing phase. -/
theorem invariant_error (candidate h : Graph (B := B) center radius L)
    (hh : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ))) :
    dist candidate h ≤ dist candidate (cert.transform candidate)/(1-cert.rate) := by
  have hk := cert.transform_contracting.2.dist_le_mul candidate h
  rw [cert.fixed_of_invariant h hh] at hk
  have ht := dist_triangle candidate (cert.transform candidate) h
  have hrate : (cert.rate:ℝ)<1 := cert.small
  apply (le_div_iff₀ (by linarith : (0:ℝ)<1-cert.rate)).mpr
  nlinarith

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
/-- Attraction is to the graph at the trajectory's CURRENT phase, rather than
to an arbitrarily selected phase point on the graph. -/
theorem transverse_error (h : Graph (B := B) center radius L)
    (hinv : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ)))
    (θ : B) (r : F) (hr : r ∈ closedBall center radius) :
    dist (cert.transverse θ r) (h.val (cert.phase θ r)) ≤ cert.rate * dist r (h.val θ) := by
  let φ := cert.phase θ r
  let η := (cert.phase_equiv h).symm φ
  have he : cert.phase η (h.val η) = φ := by
    rw [← cert.phase_equiv_eq]; exact (cert.phase_equiv h).apply_symm_apply φ
  have hs := cert.phase_separation h θ η
  rw [cert.phase_equiv_eq h θ, cert.phase_equiv_eq h η, he] at hs
  have hf := cert.phase_fiber θ (h.val θ) r (h.property.2 θ) hr
  have hi : dist θ η ≤ cert.b*dist r (h.val θ)/cert.A := by
    apply (le_div_iff₀ (show (0:ℝ)<cert.A from cert.A_pos)).mpr
    rw [dist_comm (h.val θ) r] at hf
    nlinarith
  have hn := cert.transverse_lip θ η r (h.val η) hr (h.property.2 η)
  have hh := h.property.1.dist_le_mul θ η
  have ht := dist_triangle r (h.val θ) (h.val η)
  have hv : h.val (cert.phase θ r) = cert.transverse η (h.val η) := by
    rw [hinv, he]
  rw [hv]
  have hr' : (cert.rate:ℝ) = cert.d+(cert.c+cert.d*L)*cert.b/cert.A := by simp [rate]
  rw [hr']
  calc
    _ ≤ ((cert.c:ℝ)+cert.d*L)*dist θ η+cert.d*dist r (h.val θ) := by
      nlinarith [mul_le_mul_of_nonneg_left ht cert.d.coe_nonneg,
        mul_le_mul_of_nonneg_left hh cert.d.coe_nonneg]
    _ ≤ ((cert.c:ℝ)+cert.d*L)*(cert.b*dist r (h.val θ)/cert.A)+cert.d*dist r (h.val θ) := by gcongr
    _ = _ := by ring

def step (x : B × F) : B × F := (cert.phase x.1 x.2, cert.transverse x.1 x.2)

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
theorem iterate_error (h : Graph (B := B) center radius L)
    (hinv : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ)))
    (x : B × F) (hx : x.2 ∈ closedBall center radius) (n : ℕ) :
    dist (cert.step^[n] x).2 (h.val (cert.step^[n] x).1) ≤
      (cert.rate:ℝ)^n * dist x.2 (h.val x.1) := by
  have ht (m : ℕ) : (cert.step^[m] x).2 ∈ closedBall center radius := by
    induction m with
    | zero => exact hx
    | succ m ih =>
      rw [Function.iterate_succ_apply']
      exact cert.transverse_range _ _ ih
  induction n with
  | zero => simp
  | succ n ih =>
    rw [Function.iterate_succ_apply']
    calc
      _ ≤ cert.rate * dist (cert.step^[n] x).2 (h.val (cert.step^[n] x).1) :=
        cert.transverse_error h hinv _ _ (ht n)
      _ ≤ cert.rate * ((cert.rate:ℝ)^n * dist x.2 (h.val x.1)) := by gcongr
      _ = _ := by rw [pow_succ]; ring

omit [CompactSpace B] [Nonempty B] [CompleteSpace F] in
theorem attracts (h : Graph (B := B) center radius L)
    (hinv : ∀ θ, cert.transverse θ (h.val θ) = h.val (cert.phase θ (h.val θ)))
    (x : B × F) (hx : x.2 ∈ closedBall center radius) :
    Tendsto (fun n => dist (cert.step^[n] x).2 (h.val (cert.step^[n] x).1)) atTop (𝓝 0) := by
  have hk : (cert.rate:ℝ) < 1 := cert.small
  have ht := (tendsto_pow_atTop_nhds_zero_of_lt_one cert.rate.coe_nonneg hk).mul_const
    (dist x.2 (h.val x.1))
  simp only [zero_mul] at ht
  exact squeeze_zero (fun _ => dist_nonneg) (cert.iterate_error h hinv x hx) ht

end Certificate
end DSP.AttractingGraph

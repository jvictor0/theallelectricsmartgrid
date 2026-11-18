import DSP.Filters.LinkwitzRileySplitter.PreparedProofs
import DSP.Filters.TPTSVF.Settling
import DSP.ResponseMeasurement.Circle

/-! Each real LR output approaches its proved complex band response.
The settling contract admits vanishing input transients, so cascades compose. -/
noncomputable section
namespace DSP.LinkwitzRileySplitter
open ResponseMeasurement

theorem SectionReady.settles_lowPass {f : TPTSVF ℝ} {s : TPTSVF.Snapshot ℝ}
    (h : SectionReady f s) (hg : 0 < s.g) (hk : 0 < s.k) (z : ℂ) (hz : ‖z‖=1) :
    Settles TPTSVF.step f (·.lowPass) z ((Complex.ofReal <$> s).lowPassTransfer (z-1) (z+1)) := by
  have h' := TPTSVF.settles_readout f s.g h.tuned hg (h.damping ▸ hk) z hz 0
  simpa [TPTSVF.modeResponse,h.damping,Functor.map] using h'

theorem SectionReady.settles_allPass {f : TPTSVF ℝ} {s : TPTSVF.Snapshot ℝ}
    (h : SectionReady f s) (hg : 0 < s.g) (hk : 0 < s.k) (z : ℂ) (hz : ‖z‖=1) :
    Settles TPTSVF.step f (·.allPass) z ((Complex.ofReal <$> s).allPassTransfer (z-1) (z+1)) := by
  have h' := TPTSVF.settles_readout f s.g h.tuned hg (h.damping ▸ hk) z hz 1
  simpa [TPTSVF.modeResponse,h.damping,Functor.map] using h'

theorem phase_settles (t : Topology) (f : PhaseState ℝ t) (s : TreeSnapshot ℝ t)
    (h : PhaseReady t f s) (hv : validSnapshot t s) (z : ℂ) (hz : ‖z‖=1) :
    Settles (phaseStep t) f (phaseOutput t) z
      (phaseTransfer t (mapData (Functor.map Complex.ofReal) t s) (z-1) (z+1)) := by
  induction t with
  | leaf => intro u A hu; change Tracks z u (1*A); simpa using hu
  | branch low high hl hh =>
    have root := Settles.project (phaseStep (.branch low high)) TPTSVF.step
      (·.svf) (fun _ _ => rfl) (h.1.settles_allPass hv.1 hv.2.1 z hz)
    have lo := Settles.cascade (phaseStep (.branch low high)) (phaseStep low)
      (·.low) (·.svf.allPass) (fun _ _ => rfl) root (hl f.low s.low h.2.1 hv.2.2.2.1)
    have hi := Settles.cascade (phaseStep (.branch low high)) (phaseStep high)
      (·.high) (fun v => phaseOutput low v.low) (fun _ _ => rfl) lo (hh f.high s.high h.2.2 hv.2.2.2.2)
    simpa only [phaseOutput,phaseTransfer,mapData,mul_assoc] using hi

theorem tree_settles_band (t : Topology) (f : TreeState ℝ t) (s : TreeSnapshot ℝ t)
    (h : TreeReady t f s) (hv : validSnapshot t s) (z : ℂ) (hz : ‖z‖=1) (i : Nat) :
    Settles (treeStep t) f (fun v => treeBand t v i) z
      (bandTransfer t (mapData (Functor.map Complex.ofReal) t s) (z-1) (z+1) i) := by
  induction t generalizing i with
  | leaf => intro u A hu; change Tracks z u (1*A); simpa using hu
  | branch low high hl hh =>
    have first := Settles.project (treeStep (.branch low high)) TPTSVF.step
      (·.first) (fun _ _ => rfl) (h.1.settles_lowPass hv.1 hv.2.1 z hz)
    have ap := Settles.project (treeStep (.branch low high)) TPTSVF.step
      (·.first) (fun _ _ => rfl) (h.1.settles_allPass hv.1 hv.2.1 z hz)
    have second := Settles.cascade (treeStep (.branch low high)) TPTSVF.step
      (·.second) (·.first.lowPass) (fun _ _ => rfl) first (h.2.1.settles_lowPass hv.1 hv.2.1 z hz)
    have lowRoot := second.transfer_eq (by rw [← pow_two])
    have highRoot := ap.sub lowRoot
    by_cases hi : i < low.bands
    · have phase := Settles.cascade (treeStep (.branch low high)) (phaseStep high)
        (·.lowPhase) (·.second.lowPass) (fun _ _ => rfl) lowRoot
        (phase_settles high f.lowPhase s.high h.2.2.1 hv.2.2.2.2 z hz)
      have child := Settles.cascade (treeStep (.branch low high)) (treeStep low)
        (·.low) (fun v => phaseOutput high v.lowPhase) (fun _ _ => rfl) phase
        (hl f.low s.low h.2.2.2.2.1 hv.2.2.2.1 i)
      simpa only [treeBand,bandTransfer,mapData,if_pos hi,lowSplitTransfer] using child
    · have phase := Settles.cascade (treeStep (.branch low high)) (phaseStep low)
        (·.highPhase) (fun v => v.first.allPass-v.second.lowPass) (fun _ _ => rfl) highRoot
        (phase_settles low f.highPhase s.low h.2.2.2.1 hv.2.2.2.1 z hz)
      have child := Settles.cascade (treeStep (.branch low high)) (treeStep high)
        (·.high) (fun v => phaseOutput low v.highPhase) (fun _ _ => rfl) phase
        (hh f.high s.high h.2.2.2.2.2 hv.2.2.2.2 (i-low.bands))
      simpa only [treeBand,bandTransfer,mapData,if_neg hi,highSplitTransfer,lowSplitTransfer] using child

theorem settles_band {n : Nat} (f : State n ℝ) (s : Snapshot n ℝ)
    (h : TreeReady _ f.tree s.tree) (hv : validSnapshot _ s.tree) (z : ℂ) (hz : ‖z‖=1) (i : Fin n) :
    Settles step f (fun v => v.bands i) z
      (bandTransfer _ (mapData (Functor.map Complex.ofReal) _ s.tree) (z-1) (z+1) i.val) := by
  have ht := Settles.project step (treeStep _) (·.tree) (fun _ _ => rfl)
    (tree_settles_band _ f.tree s.tree h hv z hz i.val)
  exact ht.congr (fun _ _ => rfl)

theorem Snapshot.bandResponse_fourier {n : Nat} (s : Snapshot n ℝ) (frequency : ℝ) (i : Fin n) :
    s.bandResponse frequency i = bandTransfer _ (mapData (Functor.map Complex.ofReal) _ s.tree)
      (fourier 1 (frequency : Phase)-1) (fourier 1 (frequency : Phase)+1) i.val := by
  let z := fourier 1 (frequency : Phase)
  have hz : z ≠ 0 := by have h : ‖z‖=1 := Circle.norm_coe _; intro he; simp [he] at h
  rw [Snapshot.bandResponse_delay,unitDelay_eq_fourier_inv]
  have h := bandTransfer_scale _ (mapData (Functor.map Complex.ofReal) _ s.tree)
    (1-z⁻¹) (1+z⁻¹) z hz i.val
  simpa only [mul_sub,mul_add,mul_one,mul_inv_cancel₀ hz] using h.symm

theorem computeCoefficients_settles_band {n : Nat} (spec : LogicalSpec n ℝ) (f : State n ℝ)
    (hs : ValidSpec _ spec.tree) (hr : TreeAtRest _ f.tree) (frequency : ℝ) (i : Fin n) :
    Settles step (computeCoefficients spec f) (fun v => v.bands i)
      (fourier 1 (frequency : Phase)) ((makeSnapshot spec).bandResponse frequency i) := by
  rw [Snapshot.bandResponse_fourier]
  exact settles_band _ _ (tuneTree_ready _ spec.tree f.tree hs hr)
    (snapshotTree_valid _ spec.tree hs) _ (Circle.norm_coe _) i

end DSP.LinkwitzRileySplitter

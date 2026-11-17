import DSP.Filters.LinkwitzRileySplitter.Transfer

/-! The generated recurrence realizes the composed transfer, by projecting and
cascading the shared TPTSVF realization. No separate SVF equations are assumed. -/
noncomputable section
namespace DSP.LinkwitzRileySplitter
open Bilinear
variable {K : Type} [Field K]

structure SectionReady (f : TPTSVF K) (s : TPTSVF.Snapshot K) : Prop where
  tuned : f.Tuned s.g
  damping : f.k = s.k
  rest : f.AtRest

theorem SectionReady.lowPass {f : TPTSVF K} {s : TPTSVF.Snapshot K} (h : SectionReady f s) :
    Realizes TPTSVF.step f (·.lowPass) ((const <$> s).lowPassTransfer sigma 1) := by
  convert TPTSVF.realizes_lowPass_snapshot f s.g h.tuned h.rest using 1
  rw [h.damping]

theorem SectionReady.allPass {f : TPTSVF K} {s : TPTSVF.Snapshot K} (h : SectionReady f s) :
    Realizes TPTSVF.step f (·.allPass) ((const <$> s).allPassTransfer sigma 1) := by
  convert TPTSVF.realizes_allPass_snapshot f s.g h.tuned h.rest using 1
  rw [h.damping]

def PhaseReady : (t : Topology) → PhaseState K t → TreeSnapshot K t → Prop
  | .leaf, _, _ => True
  | .branch low high, f, s => SectionReady f.svf s.value ∧
      PhaseReady low f.low s.low ∧ PhaseReady high f.high s.high

def TreeReady : (t : Topology) → TreeState K t → TreeSnapshot K t → Prop
  | .leaf, _, _ => True
  | .branch low high, f, s => SectionReady f.first s.value ∧ SectionReady f.second s.value ∧
      PhaseReady high f.lowPhase s.high ∧ PhaseReady low f.highPhase s.low ∧
      TreeReady low f.low s.low ∧ TreeReady high f.high s.high

theorem phase_realizes (t : Topology) (f : PhaseState K t) (s : TreeSnapshot K t)
    (h : PhaseReady t f s) :
    Realizes (phaseStep t) f (phaseOutput t)
      (phaseTransfer t (mapData (Functor.map const) t s) sigma 1) := by
  induction t with
  | leaf =>
    intro u
    change ser (fun n => u n) = 1 * ser u
    exact (one_mul _).symm
  | branch low high hl hh =>
    have root := Realizes.project (phaseStep (.branch low high)) TPTSVF.step
      (·.svf) (fun _ _ => rfl) h.1.allPass
    have lo := Realizes.cascade (phaseStep (.branch low high)) (phaseStep low)
      (·.low) (·.svf.allPass) (fun _ _ => rfl) root (hl f.low s.low h.2.1)
    have hi := Realizes.cascade (phaseStep (.branch low high)) (phaseStep high)
      (·.high) (fun v => phaseOutput low v.low) (fun _ _ => rfl) lo (hh f.high s.high h.2.2)
    simpa only [phaseOutput, phaseTransfer, mapData, mul_assoc] using hi

theorem tree_realizes_band (t : Topology) (f : TreeState K t) (s : TreeSnapshot K t)
    (h : TreeReady t f s) (i : Nat) :
    Realizes (treeStep t) f (fun v => treeBand t v i)
      (bandTransfer t (mapData (Functor.map const) t s) sigma 1 i) := by
  induction t generalizing i with
  | leaf =>
    intro u
    change ser (fun n => u n) = 1 * ser u
    exact (one_mul _).symm
  | branch low high hl hh =>
    have first := Realizes.project (treeStep (.branch low high)) TPTSVF.step
      (·.first) (fun _ _ => rfl) h.1.lowPass
    have ap := Realizes.project (treeStep (.branch low high)) TPTSVF.step
      (·.first) (fun _ _ => rfl) h.1.allPass
    have second := Realizes.cascade (treeStep (.branch low high)) TPTSVF.step
      (·.second) (·.first.lowPass) (fun _ _ => rfl) first h.2.1.lowPass
    have lowRoot := second.transfer_eq (by rw [← pow_two])
    have highRoot := ap.sub lowRoot
    by_cases hi : i < low.bands
    · have phase := Realizes.cascade (treeStep (.branch low high)) (phaseStep high)
        (·.lowPhase) (·.second.lowPass) (fun _ _ => rfl) lowRoot
        (phase_realizes high f.lowPhase s.high h.2.2.1)
      have child := Realizes.cascade (treeStep (.branch low high)) (treeStep low)
        (·.low) (fun v => phaseOutput high v.lowPhase) (fun _ _ => rfl) phase
        (hl f.low s.low h.2.2.2.2.1 i)
      simpa only [treeBand, bandTransfer, mapData, if_pos hi, lowSplitTransfer] using child
    · have phase := Realizes.cascade (treeStep (.branch low high)) (phaseStep low)
        (·.highPhase) (fun v => v.first.allPass - v.second.lowPass) (fun _ _ => rfl) highRoot
        (phase_realizes low f.highPhase s.low h.2.2.2.1)
      have child := Realizes.cascade (treeStep (.branch low high)) (treeStep high)
        (·.high) (fun v => phaseOutput low v.highPhase) (fun _ _ => rfl) phase
        (hh f.high s.high h.2.2.2.2.2 (i-low.bands))
      simpa only [treeBand, bandTransfer, mapData, if_neg hi, highSplitTransfer, lowSplitTransfer] using child

/-- This is the output array of the executable splitter, not an auxiliary
mathematical output that differs from what code generation exports. -/
theorem realizes_band {n : Nat} (f : State n K) (s : Snapshot n K)
    (h : TreeReady _ f.tree s.tree) (i : Fin n) :
    Realizes step f (fun v => v.bands i)
      (bandTransfer _ (mapData (Functor.map const) _ s.tree) sigma 1 i.val) := by
  have ht := Realizes.project step (treeStep _) (·.tree) (fun _ _ => rfl)
    (tree_realizes_band _ f.tree s.tree h i.val)
  exact ht.congr (fun _ _ => rfl)

/-- Finite linear combinations compose on streams as on transfer functions. -/
theorem realizes_sum {S : Type} (p : Step S (Sample K)) (initial : S)
    (fields : Nat → S → K) (transfers : Nat → Stream K) (n : Nat)
    (h : ∀ i, i < n → Realizes p initial (fields i) (transfers i)) :
    Realizes p initial (fun s => ∑ i ∈ Finset.range n, fields i s)
      (∑ i ∈ Finset.range n, transfers i) := by
  induction n with
  | zero =>
    intro u
    simp only [Finset.range_zero, Finset.sum_empty, zero_mul]
    change embed (PowerSeries.mk (fun _ => (0 : K))) = 0
    have hz : PowerSeries.mk (fun _ => (0 : K)) = 0 := by ext; simp
    rw [hz, map_zero]
  | succ n ih =>
    simpa only [Finset.sum_range_succ] using
      (ih (fun i hi => h i (Nat.lt_succ_of_lt hi))).add (h n (Nat.lt_succ_self n))

/-- Summing the actual stored output bands realizes the common all-pass phase. -/
theorem realizes_reconstruction {n : Nat} (f : State n K) (s : Snapshot n K)
    (hn : 0 < n) (h : TreeReady _ f.tree s.tree) :
    Realizes step f (fun v => ∑ i : Fin n, v.bands i)
      (phaseTransfer _ (mapData (Functor.map const) _ s.tree) sigma 1) := by
  have ht := realizes_sum step f (fun i v => treeBand _ v.tree i)
    (fun i => bandTransfer _ (mapData (Functor.map const) _ s.tree) sigma 1 i) n
    (fun i _ => Realizes.project step (treeStep _) (·.tree) (fun _ _ => rfl)
      (tree_realizes_band _ f.tree s.tree h i))
  have hs := sum_bandTransfer (Topology.balanced n)
    (mapData (Functor.map const) _ s.tree) sigma 1
  rw [Topology.balanced_bands n hn] at hs
  apply (ht.transfer_eq hs).congr
  intro v x
  simp only [step, Fin.sum_univ_eq_sum_range]

end DSP.LinkwitzRileySplitter
end

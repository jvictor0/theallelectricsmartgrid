import DSP.Filters.LinkwitzRileySplitter.Tuning

noncomputable section
namespace DSP.LinkwitzRileySplitter
open Channels

variable {R S : Type}

def mapPhase (f : R → S) : (t : Topology) → PhaseState R t → PhaseState S t
  | .leaf, s => ⟨f s.x⟩
  | .branch low high, s => ⟨f <$> s.svf, mapPhase f low s.low, mapPhase f high s.high⟩

def mapTree (f : R → S) : (t : Topology) → TreeState R t → TreeState S t
  | .leaf, s => ⟨f s.x⟩
  | .branch low high, s => ⟨f <$> s.first, f <$> s.second,
      mapPhase f high s.lowPhase, mapPhase f low s.highPhase,
      mapTree f low s.low, mapTree f high s.high⟩

instance {n : Nat} : Functor (State n) where
  map f s := ⟨mapTree f _ s.tree, fun i => f (s.bands i)⟩

instance {n : Nat} : Functor (LogicalSpec n) where
  map f s := ⟨mapData f _ s.tree⟩

instance {n : Nat} : Functor (Snapshot n) where
  map f s := ⟨mapData (Functor.map f) _ s.tree⟩

theorem phaseOutput_map (f : R → S) (t : Topology) (s : PhaseState R t) :
    phaseOutput t (mapPhase f t s) = f (phaseOutput t s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh => exact hh s.high

theorem treeBand_map (f : R → S) (t : Topology) (s : TreeState R t) (i : Nat) :
    treeBand t (mapTree f t s) i = f (treeBand t s i) := by
  induction t generalizing i with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [mapTree, treeBand]
    split <;> simp only [hl, hh]

section Processing
variable [Add R] [Sub R] [Neg R] [Mul R] [FMA R] [OfNat R 2] {m : Nat}

theorem phaseStep_channel (i : Fin m) (t : Topology) (s : PhaseState (Fin m → R) t)
    (x : Sample (Fin m → R)) :
    mapPhase (· i) t (phaseStep t s x) = phaseStep t (mapPhase (· i) t s) (channel i x) := by
  induction t generalizing x with
  | leaf => rfl
  | branch low high hl hh =>
    have hlo := congrArg (phaseOutput low) (hl s.low ⟨(s.svf.step x).allPass⟩)
    simp only [phaseOutput_map] at hlo
    simp only [phaseStep, mapPhase, hl, hh]
    congr 1
    exact congrArg (fun v => phaseStep high (mapPhase (· i) high s.high) ⟨v⟩) hlo

theorem treeStep_channel (i : Fin m) (t : Topology) (s : TreeState (Fin m → R) t)
    (x : Sample (Fin m → R)) :
    mapTree (· i) t (treeStep t s x) = treeStep t (mapTree (· i) t s) (channel i x) := by
  induction t generalizing x with
  | leaf => rfl
  | branch low high hl hh =>
    have hlo := congrArg (phaseOutput high) (phaseStep_channel i high s.lowPhase
      ⟨(s.second.step ⟨(s.first.step x).lowPass⟩).lowPass⟩)
    have hhi := congrArg (phaseOutput low) (phaseStep_channel i low s.highPhase
      ⟨(s.first.step x).allPass - (s.second.step ⟨(s.first.step x).lowPass⟩).lowPass⟩)
    simp only [phaseOutput_map] at hlo hhi
    simp only [treeStep, mapTree, phaseStep_channel, hl, hh]
    congr 1
    · exact congrArg (fun v => treeStep low (mapTree (· i) low s.low) ⟨v⟩) hlo
    · exact congrArg (fun v => treeStep high (mapTree (· i) high s.high) ⟨v⟩) hhi

theorem natural {n : Nat} : Natural (n := m) (step (R := Fin m → R) (numBands := n)) step := by
  intro i s x
  change State.mk (mapTree (· i) _ (treeStep _ s.tree x)) _ = _
  simp only [step, channel, Functor.map]
  congr 1
  · exact treeStep_channel i _ s.tree x
  · funext band
    have hb := congrArg (fun v => treeBand _ v band.val) (treeStep_channel i _ s.tree x)
    simpa only [treeBand_map, channel, Functor.map] using hb

end Processing

section Tuning
variable [OfNat R 1] [OfNat R 8] [Add R] [Mul R] [Div R] [FMA R] [Trig R] {m : Nat}

theorem tunePhase_channel (i : Fin m) (t : Topology) (spec : TreeData (Fin m → R) t)
    (s : PhaseState (Fin m → R) t) :
    mapPhase (· i) t (tunePhase t spec s) = tunePhase t (mapData (· i) t spec) (mapPhase (· i) t s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tunePhase, mapPhase, mapData, hl, hh]
    rfl

theorem tuneTree_channel (i : Fin m) (t : Topology) (spec : TreeData (Fin m → R) t)
    (s : TreeState (Fin m → R) t) :
    mapTree (· i) t (tuneTree t spec s) = tuneTree t (mapData (· i) t spec) (mapTree (· i) t s) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [tuneTree, mapTree, mapData, tunePhase_channel, hl, hh]
    rfl

theorem computeCoefficients_channel {n : Nat} (i : Fin m) (spec : LogicalSpec n (Fin m → R))
    (s : State n (Fin m → R)) :
    channel i (computeCoefficients spec s) = computeCoefficients (channel i spec) (channel i s) := by
  change State.mk (mapTree (· i) _ (tuneTree _ spec.tree s.tree)) _ = _
  rw [tuneTree_channel]
  rfl

omit [Add R] [Mul R] [FMA R] in
theorem snapshotTree_channel (i : Fin m) (t : Topology) (spec : TreeData (Fin m → R) t) :
    mapData (Functor.map (· i)) t (snapshotTree t spec) = snapshotTree t (mapData (· i) t spec) := by
  induction t with
  | leaf => rfl
  | branch low high hl hh =>
    simp only [snapshotTree, mapData, hl, hh]
    rfl

omit [Add R] [Mul R] [FMA R] in
theorem makeSnapshot_channel {n : Nat} (i : Fin m) (spec : LogicalSpec n (Fin m → R)) :
    channel i (makeSnapshot spec) = makeSnapshot (channel i spec) := by
  change Snapshot.mk (mapData (Functor.map (· i)) _ (snapshotTree _ spec.tree)) = _
  rw [snapshotTree_channel]
  rfl

end Tuning

variable {K : Type} [Field K] {m n : Nat}

theorem realizes_band_channels (f : State n (Fin m → K)) (s : Snapshot n (Fin m → K))
    (h : ∀ i, TreeReady _ (channel i f).tree (channel i s).tree) (band : Fin n) :
    Channels.Realizes step f (fun v => v.bands band) (fun i => bandTransfer _
      (mapData (Functor.map Bilinear.const) _ (channel i s).tree) Bilinear.sigma 1 band.val) :=
  Channels.realizes natural (fun i => realizes_band (channel i f) (channel i s) (h i) band)
    (fun _ _ => rfl)

end DSP.LinkwitzRileySplitter
end

import DSP.Resynthesis.FrameStorage
import DSP.Resynthesis.PVDRRoots
import DSP.Resynthesis.Real

namespace DSP.Resynthesis.FrameStorage

open SpectrumStorage (Lane)

def installGraphResult {R P : Type} (state : Scene R P) (lane : Lane)
    (result : (BinTable GateState 4096 × PVDRGraph P 4096) × Nat) : Scene R P :=
  { state with
    spectrum.gates := Function.update state.spectrum.gates lane result.1.1
    spectrum.graph := Function.update state.spectrum.graph lane result.1.2
    rng := result.2 }

private theorem analyzeGraph_run {R P : Type} [OfNat R 0] [LT R] [LE R] [DecidableLT R]
    [DecidableLE R] [Max R] [Sub R] [OfNat P 0] [Add P] [Mul P]
    (state : Scene R P) (lane : Lane) :
    let math := state.spectrum.math
    let queue := PVDR.binaryQueue (n := 4096) (fun (a b : R) => decide (a < b))
    let result := (PVDR.analyze math.widen (math.natPhase 1024) math.thresholds queue
      state.draw (state.spectrum.gates lane) (state.spectrum.input lane)).run state.rng
    (analyzeGraph lane).run state = (⟨⟩, installGraphResult state lane result) :=
  rfl

/-- Pure graph analysis and resulting random state in the exact interpretation. -/
noncomputable def exactGraphResult {S Q : Type}
    (queue : PVDR.Queue ℝ Q 4096) (draw : StateM S ℝ) (gates : BinTable GateState 4096)
    (input : PVDRInput ℝ ℝ 4096) (rng : S) : (BinTable GateState 4096 × PVDRGraph ℝ 4096) × S :=
  (PVDR.analyze id (exactMath.natPhase 1024) exactMath.thresholds queue draw gates input).run rng

/-- The storage step installs the pure graph analysis result and its random state. -/
theorem analyzeGraph_run_exact (state : Scene ℝ ℝ) (lane : Lane)
    (hMath : state.spectrum.math = exactMath) :
    let queue := PVDR.binaryQueue (n := 4096) (fun (a b : ℝ) => decide (a < b))
    let result := exactGraphResult queue state.draw (state.spectrum.gates lane)
      (state.spectrum.input lane) state.rng
    (analyzeGraph lane).run state = (⟨⟩, installGraphResult state lane result) := by
  dsimp only
  let queue := PVDR.binaryQueue (n := 4096) (fun (a b : ℝ) => decide (a < b))
  let result := fun math : GrainMath ℝ ℝ =>
    (PVDR.analyze math.widen (math.natPhase 1024) math.thresholds queue
      state.draw (state.spectrum.gates lane) (state.spectrum.input lane)).run state.rng
  have hWiden : exactMath.widen = id := rfl
  have hResult : result state.spectrum.math =
      exactGraphResult queue state.draw (state.spectrum.gates lane)
        (state.spectrum.input lane) state.rng := by
    calc
      _ = result exactMath := congrArg result hMath
      _ = _ := by unfold result exactGraphResult; rw [hWiden]
  refine (analyzeGraph_run state lane).trans ?_
  exact congrArg (fun r => ((PUnit.unit : PUnit), installGraphResult state lane r)) hResult

/-- The pure graph has at most one entry per active bin and each visited bin points
to a self root. Queue membership is the only queue law needed for the root claim. -/
theorem exactGraph_bounds_and_roots {S Q : Type}
    (queue : PVDR.Queue ℝ Q 4096) (law : PVDR.QueueMembership queue)
    (draw : StateM S ℝ) (gates : BinTable GateState 4096) (input : PVDRInput ℝ ℝ 4096)
    (rng : S) :
    let graph := (exactGraphResult queue draw gates input rng).1.2
    graph.order.length ≤ 2047 ∧
      ∀ bin ∈ graph.order,
        let root := (graph.link.get bin).root
        root ∈ graph.order ∧ (graph.link.get root).root = root := by
  unfold exactGraphResult
  let graph := ((PVDR.analyze id (exactMath.natPhase 1024) exactMath.thresholds queue
    draw gates input).run rng).1.2
  change graph.order.length ≤ 2047 ∧ _
  have nodup : graph.order.Nodup :=
    PVDR.analyze_order_nodup id (exactMath.natPhase 1024) exactMath.thresholds queue draw gates input rng
  have roots := PVDR.analyze_roots (exactMath.natPhase 1024) exactMath.thresholds queue law draw gates input rng
  refine ⟨?_, ?_⟩
  · simpa using nodup.length_le_card
  · intro bin mem
    exact ⟨(roots bin mem).1, (roots bin mem).2.1⟩

end DSP.Resynthesis.FrameStorage

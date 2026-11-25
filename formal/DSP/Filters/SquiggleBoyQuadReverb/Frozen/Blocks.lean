import DSP.Filters.SquiggleBoyQuadReverb.Runtime
import DSP.Analysis.LocalDynamics
import DSP.DelayBuffer.Transfer
import DSP.Nonlinear.Analysis
import Mathlib.Analysis.Normed.Module.FiniteDimension
import Mathlib.Analysis.Normed.Group.Constructions

/-! Logical histories and prepared cubic stencils for the frozen reverb. The
stored samples are ordered in the same way as `DelayBuffer.history`. -/
namespace DSP.SquiggleBoyQuadReverb.Frozen

open scoped BigOperators

abbrev Lanes (K : Type) := Fin 4 → K
abbrev Tape (n : ℕ) (K : Type) := Fin n → Lanes K
abbrev IntroHistory (order : List (Fin 8)) (K : Type) :=
  (j : Fin order.length) → Tape (squiggleBoyReverbIntroAPFCapacity (order.get j)) K

/-- The four pre-addressed reads are in oldest-to-newest order. -/
structure Tap (n : ℕ) (K : Type) where
  age : Fin 4 → Fin 4 → Fin n
  fraction : Lanes K

abbrev IntroTaps (order : List (Fin 8)) (K : Type) :=
  (j : Fin order.length) → Tap (squiggleBoyReverbIntroAPFCapacity (order.get j)) K

def introCons {F : Fin 8 → Type} {i : Fin 8} {order : List (Fin 8)}
    (head : F i) (tail : (j : Fin order.length) → F (order.get j)) :
    (j : Fin (i :: order).length) → F ((i :: order).get j) :=
  Fin.cases head tail

def laneGain {K : Type} [Field K] (g : Lanes K) : Lanes K →ₗ[K] Lanes K :=
  LinearMap.pi fun i => g i • LinearMap.proj i

def mixer {K : Type} [Field K] [OfScientific K]
    (literal : ∀ (a b : Lanes K) (c : K),
      squiggleBoyReverbMix (a + b) = squiggleBoyReverbMix a + squiggleBoyReverbMix b ∧
      squiggleBoyReverbMix (c • a) = c • squiggleBoyReverbMix a) :
    Lanes K →ₗ[K] Lanes K :=
  ⟨⟨squiggleBoyReverbMix, fun a b => (literal a b 0).1⟩, fun c a => (literal a 0 c).2⟩

/-- Inlining the four Givens rotations gives a linear map for any field and
the same coefficient literals as the runtime mixer. -/
def mix {K : Type} [Field K] [OfScientific K] : Lanes K →ₗ[K] Lanes K :=
  mixer fun a b c => by
    constructor <;> ext i <;> fin_cases i <;>
      simp [squiggleBoyReverbMix, givensPair, Matrix.cons_val_two, Matrix.cons_val_three,
        Matrix.vecHead, Matrix.vecTail, Function.comp_apply] <;> ring

def cubicWeights {K : Type} [Field K] (t : K) : Fin 4 → K :=
  ![-t * (1-t)^2 / 2, 1 - 5*t^2/2 + 3*t^3/2,
    t/2 + 4*t^2/2 - 3*t^3/2, -t^2*(1-t)/2]

theorem cubicInterpolate_eq_sum {K : Type} [Field K] [OfScientific K]
    (half : (0.5 : K) = 1 / 2) (v : Fin 4 → K) (t : K) :
    DelayBuffer.cubicInterpolate (v 0) (v 1) (v 2) (v 3) t =
      ∑ j, cubicWeights t j * v j := by
  simp [DelayBuffer.cubicInterpolate, fma_eq, half, Fin.sum_univ_four, cubicWeights]
  ring

def tapRead {n : ℕ} {K : Type} [Field K] (p : Tap n K) : Tape n K →ₗ[K] Lanes K :=
  LinearMap.pi fun i => ∑ j, cubicWeights (p.fraction i) j •
    ((LinearMap.proj i : Lanes K →ₗ[K] K).comp (LinearMap.proj (p.age i j)))

@[simp] theorem tapRead_apply {n : ℕ} {K : Type} [Field K] (p : Tap n K)
    (history : Tape n K) (i : Fin 4) :
    tapRead p history i = ∑ j, cubicWeights (p.fraction i) j * history (p.age i j) i := by
  simp [tapRead, LinearMap.sum_apply]

def push {n : ℕ} {K : Type} [Field K] : Tape n K × Lanes K →ₗ[K] Tape n K :=
  LinearMap.pi fun age => if h : age.val = 0 then LinearMap.snd _ _ _
    else (LinearMap.proj ⟨age.val-1, by omega⟩).comp (LinearMap.fst _ _ _)

theorem push_eq_shift {n : ℕ} {K : Type} [Field K] (history : Tape n K) (x : Lanes K) :
    push (history, x) = IIR.shift x history := by
  cases n with
  | zero => exact funext fun age => age.elim0
  | succ n =>
    funext age
    refine Fin.cases ?_ (fun j => ?_) age
    · rfl
    · simp [push, IIR.shift, Fin.cases_succ]
      rfl

def allPass {n : ℕ} {K : Type} [Field K] (p : Tap n K) (gain : K) :
    Tape n K × Lanes K →ₗ[K] Tape n K × Lanes K :=
  let history := LinearMap.fst _ _ _
  let input := LinearMap.snd _ _ _
  let out := (tapRead p).comp history - gain • input
  let written := input + gain • out
  (push.comp (history.prod written)).prod out

theorem allPass_apply {n : ℕ} {K : Type} [Field K] (p : Tap n K) (gain : K)
    (history : Tape n K) (input : Lanes K) :
    allPass p gain (history, input) =
      let y := tapRead p history - gain • input
      (push (history, input + gain • y), y) := rfl

def intro {K : Type} [Field K] (gain : K) :
    (order : List (Fin 8)) → IntroTaps order K →
      IntroHistory order K × Lanes K →ₗ[K] IntroHistory order K × Lanes K
  | [], _ => (0 : _ →ₗ[K] IntroHistory [] K).prod (LinearMap.snd _ _ _)
  | i :: order, taps =>
    let history := LinearMap.fst K (IntroHistory (i :: order) K) (Lanes K)
    let input := LinearMap.snd K (IntroHistory (i :: order) K) (Lanes K)
    let head := (allPass (taps 0) gain).comp (((LinearMap.proj 0).comp history).prod input)
    let tailIn := LinearMap.pi fun j => (LinearMap.proj j.succ).comp history
    let tail := (intro gain order (fun j => taps j.succ)).comp (tailIn.prod (LinearMap.snd _ _ _ |>.comp head))
    let saved := LinearMap.pi <| introCons
      (F := fun stage => (IntroHistory (i :: order) K × Lanes K) →ₗ[K]
        Tape (squiggleBoyReverbIntroAPFCapacity stage) K) (i := i) (order := order)
      ((LinearMap.fst _ _ _).comp head) (fun j => ((LinearMap.proj j).comp (LinearMap.fst _ _ _)).comp tail)
    saved.prod ((LinearMap.snd _ _ _).comp tail)

theorem intro_cons {K : Type} [Field K] (gain : K) (i : Fin 8) (order : List (Fin 8))
    (p : IntroTaps (i :: order) K) (h : IntroHistory (i :: order) K) (x : Lanes K) :
    intro gain (i :: order) p (h, x) =
      let head := allPass (p 0) gain (h 0, x)
      let tail := intro gain order (fun j => p j.succ) ((fun j => h j.succ), head.2)
      (introCons (F := fun i => Tape (squiggleBoyReverbIntroAPFCapacity i) K)
        (i := i) (order := order) head.1 tail.1, tail.2) := by
  apply Prod.ext
  · funext j
    refine Fin.cases ?_ (fun k => ?_) j <;> rfl
  · rfl

/-- One TPT integrator with its low-pass output, on logical memory coordinates. -/
def lowPass {K : Type} [Field K] (alpha : Lanes K) :
    Lanes K × Lanes K →ₗ[K] Lanes K × Lanes K :=
  let history := LinearMap.fst K (Lanes K) (Lanes K)
  let input := LinearMap.snd K (Lanes K) (Lanes K)
  let advance := (laneGain alpha).comp (input - history)
  let low := history + advance
  (low + advance).prod low

def damping {K : Type} [Field K] (high low : Lanes K) :
    (Lanes K × Lanes K) × Lanes K →ₗ[K] (Lanes K × Lanes K) × Lanes K :=
  let state := LinearMap.fst K (Lanes K × Lanes K) (Lanes K)
  let input := LinearMap.snd K (Lanes K × Lanes K) (Lanes K)
  let h := (lowPass high).comp (((LinearMap.fst _ _ _).comp state).prod input)
  let l := (lowPass low).comp (((LinearMap.snd _ _ _).comp state).prod
    (input - (LinearMap.snd _ _ _).comp h))
  (((LinearMap.fst _ _ _).comp h).prod ((LinearMap.fst _ _ _).comp l)).prod
    ((LinearMap.snd _ _ _).comp l)

end DSP.SquiggleBoyQuadReverb.Frozen

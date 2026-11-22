import DSP.Filters.SchroederAllPass.FilterProofs
import DSP.Filters.SchroederAllPass.Wandering
import Mathlib.Data.Fin.VecNotation

namespace DSP

/-- The eight serial input diffusion taps in 192 kHz samples, in processing order. -/
def squiggleBoyReverbIntroAPFsDelayTime : Fin 8 → Nat :=
  ![293, 487, 907, 1433, 4493, 6899, 9811, 14033]

def squiggleBoyReverbIntroCapacities : List Nat :=
  [2048, 2048, 2048, 2048, 8192, 8192, 16384, 16384]

def squiggleBoyReverbIntroAPFCapacity (stage : Fin 8) : Nat :=
  squiggleBoyReverbIntroCapacities[stage.val]'stage.isLt

/-- Processing order shared by both Lean cascades and the C++ storage renderer. -/
def squiggleBoyReverbIntroOrder : List (Fin 8) := [0, 1, 2, 3, 4, 5, 6, 7]

private def buildIntroCascade {Cascade : List Nat → Type} (empty : Cascade [])
    (series : (i : Fin 8) → {rest : List Nat} → Cascade rest →
      Cascade (squiggleBoyReverbIntroAPFCapacity i :: rest)) :
    (order : List (Fin 8)) → Cascade (order.map squiggleBoyReverbIntroAPFCapacity)
  | [] => empty
  | i :: tail => series i (buildIntroCascade empty series tail)

abbrev SquiggleBoyQuadReverbIntroAPFs (R : Type) :=
  SchroederAllPassCascade 4 R (squiggleBoyReverbIntroOrder.map squiggleBoyReverbIntroAPFCapacity)

def squiggleBoyQuadReverbIntroAPFs {R : Type} [OfNat R 0] (gain : Fin 4 → R) :
    SquiggleBoyQuadReverbIntroAPFs R :=
  have fits : ∀ stage : Fin 8,
      0 < squiggleBoyReverbIntroAPFsDelayTime stage ∧
        squiggleBoyReverbIntroAPFsDelayTime stage ≤ squiggleBoyReverbIntroAPFCapacity stage := by decide
  let stage (i : Fin 8) : SchroederAllPassFilterGeneric (squiggleBoyReverbIntroAPFCapacity i) 4 R :=
    .uniform (squiggleBoyReverbIntroAPFsDelayTime i) (fits i).1 (fits i).2 gain
  buildIntroCascade (Cascade := SchroederAllPassCascade 4 R) (.passthrough (fun _ => 0))
    (fun i {_} tail => .series (stage i) tail) squiggleBoyReverbIntroOrder

theorem squiggleBoyQuadReverbIntroAPFs_realizes {K : Type} [Field K] (gain : Fin 4 → K) :
    let chain := squiggleBoyQuadReverbIntroAPFs gain
    Channels.Realizes SchroederAllPassCascade.step chain SchroederAllPassCascade.y
      (chain.response Bilinear.const Bilinear.delay) := by
  apply SchroederAllPassCascade.realizes
  simp only [squiggleBoyQuadReverbIntroAPFs, buildIntroCascade, squiggleBoyReverbIntroOrder,
    SchroederAllPassCascade.AtRest, and_true]
  repeat' first | exact SchroederAllPassFilterGeneric.uniform_atRest _ _ _ _ | constructor

/-- The fixed-tap intro cascade has unit magnitude on the unit circle. -/
theorem squiggleBoyQuadReverbIntroAPFs_response_unit (gain : Fin 4 → ℝ)
    (ch : Fin 4) (q : ℂ) (stable : |gain ch| < 1) (unit : Complex.normSq q = 1) :
    Complex.normSq ((squiggleBoyQuadReverbIntroAPFs gain).response (fun g : ℝ => (g : ℂ)) q ch) = 1 := by
  apply SchroederAllPassCascade.response_unit _ ch q unit
  simp [squiggleBoyQuadReverbIntroAPFs, buildIntroCascade, squiggleBoyReverbIntroOrder, SchroederAllPassCascade.StableGains,
    SchroederAllPassFilterGeneric.gain, SchroederAllPassFilterGeneric.uniform,
    SchroederAllPassFilterGeneric.init, stable]

/-- The exponential speed draw has mean one knot per 1,920,000 samples. -/
def squiggleBoyReverbLambdaSamples : Nat := 1920000

def squiggleBoyReverbWanderSpec (enabled : Bool) : WanderingSchroederAllPassGeneric.WanderSpec :=
  ⟨if enabled then 128 else 0, Float.ofNat squiggleBoyReverbLambdaSamples⟩

theorem squiggleBoyReverbIntroWanderFits (enabled : Bool) (stage : Fin 8) :
    DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity stage)
      (squiggleBoyReverbIntroAPFsDelayTime stage) (squiggleBoyReverbWanderSpec enabled).depth := by
  have all : ∀ (e : Bool) (s : Fin 8),
      DelayBuffer.CubicOffsetFits (squiggleBoyReverbIntroAPFCapacity s)
        (squiggleBoyReverbIntroAPFsDelayTime s) (squiggleBoyReverbWanderSpec e).depth := by decide
  exact all enabled stage

def squiggleBoyReverbIntroSeed (stage : Fin 8) : UInt64 :=
  UInt64.ofNat (stage.val + 1)

def squiggleBoyReverbLoopSeed (stage : Fin 2) : UInt64 :=
  UInt64.ofNat (9 + stage.val)

abbrev SquiggleBoyWanderingIntroAPFs (R : Type) :=
  WanderingSchroederAllPassCascade 4 R (squiggleBoyReverbIntroOrder.map squiggleBoyReverbIntroAPFCapacity)

namespace SquiggleBoyWanderingIntroAPFs

def init {R : Type} [OfNat R 0] [SmoothRandomLFO.SampleValue R] [Sub R] [Div R] [FMA R]
    (enabled : Bool) (gain : Fin 4 → R) : SquiggleBoyWanderingIntroAPFs R :=
  let spec := squiggleBoyReverbWanderSpec enabled
  let stage (i : Fin 8) : WanderingSchroederAllPassGeneric (squiggleBoyReverbIntroAPFCapacity i) 4 R :=
    .uniform (squiggleBoyReverbIntroAPFsDelayTime i) gain spec (squiggleBoyReverbIntroSeed i)
      (squiggleBoyReverbIntroWanderFits enabled i)
  buildIntroCascade (Cascade := WanderingSchroederAllPassCascade 4 R) (.passthrough (fun _ => 0))
    (fun i {_} tail => .series (stage i) tail) squiggleBoyReverbIntroOrder

end SquiggleBoyWanderingIntroAPFs
end DSP

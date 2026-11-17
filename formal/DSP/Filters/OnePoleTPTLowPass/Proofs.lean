import DSP.Filters.OnePoleTPTLowPass
import DSP.Transfer.Analog
import DSP.Transfer.Digital
import DSP.Transfer.Delay
import DSP.Transfer.Bilinear
import DSP.Processor.Run
import Mathlib.Tactic.FieldSimp
import Mathlib.Tactic.LinearCombination

namespace DSP.OnePoleTPTLowPass

open Matrix Polynomial

/-! The bilinear variable is normalized: the physical analog prototype
ωc / (s + ωc) at s = c (z - 1) / (z + 1) is the prototype g / (s + g) below at
(z - 1) / (z + 1), with g = ωc / c. Plain trapezoidal integration uses c = 2/T,
so g = ωc T / 2; prewarping the cutoff uses g = tan(ωc T / 2).
-/

variable {K : Type}

/-- Standard observation: the output reads the old state and the current input. -/
def discrete [CommRing K] (G : K) : StateSpace 1 K where
  A _ _ := 1 - 2 * G
  B _ := 2 * G
  C _ := 1 - G
  D := G

/-- Normalized analog lowpass g / (s + g), with state equal to its output. -/
def prototype [CommRing K] (g : K) : StateSpace 1 K where
  A _ _ := -g
  B _ := g
  C _ := 1
  D := 0

def bilinear [Field K] (z : K) : K := (z - 1) / (z + 1)

section Realization

variable [CommRing K]

/-- The executable operation order is the state-space update over exact rings;
the stored output is the state-space observation of the old state. -/
theorem exported_real_step (f : OnePoleTPTLowPass K) (input : Sample K) :
    (fun _ : Fin 1 => (step f input).s, (step f input).y) =
      (discrete f.G).processor (fun _ => f.s) input.x := by
  simp only [fma_eq, StateSpace.processor, StateSpace.snapshot_values, StateSpace.observe,
    StateSpace.dot, discrete, step_s, step_y, respond_eq]
  refine Prod.ext (funext fun _ => ?_) ?_ <;>
    simp only [StateSpace.advance, StateSpace.dot, fma_eq] <;> ring

theorem states_eq (u : ℕ → K) (initial : OnePoleTPTLowPass K) (n : ℕ) :
    (Step.states step (fun k => ⟨u k⟩) initial n).G = initial.G ∧
      (fun _ : Fin 1 => (Step.states step (fun k => ⟨u k⟩) initial n).s) =
        Processor.runState (discrete initial.G).processor u (fun _ => initial.s) n := by
  induction n with
  | zero => exact ⟨rfl, rfl⟩
  | succ n ih =>
    obtain ⟨hG, hs⟩ := ih
    have h := congrArg Prod.fst
      (exported_real_step (Step.states step (fun k => ⟨u k⟩) initial n) ⟨u n⟩)
    rw [hG, hs] at h
    exact ⟨by simp [hG], h⟩

/-- Every stored output, from every initial state, is the state-space output. -/
theorem run_eq (u : ℕ → K) (initial : OnePoleTPTLowPass K) (n : ℕ) :
    (Step.states step (fun k => ⟨u k⟩) initial (n + 1)).y =
      Processor.run (discrete initial.G).processor u (fun _ => initial.s) n := by
  obtain ⟨hG, hs⟩ := states_eq u initial n
  have h := congrArg Prod.snd
    (exported_real_step (Step.states step (fun k => ⟨u k⟩) initial n) ⟨u n⟩)
  rw [hG, hs] at h
  exact h

end Realization

/-- Outputs obey the trapezoidal rule for y' = ω (x - y), with g = ω T / 2,
for arbitrary input and initial state. The zero-delay loop makes y[n+1]
depend on x[n+1] in the same sample. -/
theorem trapezoidal [Field K] (g : K) (hg : 1 + g ≠ 0) (u : ℕ → K) (s y₀ : K) (n : ℕ) :
    let y := fun k => (Step.states step (fun k => ⟨u k⟩)
      ({ G := g / (1 + g), s := s, y := y₀ } : OnePoleTPTLowPass K) (k + 1)).y
    y (n + 1) = y n + g * ((u (n + 1) - y (n + 1)) + (u n - y n)) := by
  intro y
  have hG := fun k => (states_eq u ({ G := g / (1 + g), s := s, y := y₀ } : OnePoleTPTLowPass K) k).1
  simp only [fma_eq, y, Step.states_succ, step_y, step_s, respond_eq, hG]
  field_simp
  ring

/-- A solved coefficient G (1 + g) = g makes the loop well posed: 1 + g ≠ 0. -/
theorem one_add_ne_zero [Field K] (G g : K) (hG : G * (1 + g) = g) : 1 + g ≠ 0 := by
  intro h
  have hg : g = 0 := by linear_combination -hG + G * h
  rw [hg] at h
  simp at h

theorem denominator_ne_zero [Field K] (G g : K) (hG : G * (1 + g) = g) :
    Bilinear.bil (X + Polynomial.C g) ≠ 0 := by
  apply Bilinear.bil_ne_zero
  simpa only [eval_add, eval_X, eval_C] using one_add_ne_zero G g hG

/-- From rest, the stored output realizes the bilinear image of g / (s + g).
The coefficient hypothesis G (1 + g) = g is the solved zero-delay loop. -/
theorem realizes [Field K] (f : OnePoleTPTLowPass K) (g : K) (hG : f.G * (1 + g) = g)
    (hs : f.s = 0) :
    Bilinear.Realizes step f (·.y) (Bilinear.analog (Polynomial.C g) (X + Polynomial.C g)) := by
  intro u
  let st := Step.states step (fun k => ⟨u k⟩) f
  change Bilinear.ser (fun n => (st (n + 1)).y) = _
  have hGn : ∀ n, (st n).G = f.G := fun n => Step.states_invariant step (·.G) (fun _ _ => rfl) _ f n
  have hI := Bilinear.integrator (fun n => (st n).s) (fun n => (st (n + 1)).y)
    (fun n => u n - (st (n + 1)).y) g hs
    (fun n => by simp only [fma_eq, st, Step.states_succ, step_s, step_y, respond_eq]; ring)
    (fun n => by
      simp only [fma_eq, st, Step.states_succ, step_y, respond_eq]
      rw [hGn]
      linear_combination (u n - (st n).s) * hG)
  rw [Bilinear.ser_sub] at hI
  apply Bilinear.of_cleared (denominator_ne_zero f.G g hG)
  rw [Bilinear.bil_C, Bilinear.bil_onePole]
  linear_combination hI

section Transfer

variable [Field K]

theorem pole_factor (g z : K) (hg : 1 + g ≠ 0) :
    z - (1 - 2 * (g / (1 + g))) = ((z - 1) + g * (z + 1)) / (1 + g) := by
  field_simp
  ring

theorem discrete_regular (g z : K) (hg : 1 + g ≠ 0) (hpole : (z - 1) + g * (z + 1) ≠ 0) :
    (discrete (g / (1 + g))).RegularAt z := by
  simp only [StateSpace.RegularAt, StateSpace.resolvent, Matrix.det_unique]
  simpa [discrete, pole_factor g z hg] using div_ne_zero hpole hg

theorem prototype_regular (g s : K) (hs : s + g ≠ 0) : (prototype g).RegularAt s := by
  simp only [StateSpace.RegularAt, StateSpace.resolvent, Matrix.det_unique]
  simpa [prototype] using hs

theorem discrete_transfer (g z : K) (hg : 1 + g ≠ 0) (hpole : (z - 1) + g * (z + 1) ≠ 0) :
    (discrete (g / (1 + g))).transfer z = g * (z + 1) / ((z - 1) + g * (z + 1)) := by
  symm
  apply ((discrete (g / (1 + g))).modeEquations_iff_transfer z _ (discrete_regular g z hg hpole)).mp
  refine ⟨fun _ => 2 * g / ((z - 1) + g * (z + 1)), ?_, ?_⟩
  · funext i
    fin_cases i
    simp [discrete, Matrix.mulVec, dotProduct]
    field_simp
    ring
  · simp [discrete, dotProduct]
    field_simp
    ring

theorem prototype_transfer (g s : K) (hs : s + g ≠ 0) :
    (prototype g).transfer s = g / (s + g) := by
  symm
  apply ((prototype g).modeEquations_iff_transfer s _ (prototype_regular g s hs)).mp
  refine ⟨fun _ => g / (s + g), ?_, ?_⟩
  · funext i
    fin_cases i
    simp [prototype, Matrix.mulVec, dotProduct]
    field_simp
    ring
  · simp [prototype, dotProduct]

theorem bilinear_regular (g z : K) (hz : z + 1 ≠ 0) (hpole : (z - 1) + g * (z + 1) ≠ 0) :
    bilinear z + g ≠ 0 := by
  have h : bilinear z + g = ((z - 1) + g * (z + 1)) / (z + 1) := by
    unfold bilinear
    field_simp
  rw [h]
  exact div_ne_zero hpole hz

/-- The digital transfer is the analog prototype's at the bilinear frequency. -/
theorem bilinear_transfer (g z : K) (hg : 1 + g ≠ 0) (hz : z + 1 ≠ 0)
    (hpole : (z - 1) + g * (z + 1) ≠ 0) :
    (discrete (g / (1 + g))).transfer z = (prototype g).transfer (bilinear z) := by
  rw [discrete_transfer g z hg hpole, prototype_transfer g _ (bilinear_regular g z hz hpole)]
  unfold bilinear
  field_simp

theorem delay_denominator (G : K) :
    (discrete G).delayDenominator = 1 - Polynomial.C (1 - 2 * G) * X := by
  simp [StateSpace.delayDenominator, StateSpace.delayDenominatorAt, StateSpace.delayMatrix,
    StateSpace.map, discrete]
  ring

theorem delay_numerator (G : K) :
    (discrete G).delayNumerator = Polynomial.C G * (1 + X) := by
  simp [StateSpace.delayNumerator, StateSpace.delayNumeratorAt, StateSpace.delayDenominatorAt,
    StateSpace.delayMatrix, StateSpace.map, discrete,
    Matrix.adjugate_fin_one, dotProduct, Matrix.mulVec]
  rw [show (Polynomial.C (2 : K)) = 2 from map_ofNat _ 2]
  ring

/-- From rest, the executable processor realizes G (1 + q) / (1 - (1 - 2G) q). -/
theorem iir_equivalent (G y₀ : K) (u : ℕ → K) (n : ℕ) :
    Processor.run (IIR.processor (IIR.ofPolynomials (Polynomial.C G * (1 + X))
        (1 - Polynomial.C (1 - 2 * G) * X))) u IIR.zeroState n =
      (Step.states step (fun k => ⟨u k⟩)
        ({ G := G, s := 0, y := y₀ } : OnePoleTPTLowPass K) (n + 1)).y := by
  have h := (discrete G).toIIR_correct u
  change Processor.run (IIR.processor (IIR.ofPolynomials _ _)) _ _ = _ at h
  rw [delay_numerator, delay_denominator] at h
  rw [h, run_eq]

end Transfer

/-- Geometric digital modes and exponential analog modes have equal gains. -/
theorem bilinear_modes (g z gain : ℂ) (hg : 1 + g ≠ 0) (hz : z + 1 ≠ 0)
    (hpole : (z - 1) + g * (z + 1) ≠ 0) :
    (discrete (g / (1 + g))).HasDigitalMode z gain ↔
      (prototype g).HasAnalogMode (bilinear z) gain := by
  rw [StateSpace.digital_hasTransfer _ _ _ (discrete_regular g z hg hpole),
    StateSpace.analog_hasTransfer _ _ _ (prototype_regular g _ (bilinear_regular g z hz hpole)),
    bilinear_transfer g z hg hz hpole]

end DSP.OnePoleTPTLowPass

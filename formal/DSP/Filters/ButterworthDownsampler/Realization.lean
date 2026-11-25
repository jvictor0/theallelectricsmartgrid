import DSP.Filters.ButterworthDownsampler.Transfer

noncomputable section

namespace DSP.ButterworthDownsampler

private theorem reference2_run (c : Coefficients ℝ) (input : Nat → Sample ℝ) (s : History ℝ) :
    (fun n => (Processor.run (reference2 c) input s n).x) =
      Processor.run (IIR.processor c.toIIR) (fun n => (input n).x) s := by
  have state (n : Nat) :
      Processor.runState (reference2 c) input s n =
        Processor.runState (IIR.processor c.toIIR) (fun n => (input n).x) s n := by
    induction n with
    | zero => rfl
    | succ n ih =>
      rw [Processor.runState_succ, Processor.runState_succ, ih]
      rfl
  funext n
  simp only [Processor.run, state]
  rfl

theorem section_realizes (c : Coefficients ℝ) : c.transfer.Realizes process2 (rest c) := by
  have save_zero : save c IIR.zeroState = rest c := by
    simp [save, rest, IIR.zeroState]
  intro input
  have run_eq := run2_eq c input IIR.zeroState
  rw [save_zero] at run_eq
  simp only [← run_eq, reference2_run]
  exact IIR.iir_hasTransfer c.toIIR (fun n => (input n).x)

private theorem cascade_run {S : Type} {p₁ p₂ : Processor S (Sample ℝ) (Sample ℝ)}
    {s₁ s₂ : S} (input : Nat → Sample ℝ) :
    Processor.run (Cascade.process p₁ p₂) input ⟨s₁, s₂⟩ =
      Processor.run p₂ (Processor.run p₁ input s₁) s₂ := by
  have state (n : Nat) :
      Processor.runState (Cascade.process p₁ p₂) input ⟨s₁, s₂⟩ n =
        ⟨Processor.runState p₁ input s₁ n,
          Processor.runState p₂ (Processor.run p₁ input s₁) s₂ n⟩ := by
    induction n with
    | zero => rfl
    | succ n ih =>
      simp only [Processor.runState_succ, ih]
      rfl
  funext n
  unfold Processor.run
  rw [state]
  rfl

theorem cascade_realizes {S : Type} {p₁ p₂ : Processor S (Sample ℝ) (Sample ℝ)} {s₁ s₂ : S}
    {h₁ h₂ : Transfer} (r₁ : h₁.Realizes p₁ s₁) (r₂ : h₂.Realizes p₂ s₂) :
    (h₁ * h₂).Realizes (Cascade.process p₁ p₂) ⟨s₁, s₂⟩ := by
  intro input
  let middle := Processor.run p₁ input s₁
  rw [cascade_run]
  change ((h₁.denominator * h₂.denominator : Polynomial ℝ) : PowerSeries ℝ) * _ =
    ((h₁.numerator * h₂.numerator : Polynomial ℝ) : PowerSeries ℝ) * _
  simp only [Polynomial.coe_mul]
  calc
    _ = (h₁.denominator : PowerSeries ℝ) *
        ((h₂.denominator : PowerSeries ℝ) *
          IIR.series (fun n => (Processor.run p₂ middle s₂ n).x)) := by rw [mul_assoc]
    _ = (h₁.denominator : PowerSeries ℝ) *
        ((h₂.numerator : PowerSeries ℝ) * IIR.series (fun n => (middle n).x)) := by rw [r₂ middle]
    _ = (h₂.numerator : PowerSeries ℝ) *
        ((h₁.denominator : PowerSeries ℝ) * IIR.series (fun n => (middle n).x)) := by ac_rfl
    _ = (h₂.numerator : PowerSeries ℝ) *
        ((h₁.numerator : PowerSeries ℝ) * IIR.series (fun n => (input n).x)) := by rw [r₁ input]
    _ = _ := by ac_rfl

theorem realizes2 (cutoff : ℝ) : (transfer2 cutoff).Realizes process2 (rest (tuned2 cutoff)) :=
  section_realizes _

theorem realizes4 (cutoff : ℝ) :
    (transfer4 cutoff).Realizes process4 ((tuned4 cutoff).map rest) :=
  cascade_realizes (section_realizes _) (section_realizes _)

theorem realizes8 (cutoff : ℝ) :
    (transfer8 cutoff).Realizes process8 ((tuned8 cutoff).map (Cascade.map rest)) :=
  cascade_realizes (cascade_realizes (section_realizes _) (section_realizes _))
    (cascade_realizes (section_realizes _) (section_realizes _))

end DSP.ButterworthDownsampler

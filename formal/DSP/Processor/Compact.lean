import DSP.Processor.Simulation

/-! Keep persistent memory while reusing a processor with a richer model state.
The existing simulation theorem transfers every sample of the output stream. -/
namespace DSP.Processor

variable {S M I O : Type}

def compact (p : Processor S I O) (save : S → M) (restore : M → S) : Processor M I O :=
  fun memory input =>
    let result := p (restore memory) input
    (save result.1, result.2)

theorem compact_simulation (p : Processor S I O) (save : S → M) (restore : M → S)
    (h : ∀ s x, p (restore (save s)) x = p s x) :
    Simulation p (compact p save restore) (fun s m => m = save s) := by
  intro s m hm x
  subst m
  simp [compact, h]

/-- Selecting observations does not change persistent state or advance twice. -/
def mapOutput {P : Type} (f : O → P) (p : Processor S I O) : Processor S I P :=
  fun state input => let result := p state input; (result.1, f result.2)

theorem Simulation.mapOutput {P : Type} {p : Processor S I O} {q : Processor M I O}
    {rel : S → M → Prop} (h : Simulation p q rel) (f : O → P) :
    Simulation (mapOutput f p) (mapOutput f q) rel := by
  intro s m hm x
  exact ⟨(h s m hm x).1, congrArg f (h s m hm x).2⟩

end DSP.Processor

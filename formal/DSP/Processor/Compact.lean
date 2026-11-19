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

end DSP.Processor

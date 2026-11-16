import Init

namespace DSP

/-- One call consumes an input and returns the next state and current output. -/
abbrev Processor (State Input Output : Type) := State → Input → State × Output

end DSP

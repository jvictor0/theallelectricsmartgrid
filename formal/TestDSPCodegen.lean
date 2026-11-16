import DSP.Codegen.Cpp
import DSP.Tests.CodegenFixtures

#generate_cpp DSP.Tests.swapProcessor as SwapProcessor to "build/lean/generated/SwapProcessor.hpp"
#generate_cpp DSP.Tests.gainProcessor as GainProcessor to "build/lean/generated/GainProcessor.hpp"

open Lean Elab Command in
run_cmd liftTermElabM do
  for (name, expected) in #[
      (``DSP.Tests.unsupportedField, "unsupported field type"),
      (``DSP.Tests.unsupportedExtern, "unsupported external implementation"),
      (``DSP.Tests.unsupportedLiteral, "unsupported external implementation"),
      (``DSP.Tests.unsupportedDecision, "unsupported external implementation"),
      (``DSP.Tests.unsupportedStaticCount, "unsupported external implementation"),
      (``DSP.Tests.unsupportedSize, "256-element export limit"),
      (``DSP.Tests.unsupportedUnsafe, "unsafe/partial definition"),
      (``DSP.Tests.wrongState, "returned state type differs"),
      (``DSP.Tests.wrongStateOnly, "returned state type differs")] do
    let mut rejected := false
    try
      let _ ← DSP.Codegen.generate name "TestProcessor"
    catch error =>
      let message ← error.toMessageData.toString
      unless (message.splitOn expected).length > 1 do
        throwError "unexpected rejection for {name}: {message}"
      rejected := true
    unless rejected do throwError "C++ codegen incorrectly accepted {name}"
  logInfo "C++ codegen rejection checks passed."

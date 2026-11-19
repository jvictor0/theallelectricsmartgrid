import Lake
open Lake DSL

package smartgrid where
  srcDir := "formal"
  buildDir := "build/lean/artifacts"
  packagesDir := "build/lean/packages"

require veil from git "https://github.com/verse-lab/veil.git" @ "e737e7b8fb4b61bdee837800ab6b97ee894e28c3"
require mathlib from git "https://github.com/leanprover-community/mathlib4.git" @ "v4.32.0"

@[default_target]
lean_lib DSP where
  globs := #[.one `DSP, .submodules `DSP]
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]

lean_exe dsp_filter_smoke where
  root := `DSP.Tests.Filters
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]

lean_exe dsp_codegen_reference where
  root := `DSP.Tests.CodegenReference
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]

lean_exe dsp_multiband_reference where
  root := `DSP.Tests.MultibandCodegen
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]

lean_exe dsp_delay_codegen_reference where
  root := `DSP.Tests.DelayCodegenReference
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]

@[default_target]
lean_lib Encoder where
  globs := #[.submodules `Encoder]
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]

@[default_target]
lean_lib PhaseTime where
  globs := #[.submodules `PhaseTime]
  leanOptions := #[⟨`autoImplicit, false⟩, ⟨`warningAsError, true⟩]

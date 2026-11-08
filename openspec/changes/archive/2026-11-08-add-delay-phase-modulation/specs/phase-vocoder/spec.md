## MODIFIED Requirements

### Requirement: Grain Launch and Dual-Frame Analysis
The system SHALL launch a new analysis/synthesis grain every analysis hop of H = N/4 host samples, where N = 4096 is the grain size (`Resynthesizer::x_bits = 12`, `x_hopDenom = 4`, so H = 1024 host samples, ~21.3 ms). `GrainManager::Process` runs every engine sample, so the launch interval is `Resynthesizer::GetSynthesisHopSamples()` = H * `x_oversample` engine samples. Each launch captures two Hann-windowed frames of host-rate audio from the buffer: the current frame at the warped-time read position and the previous frame H host samples earlier (`GrainManager::StartGrain`). The previous frame primes `m_prevAnalysisPhase`/`m_prevAnalysisMagnitudes`; the current frame is DFT-transformed for analysis. The synthesized spectrum SHALL be inverse-transformed into an N * `x_oversample`-sample engine-rate `SynthesisFrame` covering the same 85.333 ms, Hann-windowed (`Resynthesizer::Synthesize`), and overlap-added into one preallocated `OLAGeneric` ring read once per engine sample and divided by 1.5, giving 75% overlap-add reconstruction. Quad Delay SHALL use a 6-hop ring with one synthesis hop of nominal output latency and per-channel cubic-interpolated PM reads; sample playback SHALL retain its existing output timing and storage. Frame sizes and launch cadence SHALL remain unchanged.

#### Scenario: Grains overlap at 75%
- **WHEN** the quad delay runs steadily
- **THEN** `GrainManager::Process` launches one grain every 1024 * `x_oversample` engine samples (`Resynthesizer::GetSynthesisHopSamples()`)
- **AND** at any instant four overlapping 85.333 ms Hann-windowed frames sum into the delay output

#### Scenario: Analysis frames are separated by exactly one hop
- **WHEN** a grain launches at warped read position `startTime`
- **THEN** the current analysis frame reads samples `startTime .. startTime + 4095` and the previous frame reads `startTime - 1024 .. startTime + 3071`, both multiplied by `Math4096::Hann`

### Requirement: Three-Oscillator Unison Synthesis
The system SHALL synthesize each grain through exactly three oscillators (`x_numOscillators = 3`) with detune factors 1.0, `m_unisonDetune`, and `1/m_unisonDetune`, each holding independent synthesis phases. Gains are energy-normalized from the unison gain u: the center oscillator gets `sqrt(1 - 2u²/3)` and each side oscillator gets `u/sqrt(3)` (`Input::GetUnisonGainForOscillator`), keeping total energy constant as spread increases. `ResynthesizerGeneric::GetUnisonBase()` SHALL return a vector filled with 1.03 for channel counts other than four; for four channels it SHALL return `2^(C/1200)` with C = 40, 45, 50, and 55 cents in channel order. Quad Delay SHALL use these bases for its exponential unison detune mapping.

#### Scenario: Unison off leaves only the center voice
- **WHEN** `m_unisonGain` = 0
- **THEN** the center oscillator's gain is 1.0 and both detuned oscillators' gains are 0, so the grain is synthesized by the center voice alone

#### Scenario: Full unison weights all three voices equally
- **WHEN** `m_unisonGain` = 1
- **THEN** center gain is sqrt(1/3) ≈ 0.577 and each side gain is 1/sqrt(3) ≈ 0.577, so the three detuned copies carry equal energy and the summed energy matches the unison-off case

#### Scenario: Generic unison base remains available
- **WHEN** a mono or three-channel client requests its unison base
- **THEN** every channel receives 1.03

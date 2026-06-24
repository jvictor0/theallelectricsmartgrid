# Add Spectral Residual Model Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a residual spectral envelope to Partial Machine analysis/synthesis so non-atom DFT energy is smoothed, synthesized into the quad DFT, and shaped by reduction feedback.

**Architecture:** `SpectralModelGeneric` owns residual analysis state and one residual magnitude per DFT component. `PartialMachine` owns quad residual synthesis, reading residual bucket `k`, applying existing frequency-dependent reduction/pan helpers, adding random-phase complex energy to quad DFT component `k`, and writing reduction feedback back to residual bucket `k`.

**Tech Stack:** C++17 headers in `private/src`, doctest unit tests in `private/test`, OpenSpec artifacts under `openspec/changes/add-spectral-residual-model`.

---

## File Structure

- Modify `private/src/AdaptiveWaveTable.hpp`: add small DFT helpers for reading interpolated phase if needed and adding direct complex values to DFT components.
- Modify `private/src/OLA.hpp`: expose a `QuadDFT::AddComponent(size_t, std::complex<float>, QuadFloat)` helper for residual direct component writes.
- Modify `private/src/SpectralModel.hpp`: add `AnalysisAtom::m_analysisPhase`, add `ResidualModel`, add residual extraction and processing path.
- Modify `private/src/PartialMachine.hpp`: add `ResidualMachine`, call residual-aware extraction, integrate residual synthesis into `ProcessSynthesisFrame`.
- Modify `private/test/unit/dsp_spectralmodel.cpp`: add focused residual model and residual analysis tests.
- Modify `private/test/unit/dsp_partialmachine.cpp`: add residual synthesis, feedback, and finite output tests.
- Modify `openspec/changes/add-spectral-residual-model/tasks.md`: mark OpenSpec checkboxes complete only after matching work and verification are done.

## Task 1: Spectral Residual State

**Files:**
- Modify: `private/src/SpectralModel.hpp`
- Test: `private/test/unit/dsp_spectralmodel.cpp`

- [ ] **Step 1: Add failing residual model tests**

Add tests that instantiate `SpectralModel::ResidualModel`, set scalar attack/decay alpha, process synthetic residual inputs, and check exact DFT-index envelope lookup:

```cpp
DOCTEST_TEST_CASE("SpectralModel residual model slews and queries by DFT bucket")
{
    SpectralModel::ResidualModel model;
    SpectralModel::Input spectralInput;
    SpectralModel::ResidualModel::Input residualInput;

    spectralInput.m_slewUpAlpha = ScalarParameter::Parameter(0.5f);
    spectralInput.m_slewDownAlpha = ScalarParameter::Parameter(0.25f);

    constexpr size_t k = 12;
    residualInput.m_analysisResidualMagnitudes[k] = 1.0f;
    model.Process(spectralInput, residualInput);

    DOCTEST_CHECK(model.GetEnvelope(k) == doctest::Approx(0.5f));

    residualInput.m_analysisResidualMagnitudes[k] = 0.0f;
    model.Process(spectralInput, residualInput);

    DOCTEST_CHECK(model.GetEnvelope(k) == doctest::Approx(0.375f));
}
```

- [ ] **Step 2: Run the focused test and confirm it fails**

Run:

```bash
cmake --build private/test/build --target smartgrid_tests
private/test/build/smartgrid_tests --test-case="SpectralModel residual model slews and queries by DFT bucket"
```

Expected: compile failure because `ResidualModel` does not exist.

- [ ] **Step 3: Implement `ResidualModel`**

In `SpectralModelGeneric`, add:

```cpp
struct ResidualModel
{
    static constexpr size_t x_numBuckets = x_maxComponents;

    struct Input
    {
        float m_analysisResidualMagnitudes[x_numBuckets];

        Input()
            : m_analysisResidualMagnitudes{}
        {
        }
    };

    float m_magnitudes[x_numBuckets];
    float m_frequencies[x_numBuckets];
    float m_exponentialFrequencies[x_numBuckets];

    ResidualModel()
        : m_magnitudes{}
        , m_frequencies{}
        , m_exponentialFrequencies{}
    {
        for (size_t i = 0; i < x_numBuckets; ++i)
        {
            m_frequencies[i] = static_cast<float>(i) / static_cast<float>(x_tableSize);
            m_exponentialFrequencies[i] = m_frequencies[i];
        }
    }

    float GetEnvelope(size_t bucketIndex) const
    {
        return m_magnitudes[bucketIndex];
    }

    void Process(Input& residualInput, SpectralModelGeneric::Input& spectralInput)
    {
        for (size_t i = 0; i < x_numBuckets; ++i)
        {
            ParameterIndex index = ParameterProvider::GetIndexForFrequency(m_exponentialFrequencies[i], spectralInput.m_parameterInput);
            float slewUpAlpha = spectralInput.m_slewUpAlpha.Process(index);
            float slewDownAlpha = spectralInput.m_slewDownAlpha.Process(index);
            m_magnitudes[i] = BiDirectionalSlew::Process(
                m_magnitudes[i],
                residualInput.m_analysisResidualMagnitudes[i],
                slewUpAlpha,
                slewDownAlpha);
        }
    }
};
```

Then add `ResidualModel m_residualModel;` beside `m_atoms`.

- [ ] **Step 4: Run focused residual model test**

Run:

```bash
cmake --build private/test/build --target smartgrid_tests
private/test/build/smartgrid_tests --test-case="SpectralModel residual model slews and queries by DFT bucket"
```

Expected: PASS.

## Task 2: Residual Analysis Extraction

**Files:**
- Modify: `private/src/SpectralModel.hpp`
- Test: `private/test/unit/dsp_spectralmodel.cpp`

- [ ] **Step 1: Add failing tests for analysis phase and residual extraction**

Add tests that transform a sine buffer, call the residual-aware extraction path, and assert residual buckets are populated by remaining DFT magnitudes:

```cpp
DOCTEST_TEST_CASE("SpectralModel residual extraction stores analysis phase")
{
    SpectralModel model;
    SpectralModel::Input input;
    SpectralModel::Buffer buffer;
    SpectralModel::ResidualModel::Input residualInput;

    input.m_gainThreshold = 1e-4f;
    input.m_numAtoms = 8;
    input.m_slewUpAlpha = ScalarParameter::Parameter(1.0f);
    input.m_slewDownAlpha = ScalarParameter::Parameter(1.0f);
    input.m_omegaPortamentoAlpha = ScalarParameter::Parameter(1.0f);

    constexpr size_t kBin = 32;
    for (size_t i = 0; i < SpectralModel::x_tableSize; ++i)
    {
        float phase = static_cast<float>(kBin * i) / static_cast<float>(SpectralModel::x_tableSize);
        buffer.m_table[i] = Math4096::Sin2pi(phase);
    }

    model.ExtractAtomsAndResidual(buffer, input, residualInput);

    DOCTEST_REQUIRE(model.m_atoms.Size() > 0);
    DOCTEST_CHECK(std::abs(model.m_atoms[0]->m_analysisPhase) <= 1.0f);
}
```

- [ ] **Step 2: Run and confirm failure**

Run:

```bash
cmake --build private/test/build --target smartgrid_tests
private/test/build/smartgrid_tests --test-case="SpectralModel residual extraction stores analysis phase"
```

Expected: compile failure because `ExtractAtomsAndResidual` and `m_analysisPhase` do not exist.

- [ ] **Step 3: Add phase to `AnalysisAtom`**

Add `float m_analysisPhase;`, initialize it to `0.0f`, and update constructors to accept `analysisPhase`. Update all existing call sites and test helpers to pass `0.0f` for manually-created atoms. In `Atom::Merge`, copy `m_analysisPhase` from the matched analysis atom.

- [ ] **Step 4: Fill analysis phase during peak extraction**

When extracting peak `k + p`, use the nearest valid DFT component for phase:

```cpp
int phaseBin = std::max(1, std::min(static_cast<int>(x_maxComponents) - 1, static_cast<int>(std::round(static_cast<float>(k) + p))));
float peakPhase = std::arg(dft.m_components[phaseBin]) / (2.0f * static_cast<float>(M_PI));
analysisAtoms.Add(AnalysisAtom(peakOmega, peakMag, peakPhase, index, false));
```

- [ ] **Step 5: Add residual-aware extraction**

Add `ExtractAtomsAndResidual(Buffer& buffer, Input& input, typename ResidualModel::Input& residualInput)` that:

1. Transforms the buffer into a DFT.
2. Extracts analysis atoms.
3. Cancels non-synthetic analysis atoms from that DFT by writing `analysisPhase + 0.5f` with `analysisMagnitude`.
4. Copies `std::abs(dft.m_components[i])` into `residualInput.m_analysisResidualMagnitudes[i]`.
5. Calls the existing tracking merge/birth/death logic.
6. Calls `m_residualModel.Process(residualInput, input)`.

Keep `ExtractAtoms(Buffer&, Input&)` as a wrapper that only tracks atoms for existing callers.

- [ ] **Step 6: Run focused spectral tests**

Run:

```bash
cmake --build private/test/build --target smartgrid_tests
private/test/build/smartgrid_tests --test-case="SpectralModel*"
```

Expected: PASS.

## Task 3: Partial Machine Residual Synthesis

**Files:**
- Modify: `private/src/OLA.hpp`
- Modify: `private/src/PartialMachine.hpp`
- Test: `private/test/unit/dsp_partialmachine.cpp`

- [ ] **Step 1: Add failing residual synthesis tests**

Add tests that directly seed `pm.m_spectralModel.m_residualModel.m_magnitudes[k]`, call `ProcessSynthesisFrame`, and assert finite output and feedback writeback:

```cpp
DOCTEST_TEST_CASE("PartialMachine residual feedback writes reduced magnitude back")
{
    GlobalEnv::ResetPerTest();

    PartialMachine pm;
    PartialMachine::Input input = MakeBasicInput();
    input.m_synthesisContextInput.m_bwBaseFrequency = FrequencyDependentParameter::Parameter(0.25f);
    input.m_synthesisContextInput.m_bwWidth = FrequencyDependentParameter::Parameter(1.0f);
    input.m_synthesisContextInput.m_reductionFeedback = FrequencyDependentParameter::Parameter(1.0f);

    constexpr size_t k = 8;
    pm.m_spectralModel.m_residualModel.m_magnitudes[k] = 1.0f;
    pm.ProcessSynthesisFrame(input);

    DOCTEST_CHECK(pm.m_spectralModel.m_residualModel.m_magnitudes[k] < 1.0f);
    DOCTEST_CHECK(pm.m_spectralModel.m_residualModel.m_magnitudes[k] >= PartialMachine::SpectralModel::x_deathMag);
}
```

- [ ] **Step 2: Add direct DFT component helper**

In `QuadDFT`, add:

```cpp
void AddComponent(size_t componentIndex, std::complex<float> value, QuadFloat distribution)
{
    if (componentIndex == 0 || OLA::x_maxComponents <= componentIndex)
    {
        return;
    }

    for (int i = 0; i < 4; ++i)
    {
        m_dfts[i].m_components[componentIndex] += value * distribution[i];
    }
}
```

- [ ] **Step 3: Add `ResidualMachine`**

Inside `PartialMachine`, add a `ResidualMachine` struct with `RGen m_gen;` and:

```cpp
void Process(QuadDFT& dft, SpectralModel& spectralModel, Input& input)
{
    for (size_t k = 1; k < SpectralModel::ResidualModel::x_numBuckets; ++k)
    {
        float frequency = spectralModel.m_residualModel.m_frequencies[k];
        Index index = FrequencyDependentParameter::GetIndexForFrequency(frequency, input.m_spectralModelInput.m_parameterInput);
        float envelope = spectralModel.m_residualModel.GetEnvelope(k);
        float reduction = SynthesisContext::GetReduction(frequency, index, input.m_synthesisContextInput);
        float reducedMagnitude = envelope * reduction;
        float radius = SynthesisContext::GetRadius(frequency, index, input.m_synthesisContextInput);
        float azimuth = SynthesisContext::GetAzimuth(frequency, index, input.m_synthesisContextInput);
        QuadFloat distribution = SynthesisContext::Pan(azimuth, radius);
        float phase = m_gen.UniGen();
        std::complex<float> value(reducedMagnitude * Math::Cos2pi(phase), reducedMagnitude * Math::Sin2pi(phase));
        dft.AddComponent(k, value, distribution);
        spectralModel.m_residualModel.m_magnitudes[k] = PhaseUtils::ExpParam::Compute(
            envelope,
            std::max(SpectralModel::x_deathMag, reducedMagnitude),
            input.m_synthesisContextInput.m_reductionFeedback.ProcessLinear(index));
    }
}
```

- [ ] **Step 4: Integrate synthesis**

Add `ResidualMachine m_residualMachine;` as a `PartialMachine` member. In `ProcessSynthesisFrame`, after atom processing and before `m_ola.Write`, call:

```cpp
m_residualMachine.Process(synthesisContext.m_dft, m_spectralModel, input);
```

In `Process`, replace `m_spectralModel.ExtractAtoms(buffer, input.m_spectralModelInput);` with residual-aware extraction using a local `SpectralModel::ResidualModel::Input residualInput;`.

- [ ] **Step 5: Run focused Partial Machine tests**

Run:

```bash
cmake --build private/test/build --target smartgrid_tests
private/test/build/smartgrid_tests --test-case="PartialMachine*"
```

Expected: PASS.

## Task 4: Verification and OpenSpec Sync

**Files:**
- Modify: `openspec/changes/add-spectral-residual-model/tasks.md`

- [ ] **Step 1: Run focused tests**

Run:

```bash
cmake --build private/test/build --target smartgrid_tests
private/test/build/smartgrid_tests --test-case="SpectralModel*"
private/test/build/smartgrid_tests --test-case="PartialMachine*"
```

Expected: all focused tests pass.

- [ ] **Step 2: Run full test binary**

Run:

```bash
private/test/build/smartgrid_tests
```

Expected: all tests pass.

- [ ] **Step 3: Mark OpenSpec tasks complete**

Update every checkbox in `openspec/changes/add-spectral-residual-model/tasks.md` from `- [ ]` to `- [x]` only after implementation and tests pass.

- [ ] **Step 4: Check OpenSpec status**

Run:

```bash
openspec status --change "add-spectral-residual-model"
```

Expected: change artifacts complete and implementation tasks checked off.

- [ ] **Step 5: Final review**

Review `git diff` for accidental unrelated changes. Do not touch unrelated untracked `private/src/ExternalClockSync.hpp`.

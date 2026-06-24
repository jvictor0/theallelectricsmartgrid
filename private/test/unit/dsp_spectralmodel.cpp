#include "doctest.h"

#include "SpectralModel.hpp"

namespace
{

using Model = SpectralModel;
using Input = Model::Input;
using AnalysisAtom = Model::AnalysisAtom;
using AnalysisAtomArray = Model::AnalysisAtomArray;
using Atom = Model::Atom;

Input MakeInput(float density)
{
    Input input;
    input.m_slewUpAlpha = ScalarParameter::Parameter(1.0f);
    input.m_slewDownAlpha = ScalarParameter::Parameter(1.0f);
    input.m_omegaPortamentoAlpha = ScalarParameter::Parameter(1.0f);
    input.m_omegaDensity = ScalarParameter::Parameter(density);
    return input;
}

Atom MakeAtom(float omega)
{
    return Atom(omega, 1.0f, ScalarParameter::Index(), omega, 1.0f, 0.0f);
}

void AddAnalysisAtom(AnalysisAtomArray& atoms, float omega, float magnitude, bool isSynthetic = false)
{
    atoms.Add(AnalysisAtom(omega, magnitude, 0.0f, ScalarParameter::Index(), isSynthetic));
}

}  // namespace

DOCTEST_TEST_CASE("SpectralModel tracking theta prefers nearby continuation over slightly louder farther peak")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.101f, 0.90f);
    AddAnalysisAtom(atoms, 0.108f, 1.00f);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.101f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(0.90f));
}

DOCTEST_TEST_CASE("SpectralModel tracking theta still allows much stronger farther peak")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.101f, 0.90f);
    AddAnalysisAtom(atoms, 0.108f, 10.0f);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.108f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(10.0f));
}

DOCTEST_TEST_CASE("SpectralModel tracking keeps organic priority before theta")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.101f, 0.50f, true);
    AddAnalysisAtom(atoms, 0.108f, 0.10f, false);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.108f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(0.10f));
    DOCTEST_CHECK(atom.m_isSynthetic == false);
}

DOCTEST_TEST_CASE("SpectralModel tracking ignores first lower_bound candidate outside density window")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.12f, 1.0f);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.10f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(0.0f));
    DOCTEST_CHECK(atom.m_synthesisMagnitude == doctest::Approx(0.0f));
}

DOCTEST_TEST_CASE("SpectralModel residual model slews and queries by DFT bucket")
{
    Model::ResidualModel model;
    Input spectralInput;
    Model::ResidualModel::Input residualInput;

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

DOCTEST_TEST_CASE("SpectralModel residual model uses scalar smoothing for equal frequency lanes")
{
    using FrequencyModel = SpectralModelGeneric<12, FrequencyDependentParameter>;

    FrequencyModel::ResidualModel model;
    FrequencyModel::Input spectralInput;
    FrequencyModel::ResidualModel::Input residualInput;

    spectralInput.m_slewUpAlpha = FrequencyDependentParameter::Parameter(0.5f);
    spectralInput.m_slewDownAlpha = FrequencyDependentParameter::Parameter(0.25f);
    spectralInput.m_parameterInput.m_linearFreqs = FrequencyDependentParameter::Parameter(1.0f);

    constexpr size_t kLow = 12;
    constexpr size_t kHigh = 200;
    residualInput.m_analysisResidualMagnitudes[kLow] = 1.0f;
    residualInput.m_analysisResidualMagnitudes[kHigh] = 1.0f;
    model.Process(spectralInput, residualInput);

    DOCTEST_CHECK(model.GetEnvelope(kLow) == doctest::Approx(0.5f));
    DOCTEST_CHECK(model.GetEnvelope(kHigh) == doctest::Approx(0.5f));

    residualInput.m_analysisResidualMagnitudes[kLow] = 0.0f;
    residualInput.m_analysisResidualMagnitudes[kHigh] = 0.0f;
    model.Process(spectralInput, residualInput);

    DOCTEST_CHECK(model.GetEnvelope(kLow) == doctest::Approx(0.375f));
    DOCTEST_CHECK(model.GetEnvelope(kHigh) == doctest::Approx(0.375f));
}

DOCTEST_TEST_CASE("SpectralModel residual model caches log frequency indexes")
{
    using FrequencyModel = SpectralModelGeneric<12, FrequencyDependentParameter>;

    FrequencyModel::ResidualModel model;
    FrequencyDependentParameter::Input parameterInput;
    parameterInput.m_linearFreqs = FrequencyDependentParameter::Parameter(1.0f);

    constexpr size_t k = 12;
    float frequency = static_cast<float>(k) / static_cast<float>(FrequencyModel::x_tableSize);
    FrequencyDependentParameter::Index frequencyIndex = FrequencyDependentParameter::GetIndexForFrequency(frequency, parameterInput);
    FrequencyDependentParameter::Index logFrequencyIndex = FrequencyDependentParameter::GetIndexForLogFrequency(model.m_logFrequencies[k], parameterInput);

    DOCTEST_CHECK(model.m_logFrequencies[0] == doctest::Approx(model.m_logFrequencies[1]));
    DOCTEST_CHECK(model.m_logFrequencies[k] == doctest::Approx(FrequencyDependentParameter::FrequencyToLinear(frequency)));
    DOCTEST_CHECK(logFrequencyIndex.m_index == frequencyIndex.m_index);
    DOCTEST_CHECK(logFrequencyIndex.m_interp == doctest::Approx(frequencyIndex.m_interp));
}

DOCTEST_TEST_CASE("SpectralModel residual extraction stores analysis phase")
{
    Model model;
    Input input;
    Model::Buffer buffer;

    input.m_gainThreshold = 1e-4f;
    input.m_numAtoms = 8;
    input.m_slewUpAlpha = ScalarParameter::Parameter(1.0f);
    input.m_slewDownAlpha = ScalarParameter::Parameter(1.0f);
    input.m_omegaPortamentoAlpha = ScalarParameter::Parameter(1.0f);

    constexpr size_t x_bin = 32;
    for (size_t i = 0; i < Model::x_tableSize; ++i)
    {
        float phase = static_cast<float>(x_bin * i) / static_cast<float>(Model::x_tableSize);
        buffer.m_table[i] = Math4096::Sin2pi(phase);
    }

    model.ExtractAtomsAndResidual(buffer, input);

    DOCTEST_REQUIRE(model.m_atoms.Size() > 0);
    DOCTEST_CHECK(std::abs(model.m_atoms[0]->m_analysisPhase) <= 1.0f);
}

DOCTEST_TEST_CASE("SpectralModel residual extraction lowers cancelled sinusoid bin")
{
    Model model;
    Input input;
    Model::Buffer buffer;

    input.m_gainThreshold = 1e-4f;
    input.m_numAtoms = 8;
    input.m_slewUpAlpha = ScalarParameter::Parameter(1.0f);
    input.m_slewDownAlpha = ScalarParameter::Parameter(1.0f);
    input.m_omegaPortamentoAlpha = ScalarParameter::Parameter(1.0f);

    constexpr size_t x_bin = 32;
    for (size_t i = 0; i < Model::x_tableSize; ++i)
    {
        float phase = static_cast<float>(x_bin * i) / static_cast<float>(Model::x_tableSize);
        buffer.m_table[i] = Math4096::Sin2pi(phase);
    }

    Model::DFT rawDft;
    rawDft.Transform(buffer);
    float rawMagnitude = std::abs(rawDft.m_components[x_bin]);

    model.ExtractAtomsAndResidual(buffer, input);

    DOCTEST_CHECK(model.m_residualModel.GetEnvelope(x_bin) < rawMagnitude);
}

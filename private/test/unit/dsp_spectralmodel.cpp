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

DOCTEST_TEST_CASE("SpectralModel analysis reports sinusoid phase at the start of the frame")
{
    for (float bin : {32.0f, 32.125f, 32.25f, 32.499f, 32.5f, 32.501f, 32.75f, 32.875f})
    {
        for (float phase : {0.0f, 0.125f, 0.37f, 0.9f})
        {
            Model model;
            Input input;
            input.m_gainThreshold = 1e-4f;
            input.m_numAtoms = 8;
            Model::Buffer buffer;
            for (size_t sample = 0; sample < Model::x_tableSize; ++sample)
            {
                double angle = 2.0 * M_PI * (bin * sample / Model::x_tableSize + phase);
                buffer.m_table[sample] = 0.1f * std::cos(angle) * Math4096::Hann(sample);
            }

            Model::DFT spectrum;
            spectrum.Transform(buffer);
            AnalysisAtomArray atoms;
            model.ExtractAnalysisAtoms(spectrum, atoms, input);
            DOCTEST_REQUIRE(atoms.Size() == 1);
            DOCTEST_CAPTURE(bin);
            DOCTEST_CAPTURE(phase);
            double phaseError = std::remainder(atoms[0].m_analysisPhase - phase, 1.0);
            double tolerance = bin == 32.0f ? 1e-5 : 0.01;
            DOCTEST_CHECK(std::abs(phaseError) < tolerance);

            Model::DFT residual = spectrum;
            residual.WriteWindowedPartial(atoms[0].m_analysisPhase + 0.5f,
                2.0f * atoms[0].m_analysisMagnitude, atoms[0].m_analysisOmega);
            double originalEnergy = 0;
            double residualEnergy = 0;
            for (size_t k = 1; k < Model::x_maxComponents; ++k)
            {
                originalEnergy += std::norm(spectrum.m_components[k]);
                residualEnergy += std::norm(residual.m_components[k]);
            }

            double maxFraction = bin == 32.0f ? 1e-7 : 0.002;
            DOCTEST_CHECK(residualEnergy < maxFraction * originalEnergy);
        }
    }
}

DOCTEST_TEST_CASE("SpectralModel analysis magnitude retains Hann peak normalization")
{
    for (float bin : {32.0f, 32.25f, 32.5f, 32.75f})
    {
        for (float amplitude : {0.01f, 0.1f})
        {
            Model model;
            Input input;
            input.m_gainThreshold = 1e-4f;
            Model::Buffer buffer;
            for (size_t sample = 0; sample < Model::x_tableSize; ++sample)
            {
                double angle = 2.0 * M_PI * (bin * sample / Model::x_tableSize + 0.37);
                buffer.m_table[sample] = amplitude * std::cos(angle) * Math4096::Hann(sample);
            }

            Model::DFT spectrum;
            spectrum.Transform(buffer);
            AnalysisAtomArray atoms;
            model.ExtractAnalysisAtoms(spectrum, atoms, input);
            DOCTEST_REQUIRE(atoms.Size() == 1);
            DOCTEST_CAPTURE(bin);
            DOCTEST_CAPTURE(amplitude);
            DOCTEST_CHECK(std::abs(atoms[0].m_analysisMagnitude - amplitude / 4.0f) < amplitude / 4.0f * 0.005f);
        }
    }
}

DOCTEST_TEST_CASE("SpectralModel analyzed partials reconstruct the residual for overlapping tones")
{
    for (size_t numAtoms : {1, 8})
    {
        for (float spacing : {1.0f, 1.5f, 2.0f, 4.0f})
        {
            for (int phaseIndex = 0; phaseIndex < 32; ++phaseIndex)
            {
                Model model;
                Input input;
                input.m_gainThreshold = 1e-4f;
                input.m_numAtoms = numAtoms;
                Model::Buffer buffer;
                for (size_t sample = 0; sample < Model::x_tableSize; ++sample)
                {
                    double firstAngle = 2.0 * M_PI * 32.0 * sample / Model::x_tableSize;
                    double secondAngle = 2.0 * M_PI * ((32.0 + spacing) * sample / Model::x_tableSize + phaseIndex / 32.0);
                    buffer.m_table[sample] = 0.1f * (std::cos(firstAngle) + std::cos(secondAngle)) * Math4096::Hann(sample);
                }

                Model::DFT spectrum;
                spectrum.Transform(buffer);
                AnalysisAtomArray atoms;
                model.ExtractAnalysisAtoms(spectrum, atoms, input);
                DOCTEST_REQUIRE(!atoms.Empty());
                Model::DFT reconstructedResidual = spectrum;
                for (const AnalysisAtom& atom : atoms)
                {
                    reconstructedResidual.WriteWindowedPartial(atom.m_analysisPhase + 0.5f,
                        2.0f * atom.m_analysisMagnitude, atom.m_analysisOmega);
                }

                model.ExtractAtomsAndResidual(buffer, input);
                double originalEnergy = 0.0;
                double residualEnergy = 0.0;
                double differenceEnergy = 0.0;
                for (size_t k = 1; k < Model::x_maxComponents; ++k)
                {
                    originalEnergy += std::norm(spectrum.m_components[k]);
                    residualEnergy += std::norm(reconstructedResidual.m_components[k]);
                    double difference = std::abs(reconstructedResidual.m_components[k]) - model.m_residualModel.GetEnvelope(k);
                    differenceEnergy += difference * difference;
                }

                DOCTEST_CAPTURE(numAtoms);
                DOCTEST_CAPTURE(spacing);
                DOCTEST_CAPTURE(phaseIndex);
                DOCTEST_CHECK(residualEnergy <= originalEnergy * (1.0 + 1e-6));
                DOCTEST_CHECK(differenceEnergy <= originalEnergy * 1e-10);
            }
        }
    }
}

DOCTEST_TEST_CASE("SpectralModel residual extraction cancels Hann-windowed tones")
{
    for (float bin : {32.0f, 32.25f, 32.5f, 32.75f})
    {
        for (float phase : {0.0f, 0.125f, 0.37f})
        {
            Model model;
            Input input;
            input.m_gainThreshold = 1e-4f;
            input.m_numAtoms = 8;
            Model::Buffer buffer;
            for (size_t sample = 0; sample < Model::x_tableSize; ++sample)
            {
                double angle = 2.0 * M_PI * (bin * sample / Model::x_tableSize + phase);
                buffer.m_table[sample] = 0.1f * std::cos(angle) * Math4096::Hann(sample);
            }

            Model::DFT raw;
            raw.Transform(buffer);
            model.ExtractAtomsAndResidual(buffer, input);
            DOCTEST_REQUIRE(model.m_atoms.Size() == 1);
            double originalEnergy = 0;
            double residualEnergy = 0;
            for (size_t k = 1; k < Model::x_maxComponents; ++k)
            {
                originalEnergy += std::norm(raw.m_components[k]);
                double magnitude = model.m_residualModel.GetEnvelope(k);
                residualEnergy += magnitude * magnitude;
            }

            DOCTEST_CAPTURE(bin);
            DOCTEST_CAPTURE(phase);
            double maxFraction = bin == 32.0f ? 1e-7 : 0.002;
            DOCTEST_CHECK(residualEnergy < maxFraction * originalEnergy);
        }
    }
}

DOCTEST_TEST_CASE("SpectralModel residual subtraction never adds energy to nearby tones")
{
    for (float spacing : {1.0f, 1.5f, 2.0f, 4.0f})
    {
        for (int phaseIndex = 0; phaseIndex < 32; ++phaseIndex)
        {
            Model model;
            Input input;
            input.m_gainThreshold = 1e-4f;
            input.m_numAtoms = 8;
            Model::Buffer buffer;
            for (size_t sample = 0; sample < Model::x_tableSize; ++sample)
            {
                double firstAngle = 2.0 * M_PI * 32.0 * sample / Model::x_tableSize;
                double secondAngle = 2.0 * M_PI * ((32.0 + spacing) * sample / Model::x_tableSize + phaseIndex / 32.0);
                buffer.m_table[sample] = 0.1f * (std::cos(firstAngle) + std::cos(secondAngle)) * Math4096::Hann(sample);
            }

            Model::DFT raw;
            raw.Transform(buffer);
            model.ExtractAtomsAndResidual(buffer, input);
            double originalEnergy = 0;
            double residualEnergy = 0;
            for (size_t bin = 1; bin < Model::x_maxComponents; ++bin)
            {
                originalEnergy += std::norm(raw.m_components[bin]);
                double magnitude = model.m_residualModel.GetEnvelope(bin);
                residualEnergy += magnitude * magnitude;
            }

            DOCTEST_CAPTURE(spacing);
            DOCTEST_CAPTURE(phaseIndex);
            DOCTEST_CHECK(residualEnergy <= originalEnergy * (1.0 + 1e-6));
        }
    }
}

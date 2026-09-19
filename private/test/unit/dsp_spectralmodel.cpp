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

float AtOctaveDistance(float omega, float distance)
{
    return omega * std::exp2f(distance);
}

void AddAnalysisAtom(AnalysisAtomArray& atoms, float omega, float magnitude)
{
    atoms.Add(AnalysisAtom(omega, magnitude, 0.0f, ScalarParameter::Index()));
}

}  // namespace

DOCTEST_TEST_CASE("SpectralModel tracking theta prefers nearby continuation over slightly louder farther peak")
{
    Model model;
    Input input = MakeInput(0.1f);
    model.m_atoms.Add(MakeAtom(0.10f));
    Atom* atom = model.m_atoms[0];
    AnalysisAtomArray atoms;
    float nearbyOmega = AtOctaveDistance(0.10f, 0.01f);
    float fartherOmega = AtOctaveDistance(0.10f, 0.08f);

    AddAnalysisAtom(atoms, nearbyOmega, 0.90f);
    AddAnalysisAtom(atoms, fartherOmega, 1.00f);

    model.TrackAnalysisAtoms(atoms, input);

    DOCTEST_REQUIRE(model.IsAtomAllocated(atom));
    DOCTEST_CHECK(atom->m_analysisOmega == doctest::Approx(nearbyOmega));
    DOCTEST_CHECK(atom->m_analysisMagnitude == doctest::Approx(0.90f));
}

DOCTEST_TEST_CASE("SpectralModel tracking theta still allows much stronger farther peak")
{
    Model model;
    Input input = MakeInput(0.1f);
    model.m_atoms.Add(MakeAtom(0.10f));
    Atom* atom = model.m_atoms[0];
    AnalysisAtomArray atoms;
    float nearbyOmega = AtOctaveDistance(0.10f, 0.01f);
    float fartherOmega = AtOctaveDistance(0.10f, 0.08f);

    AddAnalysisAtom(atoms, nearbyOmega, 0.90f);
    AddAnalysisAtom(atoms, fartherOmega, 10.0f);

    model.TrackAnalysisAtoms(atoms, input);

    DOCTEST_REQUIRE(model.IsAtomAllocated(atom));
    DOCTEST_CHECK(atom->m_analysisOmega == doctest::Approx(fartherOmega));
    DOCTEST_CHECK(atom->m_analysisMagnitude == doctest::Approx(10.0f));
}

DOCTEST_TEST_CASE("SpectralModel tracking decays old atoms and births peaks outside their density window")
{
    Model model;
    Input input = MakeInput(0.1f);
    model.m_atoms.Add(MakeAtom(0.10f));
    Atom* atom = model.m_atoms[0];
    AnalysisAtomArray atoms;

    float newOmega = AtOctaveDistance(0.10f, 0.11f);
    AddAnalysisAtom(atoms, newOmega, 1.0f);

    model.TrackAnalysisAtoms(atoms, input);

    DOCTEST_CHECK_FALSE(model.IsAtomAllocated(atom));
    DOCTEST_REQUIRE(model.m_atoms.Size() == 1);
    DOCTEST_CHECK(model.m_atoms[0]->m_analysisOmega == doctest::Approx(newOmega));
}

DOCTEST_TEST_CASE("SpectralModel tracking theta uses octave distance at every frequency")
{
    AnalysisAtom lowCandidate(
        AtOctaveDistance(0.01f, 0.25f),
        1.0f,
        0.0f,
        ScalarParameter::Index());
    AnalysisAtom highCandidate(
        AtOctaveDistance(0.10f, 0.25f),
        1.0f,
        0.0f,
        ScalarParameter::Index());

    float lowTheta = AnalysisAtom::PreferredMatchTheta(lowCandidate, std::log2f(0.01f), 0.5f);
    float highTheta = AnalysisAtom::PreferredMatchTheta(highCandidate, std::log2f(0.10f), 0.5f);

    DOCTEST_CHECK(lowTheta == doctest::Approx(0.5f));
    DOCTEST_CHECK(highTheta == doctest::Approx(0.5f));
}

DOCTEST_TEST_CASE("SpectralModel dominated merge follows the octave density cone")
{
    Input input = MakeInput(0.5f);
    input.m_slewDownAlpha = ScalarParameter::Parameter(0.0f);

    const float distances[] = {0.0f, 0.25f, 0.5f};
    const float expectedMagnitudes[] = {0.0625f, 0.125f, 0.25f};
    for (size_t i = 0; i < 3; ++i)
    {
        Atom atom = MakeAtom(0.10f);
        atom.m_synthesisMagnitude = 0.25f;
        AnalysisAtom analysisAtom(
            AtOctaveDistance(0.10f, distances[i]),
            1.0f,
            0.0f,
            ScalarParameter::Index());

        atom.MergeDominated(analysisAtom, 0.5f, input);

        DOCTEST_CHECK(atom.m_synthesisMagnitude == doctest::Approx(expectedMagnitudes[i]));
    }
}

DOCTEST_TEST_CASE("SpectralModel domination uses the old atom density across lane boundaries")
{
    using LaneModel = SpectralModelGeneric<12, FrequencyDependentParameter>;
    using LaneParameter = FrequencyDependentParameter::Parameter;
    const float oldDensities[] = {0.249999f, 0.25f, 0.250001f, 1.0f};
    const float peakDensities[] = {1.0f, 1.0f, 1.0f, 0.01f};
    const float expectedMagnitudes[] = {0.25f, 0.25f, 0.25f, 1.0f / 12.0f};
    for (size_t i = 0; i < 4; ++i)
    {
        LaneModel model;
        LaneModel::Input input;
        input.m_parameterInput.m_linearFreqs = LaneParameter(1.0f);
        input.m_omegaDensity.m_parameters[0] = oldDensities[i];
        input.m_omegaDensity.m_parameters[1] = peakDensities[i];
        input.m_slewDownAlpha = LaneParameter(0.0f);
        float oldOmega = 0.015625f;
        float peakOmega = AtOctaveDistance(oldOmega, 0.25f);
        auto oldIndex = FrequencyDependentParameter::GetIndexForFrequency(oldOmega, input.m_parameterInput);
        auto peakIndex = FrequencyDependentParameter::GetIndexForFrequency(peakOmega, input.m_parameterInput);
        model.m_atoms.Add(LaneModel::Atom(oldOmega, 0.25f, oldIndex, oldOmega, 0.25f, 0));
        model.m_atoms.Add(LaneModel::Atom(peakOmega, 1.0f, peakIndex, peakOmega, 1.0f, 0));
        auto* tail = model.m_atoms[0];
        LaneModel::AnalysisAtomArray peaks;
        peaks.Add(LaneModel::AnalysisAtom(peakOmega, 1.0f, 0, peakIndex));

        model.TrackAnalysisAtoms(peaks, input);

        DOCTEST_REQUIRE(model.IsAtomAllocated(tail));
        DOCTEST_CHECK(tail->m_synthesisMagnitude == doctest::Approx(expectedMagnitudes[i]));
    }
}

DOCTEST_TEST_CASE("SpectralModel matched analysis atoms do not spawn duplicates")
{
    Model model;
    Input input = MakeInput(0.5f);
    model.m_atoms.Add(MakeAtom(0.10f));
    AnalysisAtomArray atoms;
    AddAnalysisAtom(atoms, 0.10f, 1.0f);

    model.TrackAnalysisAtoms(atoms, input);

    DOCTEST_CHECK(model.m_atoms.Size() == 1);
}

DOCTEST_TEST_CASE("SpectralModel matcher preserves stationary identities using analysis frequency order")
{
    Model model;
    Input input = MakeInput(1.0f);
    model.m_atoms.Add(MakeAtom(0.125f));
    model.m_atoms.Add(MakeAtom(0.0625f));
    Atom* high = model.m_atoms[0];
    Atom* low = model.m_atoms[1];
    high->m_synthesisOmega = 0.01f;
    low->m_synthesisOmega = 0.25f;
    low->m_synthesisMagnitude = 0.25f;
    low->m_synthesisPhase = 0.375;
    AnalysisAtomArray peaks;
    AddAnalysisAtom(peaks, 0.125f, 0.1f);
    AddAnalysisAtom(peaks, 0.0625f, 1.0f);

    model.m_matcher.Match(model.m_atoms, peaks, input);

    auto& results = model.m_matcher.m_result;
    DOCTEST_REQUIRE(results.m_synthesisAtomResults.Size() == 2);
    DOCTEST_REQUIRE(results.m_analysisAtomResults.Size() == 2);
    DOCTEST_CHECK(results.m_synthesisAtomResults[0].m_atom == low);
    DOCTEST_CHECK(results.m_synthesisAtomResults[1].m_atom == high);
    for (size_t i = 0; i < 2; ++i)
    {
        DOCTEST_REQUIRE(results.m_synthesisAtomResults[i].m_isMatch);
        DOCTEST_CHECK(results.m_synthesisAtomResults[i].m_analysisAtom == &peaks[i]);
        DOCTEST_CHECK(results.m_analysisAtomResults[i].m_isMatched);
    }

    DOCTEST_CHECK(low->m_synthesisOmega == doctest::Approx(0.25f));
    DOCTEST_CHECK(low->m_synthesisMagnitude == doctest::Approx(0.25f));
    DOCTEST_CHECK(low->m_synthesisPhase == doctest::Approx(0.375));
    DOCTEST_CHECK(high->m_synthesisOmega == doctest::Approx(0.01f));
    DOCTEST_CHECK(model.m_atoms[0] == high);
}

DOCTEST_TEST_CASE("SpectralModel matcher maximizes total theta before match count without crossings")
{
    Model model;
    Input input = MakeInput(0.5f);
    model.m_atoms.Add(MakeAtom(0.0625f));
    model.m_atoms.Add(MakeAtom(AtOctaveDistance(0.0625f, 0.25f)));
    AnalysisAtomArray peaks;
    AddAnalysisAtom(peaks, AtOctaveDistance(0.0625f, -0.25f), 0.1f);
    AddAnalysisAtom(peaks, 0.0625f, 1.0f);

    model.m_matcher.Match(model.m_atoms, peaks, input);

    // One match scores 1.0; the ordered two-match alternative scores 0.55.
    // Matching the remaining old atom to the first peak would cross the winner.
    //
    auto& results = model.m_matcher.m_result;
    DOCTEST_REQUIRE(results.m_synthesisAtomResults.Size() == 2);
    DOCTEST_CHECK(results.m_synthesisAtomResults[0].m_isMatch);
    DOCTEST_CHECK(results.m_synthesisAtomResults[0].m_analysisAtom == &peaks[1]);
    DOCTEST_CHECK_FALSE(results.m_synthesisAtomResults[1].m_isMatch);
    DOCTEST_CHECK_FALSE(results.m_analysisAtomResults[0].m_isMatched);
    DOCTEST_CHECK(results.m_analysisAtomResults[1].m_isMatched);
}

DOCTEST_TEST_CASE("SpectralModel matcher accepts zero-score boundary pairs on equal total theta")
{
    Model model;
    Input input = MakeInput(1.0f);
    model.m_atoms.Add(MakeAtom(0.0625f));
    AnalysisAtomArray peaks;
    AddAnalysisAtom(peaks, 0.125f, 1.0f);

    model.m_matcher.Match(model.m_atoms, peaks, input);

    DOCTEST_CHECK(model.m_matcher.m_result.m_synthesisAtomResults[0].m_isMatch);
    DOCTEST_CHECK(model.m_matcher.m_result.m_analysisAtomResults[0].m_isMatched);
}

DOCTEST_TEST_CASE("SpectralModel matcher rejects weak pairs before reserving an analysis peak")
{
    Model model;
    Input input = MakeInput(0.5f);
    model.m_atoms.Add(MakeAtom(0.0625f));
    model.m_atoms[0]->m_synthesisMagnitude = 1000.0f;
    AnalysisAtomArray peaks;
    AddAnalysisAtom(peaks, 0.0625f, 0.5f);
    AddAnalysisAtom(peaks, AtOctaveDistance(0.0625f, 0.49f), 2.0f);

    model.m_matcher.Match(model.m_atoms, peaks, input);

    auto& results = model.m_matcher.m_result;
    DOCTEST_REQUIRE(results.m_synthesisAtomResults[0].m_isMatch);
    DOCTEST_CHECK(results.m_synthesisAtomResults[0].m_analysisAtom == &peaks[1]);
    DOCTEST_CHECK_FALSE(results.m_analysisAtomResults[0].m_isMatched);
    DOCTEST_CHECK(results.m_analysisAtomResults[1].m_isMatched);
}

DOCTEST_TEST_CASE("SpectralModel unclaimed analysis peak can suppress a tail before its attack")
{
    using LaneModel = SpectralModelGeneric<12, FrequencyDependentParameter>;
    using LaneParameter = FrequencyDependentParameter::Parameter;
    using LaneIndex = FrequencyDependentParameter::Index;
    LaneModel model;
    LaneModel::Input input;
    input.m_omegaDensity = LaneParameter(1.0f);
    input.m_omegaDensity.m_parameters[1] = 0.1f;
    input.m_slewUpAlpha = LaneParameter(0.1f);
    input.m_slewDownAlpha = LaneParameter(0.0f);
    float lowOmega = 0.015625f;
    float middleOmega = AtOctaveDistance(lowOmega, 0.25f);
    float highOmega = AtOctaveDistance(lowOmega, 0.5f);
    model.m_atoms.Add(LaneModel::Atom(lowOmega, 0.1f, LaneIndex(0, 0), lowOmega, 0.1f, 0));
    model.m_atoms.Add(LaneModel::Atom(middleOmega, 0.1f, LaneIndex(0, 1), middleOmega, 0.1f, 0));
    auto* tail = model.m_atoms[1];
    LaneModel::AnalysisAtomArray peaks;
    peaks.Add(LaneModel::AnalysisAtom(middleOmega, 0.3f, 0, LaneIndex(0, 1)));
    peaks.Add(LaneModel::AnalysisAtom(highOmega, 1.0f, 0, LaneIndex(0, 2)));

    model.TrackAnalysisAtoms(peaks, input);

    auto& results = model.m_matcher.m_result;
    DOCTEST_REQUIRE_FALSE(results.m_synthesisAtomResults[1].m_isMatch);
    DOCTEST_CHECK(results.m_synthesisAtomResults[1].m_analysisAtom == &peaks[0]);
    DOCTEST_CHECK_FALSE(results.m_analysisAtomResults[0].m_isMatched);
    DOCTEST_REQUIRE(model.IsAtomAllocated(tail));
    DOCTEST_CHECK(tail->m_synthesisMagnitude == doctest::Approx(0.1f * 0.1f / 0.3f));
    DOCTEST_REQUIRE(model.m_atoms.Size() == 3);
    DOCTEST_CHECK(model.m_atoms[2]->m_analysisOmega == doctest::Approx(middleOmega));
    DOCTEST_CHECK(model.m_atoms[2]->m_synthesisMagnitude == doctest::Approx(0.03f));
}

DOCTEST_TEST_CASE("SpectralModel matcher clears previous results with empty input on either side")
{
    Model model;
    Input input = MakeInput(1.0f);
    model.m_atoms.Add(MakeAtom(0.0625f));
    AnalysisAtomArray peaks;
    AddAnalysisAtom(peaks, 0.0625f, 1.0f);
    model.m_matcher.Match(model.m_atoms, peaks, input);
    DOCTEST_REQUIRE(model.m_matcher.m_result.m_synthesisAtomResults[0].m_isMatch);

    peaks.Clear();
    model.m_matcher.Match(model.m_atoms, peaks, input);
    auto& results = model.m_matcher.m_result;
    DOCTEST_REQUIRE(results.m_synthesisAtomResults.Size() == 1);
    DOCTEST_CHECK_FALSE(results.m_synthesisAtomResults[0].m_isMatch);
    DOCTEST_CHECK(results.m_synthesisAtomResults[0].m_analysisAtom == nullptr);
    DOCTEST_CHECK(results.m_analysisAtomResults.Empty());

    model.m_atoms.Pop();
    AddAnalysisAtom(peaks, 0.0625f, 1.0f);
    model.m_matcher.Match(model.m_atoms, peaks, input);
    DOCTEST_CHECK(results.m_synthesisAtomResults.Empty());
    DOCTEST_REQUIRE(results.m_analysisAtomResults.Size() == 1);
    DOCTEST_CHECK_FALSE(results.m_analysisAtomResults[0].m_isMatched);
    DOCTEST_CHECK(results.m_analysisAtomResults[0].m_analysisAtom == &peaks[0]);
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
            Model::DFT residual = spectrum;
            AnalysisAtomArray atoms;
            model.ExtractAndSubtractAnalysisAtoms(residual, atoms, input);
            DOCTEST_REQUIRE(atoms.Size() == 1);
            DOCTEST_CAPTURE(bin);
            DOCTEST_CAPTURE(phase);
            double phaseError = std::remainder(atoms[0].m_analysisPhase - phase, 1.0);
            double tolerance = bin == 32.0f ? 1e-5 : 0.01;
            DOCTEST_CHECK(std::abs(phaseError) < tolerance);

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
            model.ExtractAndSubtractAnalysisAtoms(spectrum, atoms, input);
            DOCTEST_REQUIRE(atoms.Size() == 1);
            DOCTEST_CAPTURE(bin);
            DOCTEST_CAPTURE(amplitude);
            DOCTEST_CHECK(std::abs(atoms[0].m_analysisMagnitude - amplitude / 4.0f) < amplitude / 4.0f * 0.005f);
        }
    }
}

DOCTEST_TEST_CASE("SpectralModel analysis leaves the residual in the DFT for overlapping tones")
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
                Model::DFT residual = spectrum;
                AnalysisAtomArray atoms;
                model.ExtractAndSubtractAnalysisAtoms(residual, atoms, input);
                DOCTEST_REQUIRE(!atoms.Empty());
                model.ExtractAtomsAndResidual(buffer, input);
                double originalEnergy = 0.0;
                double residualEnergy = 0.0;
                double differenceEnergy = 0.0;
                for (size_t k = 1; k < Model::x_maxComponents; ++k)
                {
                    originalEnergy += std::norm(spectrum.m_components[k]);
                    residualEnergy += std::norm(residual.m_components[k]);
                    double difference = std::abs(residual.m_components[k]) - model.m_residualModel.GetEnvelope(k);
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

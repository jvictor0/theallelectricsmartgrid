#include "doctest.h"

#include "DeepVocoder.hpp"

#include <memory>

DOCTEST_TEST_CASE("DeepVocoder follows a partial within its one-semitone matching radius")
{
    for (float cents : {-90.0f, 90.0f})
    {
        auto vocoder = std::make_unique<DeepVocoder>();
        DeepVocoder::Input input;
        SpectralModel::Input spectralInput = input.MakeSpectralInput();
        float frequency = 440.0f / 48000.0f;
        float nextFrequency = frequency * std::exp2f(cents / 1200.0f);
        vocoder->m_spectralModel.m_atoms.Add(
            SpectralModel::Atom(frequency, 0.25f, {}, frequency, 0.25f, 0));
        SpectralModel::Atom* original = vocoder->m_spectralModel.m_atoms[0];
        SpectralModel::AnalysisAtomArray peaks;
        peaks.Add(SpectralModel::AnalysisAtom(nextFrequency, 0.25f, 0, {}));

        vocoder->m_spectralModel.TrackAnalysisAtoms(peaks, spectralInput);

        DOCTEST_REQUIRE(vocoder->m_spectralModel.m_atoms.Size() == 1);
        DOCTEST_CHECK(vocoder->m_spectralModel.m_atoms[0] == original);
        DOCTEST_CHECK(original->m_synthesisOmega == doctest::Approx(nextFrequency));
        DOCTEST_CHECK(original->m_synthesisMagnitude == doctest::Approx(0.25f));
    }
}

DOCTEST_TEST_CASE("DeepVocoder starts a new partial beyond its one-semitone matching radius")
{
    for (float cents : {-110.0f, 110.0f})
    {
        auto vocoder = std::make_unique<DeepVocoder>();
        DeepVocoder::Input input;
        SpectralModel::Input spectralInput = input.MakeSpectralInput();
        float frequency = 440.0f / 48000.0f;
        float nextFrequency = frequency * std::exp2f(cents / 1200.0f);
        vocoder->m_spectralModel.m_atoms.Add(
            SpectralModel::Atom(frequency, 0.25f, {}, frequency, 0.25f, 0));
        SpectralModel::Atom* original = vocoder->m_spectralModel.m_atoms[0];
        SpectralModel::AnalysisAtomArray peaks;
        peaks.Add(SpectralModel::AnalysisAtom(nextFrequency, 0.25f, 0, {}));

        vocoder->m_spectralModel.TrackAnalysisAtoms(peaks, spectralInput);

        DOCTEST_REQUIRE(vocoder->m_spectralModel.m_atoms.Size() == 2);
        DOCTEST_CHECK(original->m_synthesisOmega == doctest::Approx(frequency));
        DOCTEST_CHECK(vocoder->m_spectralModel.m_atoms[1]->m_synthesisOmega == doctest::Approx(nextFrequency));
    }
}

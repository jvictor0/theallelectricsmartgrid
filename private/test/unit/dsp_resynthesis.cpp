#include "doctest.h"

#include <cmath>
#include <memory>

#include "Resynthesis.hpp"

DOCTEST_TEST_CASE("Resynthesizer tracks a stable partial without accumulating phase rounding drift")
{
    constexpr double x_sampleRate = 48000.0;
    constexpr size_t x_frames = 33750;
    for (double frequency : {750.0, 997.0, 7901.0, 17003.0})
    {
        DOCTEST_CAPTURE(frequency);
        auto synth = std::make_unique<Resynthesizer>();
        const int bin = static_cast<int>(std::round(frequency * Resynthesizer::x_N / x_sampleRate));
        const float initialPhase = 0.113f;
        float previousPhase = initialPhase;
        auto& oscillator = synth->m_oscillators[0];
        oscillator.m_synthesisPhase[bin] = initialPhase;
        Resynthesizer::PVDR pvdr(synth.get());
        pvdr.m_size = 1;
        pvdr.m_results[0].m_bin = bin;
        pvdr.m_results[0].m_parent = bin;
        for (size_t frame = 1; frame <= x_frames; ++frame)
        {
            const double cycles = initialPhase + frequency * static_cast<double>(frame * Resynthesizer::x_H) / x_sampleRate;
            const float phase = static_cast<float>(cycles - std::floor(cycles + 0.5));
            synth->m_prevAnalysisPhase[bin] = previousPhase;
            synth->m_analysisPhase[bin] = phase;
            synth->SetAnalysisOmega(bin);
            pvdr.m_results[0].m_delta = synth->m_omegaInstantaneous[bin] * Resynthesizer::x_H;
            oscillator.FixupPhases(1.0f, &pvdr);
            previousPhase = phase;
        }

        const double expected = initialPhase + frequency * 720.0;
        DOCTEST_CHECK(std::abs(oscillator.m_synthesisPhase[bin] - expected) < 1e-6);
    }
}

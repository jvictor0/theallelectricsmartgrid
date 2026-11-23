#include "NaiveReuppingLadder.hpp"
#include <algorithm>
#include <cmath>

using Filter = NaiveReuppingLadderGeneric<float, 1>;

static Filter Configure(double drive, double resonance, double cutoffHz)
{
    Filter filter;
    filter.SetCutoffAndResonance(cutoffHz / 192000., std::expm1(std::log(49.) * resonance) / 48.);
    filter.SetSaturationGain(0.25 * std::pow(20., drive));
    filter.Reset();
    return filter;
}

extern "C" void Coefficients(double drive, double resonance, double cutoffHz, float* destination)
{
    auto filter = Configure(drive,resonance,cutoffHz);
    const auto& detector = filter.m_inputEnvelope;
    const auto& svf = detector.m_stage1.m_state;
    const float values[] = {filter.m_stage1.m_alpha[0], filter.m_baseFeedback[0], filter.m_saturationGain[0],
        filter.m_saturationTanhGain[0], svf.m_a1[0], svf.m_a2[0], svf.m_a3[0], detector.m_powerAlpha[0],
        filter.m_referencePowerGain[0], filter.m_dampingScale[0], Filter::x_dampingEpsilon * Filter::x_dampingEpsilon};
    std::copy_n(values, 11, destination);
}

extern "C" void Reference(double drive, double resonance, double cutoffHz, int samples, const float* input, float* output)
{
    auto filter = Configure(drive,resonance,cutoffHz);
    for (int i = 0; i < samples; ++i)
    {
        output[i] = filter.Process(input[i])[0];
    }
}

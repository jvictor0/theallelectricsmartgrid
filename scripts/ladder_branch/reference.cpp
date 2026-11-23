#include "NaiveReuppingLadder.hpp"
#include <algorithm>
#include <array>
#include <vector>

// Production processor in double, with the full history taken just before input[0].
//
using Ladder = NaiveReuppingLadderGeneric<double, 1>;

static Ladder Configure(double cutoff, double resonance, double gain)
{
    Ladder filter;
    filter.SetCutoffAndResonance(cutoff, resonance);
    filter.SetSaturationGain(gain);
    return filter;
}

extern "C" void Coefficients(double cutoff, double resonance, double gain, double* result)
{
    auto filter = Configure(cutoff, resonance, gain);
    auto& envelope = filter.m_inputEnvelope;
    auto& svf = envelope.m_stage1.m_state;
    const double values[] = {filter.m_stage1.m_alpha[0], filter.m_baseFeedback[0], gain,
        filter.m_saturationTanhGain[0], svf.m_a1[0], svf.m_a2[0], svf.m_a3[0], envelope.m_powerAlpha[0],
        filter.m_referencePowerGain[0], filter.m_dampingScale[0], Ladder::x_dampingEpsilon * Ladder::x_dampingEpsilon};
    std::copy(std::begin(values), std::end(values), result);
}

extern "C" double CriticalFeedback(double cutoff, double resonance, double gain)
{
    return Configure(cutoff, resonance, gain).m_criticalFeedback[0];
}

// Experimental stage spacing. Keep the production detector and its tuning
// fixed so this comparison changes only the four audio-stage coefficients.
//
extern "C" void SpreadResponse(double cutoff, double resonance, double gain, double spread,
    const double* input, int length, double* output)
{
    auto filter = Configure(cutoff, resonance, gain);
    const auto alpha = filter.m_stage1.m_alpha;
    filter.m_stage1.m_alpha = alpha / (spread * spread);
    filter.m_stage2.m_alpha = alpha / spread;
    filter.m_stage3.m_alpha = alpha * spread;
    filter.m_stage4.m_alpha = alpha * (spread * spread);
    for (int i = 0; i < length; ++i)
    {
        output[i] = filter.Process(input[i])[0];
    }
}

extern "C" void ReturnMap(double cutoff, double resonance, double gain,
    const double* input, int length, int periods, double* state, double* output)
{
    auto filter = Configure(cutoff, resonance, gain);
    auto& dry = filter.m_inputEnvelope;
    auto& wet = filter.m_feedbackEnvelope;
    const std::array<Ladder::Vector*, 14> fields = {&filter.m_stage1.m_output, &filter.m_stage2.m_output,
        &filter.m_stage3.m_output, &filter.m_stage4.m_output, &dry.m_stage1.m_state.m_ic1, &dry.m_stage1.m_state.m_ic2,
        &dry.m_stage2.m_state.m_ic1, &dry.m_stage2.m_state.m_ic2, &dry.m_power, &wet.m_stage1.m_state.m_ic1,
        &wet.m_stage1.m_state.m_ic2, &wet.m_stage2.m_state.m_ic1, &wet.m_stage2.m_state.m_ic2, &wet.m_power};
    for (size_t i = 0; i < fields.size(); ++i)
    {
        *fields[i] = state[i];
    }

    for (int period = 0; period < periods; ++period)
    {
        for (int n = 0; n < length; ++n)
        {
            output[n] = filter.Process(input[n])[0];
        }
    }

    for (size_t i = 0; i < fields.size(); ++i)
    {
        state[i] = (*fields[i])[0];
    }
}

// Independent finite differences for each short shooting block. Each block
// starts with its own proposed wet state and the periodic upstream dry history.
//
extern "C" void ShootingBlocks(double cutoff, const double* resonance, double gain, const double* input,
    const int* edges, int blocks, const double* starts, const double* scales, double* ends,
    double* matrices, double* controls, double* output)
{
    constexpr std::array<size_t, 9> active = {0, 1, 2, 3, 9, 10, 11, 12, 13};
    constexpr double h = 1e-6;
    std::vector<double> scratch(edges[blocks]);
    for (int b = 0; b < blocks; ++b)
    {
        const auto* initial = starts + 14*b;
        const auto* samples = input + edges[b];
        auto* end = ends + 14*b;
        const int count = edges[b+1]-edges[b];
        std::copy_n(initial, 14, end);
        ReturnMap(cutoff, resonance[0], gain, samples, count, 1, end, output+edges[b]);
        for (size_t col = 0; col <= active.size(); ++col)
        {
            std::array<double, 14> plus, minus;
            std::copy_n(initial, 14, plus.begin());
            minus = plus;
            bool knob = col == active.size();
            if (!knob)
            {
                plus[active[col]] += h*scales[col];
                minus[active[col]] -= h*scales[col];
            }

            ReturnMap(cutoff, resonance[knob ? 1 : 0], gain, samples, count, 1, plus.data(), scratch.data());
            ReturnMap(cutoff, resonance[knob ? 2 : 0], gain, samples, count, 1, minus.data(), scratch.data());
            for (size_t row = 0; row < active.size(); ++row)
            {
                const double value = (plus[active[row]]-minus[active[row]])/(2*h*scales[row]);
                if (knob)
                {
                    controls[9*b+row] = value;
                }
                else
                {
                    matrices[81*b+9*row+col] = value;
                }
            }
        }
    }
}

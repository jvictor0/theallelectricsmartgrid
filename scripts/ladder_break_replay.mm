// Replay one continuation path with the survey's host coefficients and float recurrence.
//
#include "ladder_gpu_host.hpp"

extern "C" void LadderBreakPrepare(double drive, double cutoff, LadderGPU::Parameters* base, float* feedback)
{
    *base = PrepareGPUParameters(drive, 1, cutoff);
    for (int step = 0; step < 10000; ++step)
    {
        const double knob = step % 101 == 0 ? static_cast<double>(step / 101) / 99 : static_cast<double>(step) / 9999;
        feedback[step] = std::min(base->m_feedback, RequestedGPUFeedback(knob));
    }
}

extern "C" int LadderBreakTone(double target, double amplitude, float* input, int capacity)
{
    Options options;
    options.m_amplitude = amplitude;
    const auto tone = PrepareFrequency(target, options);
    if (tone.m_period > capacity)
    {
        return 0;
    }

    std::copy_n(tone.m_input.begin(), tone.m_period, input);
    return tone.m_period;
}

extern "C" void LadderBreakStep(const LadderGPU::Parameters* p, LadderGPU::History* state,
    const float* input, int period, int phase, int count, float* output)
{
    for (int n = 0; n < count; ++n)
    {
        const float y = LadderGPU::Step(*state, *p, input[phase]);
        if (output != nullptr)
        {
            output[n] = y;
        }

        if (++phase == period)
        {
            phase = 0;
        }
    }
}

#include "stock.cpp"
#include <array>
#include <cstdint>
#include <cstring>

extern "C" Filter* CreateCycleFilter(double drive, double resonance, double cutoff)
{
    return new Filter(Configure(drive, resonance, cutoff));
}

extern "C" void DestroyCycleFilter(Filter* filter)
{
    delete filter;
}

extern "C" Filter* CopyCycleFilter(const Filter* filter)
{
    return new Filter(*filter);
}

extern "C" void ReadCycleState(const Filter* filter, uint32_t* dest)
{
    const auto& dry = filter->m_inputEnvelope;
    const auto& wet = filter->m_feedbackEnvelope;
    const std::array<float, 14> values = {
        filter->m_stage1.m_output[0], filter->m_stage2.m_output[0],
        filter->m_stage3.m_output[0], filter->m_stage4.m_output[0],
        dry.m_stage1.m_state.m_ic1[0], dry.m_stage1.m_state.m_ic2[0],
        dry.m_stage2.m_state.m_ic1[0], dry.m_stage2.m_state.m_ic2[0], dry.m_power[0],
        wet.m_stage1.m_state.m_ic1[0], wet.m_stage1.m_state.m_ic2[0],
        wet.m_stage2.m_state.m_ic1[0], wet.m_stage2.m_state.m_ic2[0], wet.m_power[0]};
    std::memcpy(dest, values.data(), sizeof(values));
}

extern "C" void RunCycleRecord(Filter* filter, int samples, const float* input, float* output)
{
    for (int i = 0; i < samples; ++i)
    {
        output[i] = filter->Process(input[i])[0];
    }
}

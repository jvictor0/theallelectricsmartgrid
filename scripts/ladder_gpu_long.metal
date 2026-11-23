// Multiple tones share the survey's exact recurrence and compensated measurements.
// Each dispatch advances persistent histories through one case-specific interval.
//
struct LongTone
{
    uint m_offset;
    uint m_period;
};

struct LongInterval
{
    uint m_begin;
    uint m_end;
};

struct LongDispatch
{
    uint m_count;
    uint m_starts;
    uint m_cases;
    uint m_slot;
    uint m_synthetic;
};

kernel void LadderLongMeasure(device const LadderGPU::Parameters* parameters [[buffer(0)]],
    device LadderGPU::History* histories [[buffer(1)]],
    device const float* input [[buffer(2)]],
    device const float* quadrature [[buffer(3)]],
    device LadderGPU::Partial* output [[buffer(4)]],
    constant LongDispatch& dispatch [[buffer(5)]],
    device const LongTone* tones [[buffer(6)]],
    device const LongInterval* intervals [[buffer(7)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= dispatch.m_count)
    {
        return;
    }

    const uint point = index/dispatch.m_starts;
    const auto p = parameters[point];
    const auto tone = tones[point];
    const auto interval = intervals[dispatch.m_slot*dispatch.m_cases+point];
    auto state = histories[index];
    LadderGPU::Measurement measured;
    uint phase = interval.m_begin%tone.m_period;
    for (uint n = interval.m_begin; n < interval.m_end; ++n)
    {
        const float in = input[tone.m_offset+phase];
        const float quadratureValue = quadrature[tone.m_offset+phase];
        float y;
        if (dispatch.m_synthetic)
        {
            const float cosine = in*4;
            const float sine = quadratureValue*4;
            y = 0.125f+0.6f*cosine-0.8f*sine+0.3f*(4*cosine*cosine*cosine-3*cosine);
        }
        else
        {
            y = LadderGPU::Step(state,p,in);
        }

        measured.Add(y,in,quadratureValue);
        ++phase;
        if (phase == tone.m_period)
        {
            phase = 0;
        }
    }

    histories[index] = state;
    output[index] = measured.Get();
}

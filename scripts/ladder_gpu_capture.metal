// Continue saved survey histories, retaining every output sample.
//
struct CaptureRequest
{
    uint m_index;
    uint m_samples;
};

struct CaptureDispatch
{
    uint m_count;
    uint m_starts;
    uint m_absoluteBegin;
    uint m_offset;
    uint m_chunk;
};

kernel void LadderCapture(device const LadderGPU::Parameters* parameters [[buffer(0)]],
    device LadderGPU::History* histories [[buffer(1)]],
    device const float* input [[buffer(2)]],
    device const CaptureRequest* requests [[buffer(3)]],
    device float* output [[buffer(4)]],
    constant CaptureDispatch& dispatch [[buffer(5)]],
    device const LongTone* tones [[buffer(6)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= dispatch.m_count)
    {
        return;
    }

    const auto request = requests[index];
    const auto point = request.m_index/dispatch.m_starts;
    const auto tone = tones[point];
    const auto p = parameters[point];
    auto state = histories[request.m_index];
    uint phase = (dispatch.m_absoluteBegin+dispatch.m_offset)%tone.m_period;
    const uint count = min(dispatch.m_chunk,request.m_samples > dispatch.m_offset ? request.m_samples-dispatch.m_offset : 0);
    for (uint n = 0; n < count; ++n)
    {
        output[index*dispatch.m_chunk+n] = LadderGPU::Step(state,p,input[tone.m_offset+phase]);
        ++phase;
        if (phase == tone.m_period)
        {
            phase = 0;
        }
    }

    histories[request.m_index] = state;
}

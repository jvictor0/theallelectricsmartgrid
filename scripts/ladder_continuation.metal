// Fine steps use a small separate kernel so diagnostics do not raise register use.
//
kernel void ContinuationAdvance(device const LadderContinuation::Curve* curves [[buffer(0)]],
    device LadderGPU::History* histories [[buffer(1)]],
    device const LadderContinuation::Tone* tones [[buffer(2)]],
    device const float* input [[buffer(3)]],
    device const float* feedback [[buffer(4)]],
    constant LadderContinuation::Dispatch& dispatch [[buffer(5)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= dispatch.m_count)
    {
        return;
    }

    const auto curve = curves[index];
    auto p = curve.m_base;
    p.m_feedback = min(feedback[dispatch.m_level],curve.m_cap);
    auto state = histories[index];
    LadderContinuation::Advance(state,p,tones[curve.m_tone],input,dispatch.m_begin,dispatch.m_samples);
    histories[index] = state;
}

kernel void ContinuationMeasure(device const LadderContinuation::Curve* curves [[buffer(0)]],
    device LadderGPU::History* histories [[buffer(1)]],
    device const LadderContinuation::Tone* tones [[buffer(2)]],
    device const float* input [[buffer(3)]],
    device const float* feedback [[buffer(4)]],
    constant LadderContinuation::Dispatch& dispatch [[buffer(5)]],
    device const float* quadrature [[buffer(6)]],
    device LadderContinuation::Result* output [[buffer(7)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= dispatch.m_count)
    {
        return;
    }

    const auto curve = curves[index];
    auto p = curve.m_base;
    p.m_feedback = min(feedback[dispatch.m_level],curve.m_cap);
    auto state = histories[index];
    if (dispatch.m_level/dispatch.m_stride >= curve.m_firstRecord)
    {
        output[index] = LadderContinuation::Measure(state,p,tones[curve.m_tone],input,quadrature,dispatch);
    }
    else
    {
        LadderContinuation::Advance(state,p,tones[curve.m_tone],input,dispatch.m_begin,dispatch.m_samples);
    }

    histories[index] = state;
}

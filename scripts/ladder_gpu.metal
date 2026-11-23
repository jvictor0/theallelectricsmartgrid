// The host prepends ladder_gpu_common.hpp before compiling this source.
// One thread owns one trajectory. Segments preserve history between dispatches.
//
kernel void LadderMeasure(device const LadderGPU::Parameters* parameters [[buffer(0)]],
    device LadderGPU::History* histories [[buffer(1)]],
    device const float* input [[buffer(2)]],
    device const float* quadrature [[buffer(3)]],
    device LadderGPU::Partial* output [[buffer(4)]],
    constant LadderGPU::Dispatch& dispatch [[buffer(5)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= dispatch.m_count)
    {
        return;
    }

    const auto p = parameters[index];
    auto state = histories[index];
    LadderGPU::Measurement measured;
    for (unsigned n = dispatch.m_begin; n < dispatch.m_end; ++n)
    {
        float y;
        if (dispatch.m_synthetic)
        {
            const float cosine = input[n]*4;
            const float sine = quadrature[n]*4;
            y = 0.125f+0.6f*cosine-0.8f*sine+0.3f*(4*cosine*cosine*cosine-3*cosine);
        }
        else
        {
            y = LadderGPU::Step(state, p, input[n]);
        }

        measured.Add(y, input[n], quadrature[n]);
    }

    histories[index] = state;
    output[index] = measured.Get();
}

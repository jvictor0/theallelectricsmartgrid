// Shared measurement implementation for CPU reference and Metal continuation.
// The actual history always follows one trajectory; lookahead copies are diagnostics.
//
#pragma once
#ifdef __METAL_VERSION__
#define CONT_DEVICE device
#define CONT_THREAD thread
#else
#define CONT_DEVICE
#define CONT_THREAD
#endif

namespace LadderContinuation
{
struct Curve
{
    LadderGPU::Parameters m_base;
    float m_cap;
    unsigned m_tone;
    unsigned m_id;
    unsigned m_firstRecord;
};

struct Tone
{
    unsigned m_offset;
    unsigned m_period;
    unsigned m_cycles;
    unsigned m_lag;
    float m_weights[16];
};

struct Dispatch
{
    unsigned m_count;
    unsigned m_level;
    unsigned m_begin;
    unsigned m_samples;
    unsigned m_stride;
    unsigned m_synthetic;
    float m_inverseAmplitude;
};

struct Result
{
    LadderGPU::Partial m_first;
    LadderGPU::Partial m_second;
    float m_cycleError;
    float m_bufferError;
    float m_stateError;
    float m_stateEnergy;
    unsigned m_firstSamples;
    unsigned m_secondSamples;
};

inline unsigned NextPhase(unsigned phase, unsigned period)
{
    return phase+1 == period ? 0 : phase+1;
}

inline void Advance(CONT_THREAD LadderGPU::History& state, LadderGPU::Parameters p,
    Tone tone, CONT_DEVICE const float* input, unsigned begin, unsigned count)
{
    unsigned phase = begin%tone.m_period;
    for (unsigned n = 0; n < count; ++n)
    {
        LadderGPU::Step(state,p,input[tone.m_offset+phase]);
        phase = NextPhase(phase,tone.m_period);
    }
}

inline float Output(CONT_THREAD LadderGPU::History& state, LadderGPU::Parameters p,
    float input, float quadrature, Dispatch dispatch)
{
    const float y = LadderGPU::Step(state,p,input);
    if (dispatch.m_synthetic)
    {
        const float cosine = input*dispatch.m_inverseAmplitude;
        const float sine = quadrature*dispatch.m_inverseAmplitude;
        return 0.125f+0.6f*cosine-0.8f*sine+0.3f*(4*cosine*cosine*cosine-3*cosine);
    }

    return y;
}

inline Result Measure(CONT_THREAD LadderGPU::History& state, LadderGPU::Parameters p,
    Tone tone, CONT_DEVICE const float* input, CONT_DEVICE const float* quadrature, Dispatch dispatch)
{
    const unsigned q = tone.m_period;
    const unsigned begin = (dispatch.m_begin+dispatch.m_samples/2+q-1)/q*q;
    const unsigned end = (dispatch.m_begin+dispatch.m_samples)/q*q;
    const unsigned middle = begin+((end-begin)/q/2)*q;
    Advance(state,p,tone,input,dispatch.m_begin,begin-dispatch.m_begin);
    auto future = state;
    auto buffer = state;
    Advance(future,p,tone,input,begin,tone.m_lag-7);
    Advance(buffer,p,tone,input,begin,q);
    unsigned futurePhase = (begin+tone.m_lag-7)%q;
    float window[16];
    for (unsigned i = 0; i < 16; ++i)
    {
        const unsigned index = tone.m_offset+futurePhase;
        window[i] = Output(future,p,input[index],quadrature[index],dispatch);
        futurePhase = NextPhase(futurePhase,q);
    }

    LadderGPU::Measurement first,second;
    LadderGPU::Sum cycleError,bufferError,stateError,stateEnergy;
    unsigned phase = begin%q;
    for (unsigned n = begin; n < end; ++n)
    {
        const unsigned index = tone.m_offset+phase;
        const float in = input[index];
        const float quad = quadrature[index];
        const float y = Output(state,p,in,quad,dispatch);
        const float yBuffer = Output(buffer,p,in,quad,dispatch);
        float yCycle = 0;
        for (unsigned j = 0; j < 16; ++j)
        {
            yCycle += window[j]*tone.m_weights[j];
        }

        cycleError.Add((yCycle-y)*(yCycle-y));
        bufferError.Add((yBuffer-y)*(yBuffer-y));
        const float da = state.m_a-buffer.m_a;
        const float db = state.m_b-buffer.m_b;
        const float dc = state.m_c-buffer.m_c;
        const float dd = state.m_d-buffer.m_d;
        stateError.Add(da*da+db*db+dc*dc+dd*dd);
        stateEnergy.Add(state.m_a*state.m_a+state.m_b*state.m_b+state.m_c*state.m_c+state.m_d*state.m_d);
        if (n < middle)
        {
            first.Add(y,in,quad);
        }
        else
        {
            second.Add(y,in,quad);
        }

        for (unsigned j = 0; j < 15; ++j)
        {
            window[j] = window[j+1];
        }

        const unsigned next = tone.m_offset+futurePhase;
        window[15] = Output(future,p,input[next],quadrature[next],dispatch);
        futurePhase = NextPhase(futurePhase,q);
        phase = NextPhase(phase,q);
    }

    Advance(state,p,tone,input,end,dispatch.m_begin+dispatch.m_samples-end);
    return {first.Get(),second.Get(),cycleError.m_value,bufferError.m_value,
        stateError.m_value,stateEnergy.m_value,middle-begin,end-middle};
}
}

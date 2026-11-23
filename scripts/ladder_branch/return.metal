#include <metal_stdlib>
using namespace metal;

// One lane carries a directional derivative of the wet return map. All lanes also
// advance the physical orbit, including the two detector cascades and both powers.
//
struct Dual
{
    float value;
    float slope;
};

inline Dual operator+(Dual a, Dual b) { return {a.value + b.value, a.slope + b.slope}; }
inline Dual operator-(Dual a, Dual b) { return {a.value - b.value, a.slope - b.slope}; }
inline Dual operator*(float a, Dual b) { return {a * b.value, a * b.slope}; }

inline Dual Saturate(Dual input, device const float* c)
{
    float x = input.value * c[2];
    float xx = x*x;
    float den = fma(9.f, xx, 27.f);
    float derivative = 3.f*(xx-9.f)/den;
    float slope = fabs(x) >= 3.f ? 0.f : derivative*derivative;
    return {clamp(x*(27.f+xx)/den, -1.f, 1.f)/c[3], slope*c[2]*input.slope/c[3]};
}

inline Dual Band(Dual input, thread Dual* memory, device const float* c)
{
    Dual difference = input - memory[1];
    Dual bp = c[4]*memory[0] + c[5]*difference;
    Dual lp = memory[1] + c[5]*memory[0] + c[6]*difference;
    memory[0] = 2.f*bp - memory[0];
    memory[1] = 2.f*lp - memory[1];
    return bp;
}

inline void Detector(Dual input, thread Dual* memory, device const float* c)
{
    Dual band = Band(Band(input, memory, c), memory+2, c);
    Dual power = {band.value * band.value, 2.f * band.value * band.slope};
    memory[4] = memory[4] + c[7]*(power - memory[4]);
}

kernel void Period(device const float* packets [[buffer(0)]], device float* out [[buffer(1)]],
                   device const uint2* bounds [[buffer(2)]], uint job [[threadgroup_position_in_grid]],
                   uint column [[thread_index_in_threadgroup]])
{
    device const float* c = packets + bounds[job].x;
    uint q = bounds[job].y;
    Dual state[14];
    for (uint i = 0; i < 14; ++i)
    {
        bool active = i < 4 || i >= 9;
        uint axis = i < 4 ? i : i-5;
        state[i] = {c[11+i], active && column == axis+1 ? 1.f : 0.f};
    }

    for (uint n = 0; n < q; ++n)
    {
        float dry = state[8].value;
        float excess = state[13].value - c[8]*dry;
        float damp = c[9]/(dry+c[10]);
        float den = 1.f + damp*max(0.f, excess);
        float k = c[1]/den;
        float dk = excess > 0.f ? -k*damp*state[13].slope/den : 0.f;
        Dual input = {c[25+n], 0.f};
        Dual d = state[3];
        Dual stage = input - Dual{k*d.value, k*d.slope+dk*d.value};
        for (uint i = 0; i < 4; ++i)
        {
            state[i] = (1.f-c[0])*state[i] + c[0]*stage;
            if (i < 3)
            {
                stage = Saturate(state[i], c);
            }
        }

        Detector(input, state+4, c);
        Detector(state[3], state+9, c);
    }

    for (uint i = 0; i < 9; ++i)
    {
        Dual val = state[i < 4 ? i : i+5];
        out[(job*10+column)*9+i] = column == 0 ? val.value : val.slope;
    }
}

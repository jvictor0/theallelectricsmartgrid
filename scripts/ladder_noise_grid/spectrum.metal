#include <metal_stdlib>
using namespace metal;

constant uint nativeCount = 131072;
constant uint hostCount = 32768;
constant uint fftSize = 4096;
constant uint highBins = 2049;
constant uint lowBins = 513;
constant uint highFrames = 15;
constant uint frameStride = highFrames * highBins + lowBins;

struct Job
{
    float c[11];
    uint seed;
};

struct State
{
    float stage[4];
    float dry[5];
    float wet[5];
};

inline float Band(float x, thread float* state, thread const Job& job)
{
    const float diff = x - state[1];
    const float bp = fma(job.c[5], diff, job.c[4] * state[0]);
    const float lp = fma(job.c[6], diff, fma(job.c[5], state[0], state[1]));
    state[0] = fma(2.f, bp, -state[0]);
    state[1] = fma(2.f, lp, -state[1]);
    return bp;
}

inline void Detect(float x, thread float* state, thread const Job& job)
{
    const float bp = Band(Band(x, state, job), state + 2, job);
    state[4] += job.c[7] * (bp * bp - state[4]);
}

kernel void Simulate(device const Job* jobs [[buffer(0)]], device State* states [[buffer(1)]],
    device const float* noise [[buffer(2)]], device float* native [[buffer(3)]],
    constant uint& begin [[buffer(4)]], uint id [[thread_position_in_grid]])
{
    const Job job = jobs[id];
    State state = states[id];
    device const float* input = noise + job.seed * nativeCount + begin;
    device float* out = native + ulong(id) * (nativeCount + 256) + 256 + begin;
    for (uint n = 0; n < 16384; ++n)
    {
        const float x = input[n];
        float y = x;
        if (job.c[0] >= 0)
        {
            const float excess = max(0.f, state.wet[4] - job.c[8] * state.dry[4]) / (state.dry[4] + job.c[10]);
            const float k = job.c[1] / (1.f + job.c[9] * excess);
            y = x - k * state.stage[3];
            for (uint stage = 0; stage < 4; ++stage)
            {
                const float v = fma(job.c[0], y, (1.f - job.c[0]) * state.stage[stage]);
                state.stage[stage] = v;
                const float t = v * job.c[2];
                y = clamp(t * (t*t + 27.f) / fma(t*9.f, t, 27.f), -1.f, 1.f) / job.c[3];
            }

            Detect(x, state.dry, job);
            Detect(state.stage[3], state.wet, job);
            y *= 1.f + k;
        }

        out[n] = y;
    }

    states[id] = state;
}

// Causal linear-phase FIR. Each tile includes the overlap from the previous epoch.
//
kernel void Decimate(device const float* src [[buffer(0)]], device float* dst [[buffer(1)]],
    constant float* taps [[buffer(2)]], constant uint4& shape [[buffer(3)]],
    uint2 group [[threadgroup_position_in_grid]], uint t [[thread_index_in_threadgroup]])
{
    const uint factor = shape.x, history = shape.y, target = shape.z, prefix = shape.w;
    const ulong base = ulong(group.y) * (factor * target + history) + group.x * 256 * factor;
    threadgroup float tile[2304];
    for (uint i = t; i < 256*factor + history; i += 256)
    {
        tile[i] = src[base + i];
    }

    threadgroup_barrier(mem_flags::mem_threadgroup);
    float out = 0;
    for (uint j = 0; j <= history; ++j)
    {
        out = fma(tile[t*factor + history - j], taps[j], out);
    }

    dst[ulong(group.y)*(target + prefix) + prefix + group.x*256 + t] = out;
}

kernel void KeepOverlap(device float* native [[buffer(0)]], device float* host [[buffer(1)]], uint2 id [[thread_position_in_grid]])
{
    device float* a = native + ulong(id.y)*(nativeCount + 256);
    a[id.x] = a[nativeCount + id.x];
    if (id.x < 128)
    {
        device float* b = host + ulong(id.y)*(hostCount + 128);
        b[id.x] = b[hostCount + id.x];
    }
}

kernel void Analyze(device const float* host [[buffer(0)]], device const float* bass [[buffer(1)]],
    device float* powers [[buffer(2)]], constant float* window [[buffer(3)]], constant float2* twiddle [[buffer(4)]],
    uint2 group [[threadgroup_position_in_grid]], uint t [[thread_index_in_threadgroup]])
{
    const bool low = group.x == highFrames;
    device const float* samples = low ? bass + ulong(group.y)*fftSize :
        host + ulong(group.y)*(hostCount + 128) + 128 + group.x*2048;
    threadgroup float2 work[4096];
    for (uint i = t; i < fftSize; i += 256)
    {
        work[reverse_bits(i) >> 20] = float2(samples[i] * window[i], 0);
    }

    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint halfSize = 1; halfSize < fftSize; halfSize *= 2)
    {
        for (uint i = t; i < fftSize / 2; i += 256)
        {
            const uint j = i & (halfSize - 1), p = (i - j) * 2 + j;
            const float2 a = work[p], b = work[p + halfSize], w = twiddle[j*(fftSize/(2*halfSize))];
            const float2 v = float2(w.x*b.x - w.y*b.y, w.y*b.x + w.x*b.y);
            work[p] = a + v;
            work[p + halfSize] = a - v;
        }

        threadgroup_barrier(mem_flags::mem_threadgroup);
    }

    const float scale = 1.f / ((low ? 6000.f : 48000.f) * 1536.f);
    for (uint k = t; k < (low ? lowBins : highBins); k += 256)
    {
        const float sides = k == 0 || k == fftSize/2 ? 1.f : 2.f;
        powers[ulong(group.y)*frameStride + group.x*highBins + k] = scale*sides*dot(work[k],work[k]);
    }
}

kernel void Average(device const float* powers [[buffer(0)]], device float* result [[buffer(1)]], uint2 id [[thread_position_in_grid]])
{
    device const float* row = powers + ulong(id.y)*frameStride;
    float value = 0;
    if (id.x < highBins)
    {
        for (uint j = 0; j < highFrames; ++j)
        {
            value += row[j*highBins + id.x];
        }

        value /= highFrames;
    }
    else
    {
        value = row[highFrames*highBins + id.x - highBins];
    }

    result[ulong(id.y)*(highBins + lowBins) + id.x] = value;
}

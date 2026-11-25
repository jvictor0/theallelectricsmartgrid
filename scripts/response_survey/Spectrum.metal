constant uint nativeCount = 131072;
constant uint hostCount = 32768;
constant uint fftSize = 4096;
constant uint highBins = 2049;
constant uint lowBins = 513;
constant uint highFrames = 15;
constant uint frameStride = highFrames * highBins + lowBins;
constant uint engineFrames = 16;

// Step and SURVEY_STATE_WORDS come from the exported mono Lean processor.
//
kernel void Simulate(device float* states [[buffer(0)]], device const float* noise [[buffer(1)]],
    device float* native [[buffer(2)]], constant uint4& shape [[buffer(3)]],
    uint id [[thread_position_in_grid]])
{
    const uint start = shape.x;
    const uint waves = shape.y;
    const bool wet = id < shape.z;
    const bool capture = shape.w != 0;
    device const float* input = noise + ulong(id % waves) * nativeCount + start;
    device float* out = native + ulong(id) * (nativeCount + 256) + 256 + start;
    if (!wet)
    {
        if (capture)
        {
            for (uint n = 0; n < 16384; ++n)
            {
                out[n] = input[n];
            }
        }

        return;
    }

    float state[SURVEY_STATE_WORDS];
    device float* saved = states + ulong(id) * SURVEY_STATE_WORDS;
    for (uint i = 0; i < SURVEY_STATE_WORDS; ++i)
    {
        state[i] = saved[i];
    }

    for (uint n = 0; n < 16384; ++n)
    {
        float y = Step(state, input[n]);
        if (capture)
        {
            out[n] = y;
        }
    }

    for (uint i = 0; i < SURVEY_STATE_WORDS; ++i)
    {
        saved[i] = state[i];
    }
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
    constant uint& engine [[buffer(5)]],
    uint2 group [[threadgroup_position_in_grid]], uint t [[thread_index_in_threadgroup]])
{
    const bool low = !engine && group.x == highFrames;
    device const float* samples = low ? bass + ulong(group.y)*fftSize :
        host + ulong(group.y)*(hostCount + 128) + 128 + group.x*2048;
    if (engine)
    {
        const uint hop = nativeCount / engineFrames;
        samples = host + ulong(group.y)*(nativeCount + 256) + 256 + group.x*hop + (hop - fftSize)/2;
    }
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

    const float scale = (engine ? 1.f : (low ? 32.f : 4.f)) / (SURVEY_SAMPLE_RATE * 1536.f);
    const uint stride = engine ? engineFrames*highBins : frameStride;
    for (uint k = t; k < (low ? lowBins : highBins); k += 256)
    {
        const float sides = k == 0 || k == fftSize/2 ? 1.f : 2.f;
        powers[ulong(group.y)*stride + group.x*highBins + k] = scale*sides*dot(work[k],work[k]);
    }
}

kernel void Average(device const float* powers [[buffer(0)]], device float* result [[buffer(1)]], constant uint4& shape [[buffer(2)]], uint2 id [[thread_position_in_grid]])
{
    const bool engine = shape.y != 0;
    const uint frames = engine ? engineFrames : highFrames;
    device const float* row = powers + ulong(id.y)*(engine ? engineFrames*highBins : frameStride);
    float value = 0;
    if (id.x < highBins)
    {
        for (uint j = 0; j < frames; ++j)
        {
            value += row[j*highBins + id.x];
        }

        value /= frames;
    }
    else
    {
        value = row[highFrames*highBins + id.x - highBins];
    }

    ulong index = ulong(id.y)*shape.z + (engine ? highBins + lowBins : 0) + id.x;
    result[index] = shape.x ? value : result[index] + value;
}

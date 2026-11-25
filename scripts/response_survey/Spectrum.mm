#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
constexpr size_t x_native = 131072, x_host = 32768, x_fft = 4096, x_bins = 2562;
constexpr size_t x_engineBins = x_fft / 2 + 1;
struct Runner
{
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_queue;
    std::array<id<MTLComputePipelineState>, 5> m_pipelines;
    id<MTLBuffer> m_states, m_noise, m_native, m_host, m_bass, m_powers, m_result;
    id<MTLBuffer> m_fastFIR, m_slowFIR, m_window, m_twiddle;
    size_t m_capacity, m_waveCapacity, m_words, m_wet = 0, m_waves = 0, m_count = 0;
    bool m_engineSpectrum;
    uint32_t m_binCount;

    id<MTLBuffer> Buffer(size_t bytes)
    {
        auto b = [m_device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
        if (!b)
        {
            throw std::runtime_error("Metal buffer allocation failed");
        }

        return b;
    }

    id<MTLBuffer> Table(const float* data, size_t count)
    {
        auto b = Buffer(count*sizeof(float));
        std::memcpy(b.contents, data, b.length);
        return b;
    }

    Runner(const char* sourceText, size_t capacity, size_t waves, size_t words, bool engineSpectrum,
        const float* fast, const float* slow, const float* window, const float* twiddle)
        : m_capacity(capacity), m_waveCapacity(waves), m_words(words), m_engineSpectrum(engineSpectrum),
          m_binCount(x_bins + (engineSpectrum ? x_engineBins : 0))
    {
        m_device = MTLCreateSystemDefaultDevice();
        if (!m_device)
        {
            throw std::runtime_error("No Metal GPU");
        }

        m_queue = [m_device newCommandQueue];
        if (!m_queue)
        {
            throw std::runtime_error("Could not create Metal command queue");
        }

        NSError* err = nil;
        auto source = [NSString stringWithUTF8String:sourceText];
        auto opts = [MTLCompileOptions new];
        opts.mathMode = MTLMathModeSafe;
        opts.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
        auto lib = source ? [m_device newLibraryWithSource:source options:opts error:&err] : nil;
        if (!lib)
        {
            throw std::runtime_error(err.localizedDescription.UTF8String);
        }

        NSArray* names = @[@"Simulate", @"Decimate", @"KeepOverlap", @"Analyze", @"Average"];
        for (size_t i = 0; i < m_pipelines.size(); ++i)
        {
            m_pipelines[i] = [m_device newComputePipelineStateWithFunction:[lib newFunctionWithName:names[i]] error:&err];
            if (!m_pipelines[i])
            {
                throw std::runtime_error(err.localizedDescription.UTF8String);
            }
        }

        m_states = Buffer(capacity*m_words*sizeof(float));
        m_noise = Buffer(waves*x_native*sizeof(float));
        m_native = Buffer(capacity*(x_native+256)*4);
        m_host = Buffer(capacity*(x_host+128)*4);
        m_bass = Buffer(capacity*x_fft*4);
        m_powers = Buffer(capacity*(engineSpectrum ? 16*x_engineBins : 15*x_engineBins+513)*4);
        m_result = Buffer(capacity*m_binCount*4);
        m_fastFIR = Table(fast,257);
        m_slowFIR = Table(slow,129);
        m_window = Table(window,4096);
        m_twiddle = Table(twiddle,4096);
    }

    id<MTLComputeCommandEncoder> Encode(id<MTLCommandBuffer> cmd, size_t pipeline, NSArray* buffers)
    {
        auto enc = [cmd computeCommandEncoder];
        if (!enc)
        {
            throw std::runtime_error("Could not encode Metal spectrum pass");
        }

        [enc setComputePipelineState:m_pipelines[pipeline]];
        for (NSUInteger i = 0; i < buffers.count; ++i)
        {
            [enc setBuffer:buffers[i] offset:0 atIndex:i];
        }

        return enc;
    }

    void Decimate(id<MTLCommandBuffer> cmd, id<MTLBuffer> src, id<MTLBuffer> dst, id<MTLBuffer> taps,
        std::array<uint32_t,4> shape)
    {
        auto e = Encode(cmd,1,@[src,dst,taps]);
        [e setBytes:shape.data() length:sizeof(shape) atIndex:3];
        [e dispatchThreadgroups:MTLSizeMake(shape[2]/256,m_count,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
        [e endEncoding];
    }

    void Analyze(id<MTLCommandBuffer> cmd, bool first, uint32_t engine)
    {
        auto input = engine ? m_native : m_host;
        auto fft = Encode(cmd,3,@[input,m_bass,m_powers,m_window,m_twiddle]);
        [fft setBytes:&engine length:sizeof(engine) atIndex:5];
        [fft dispatchThreadgroups:MTLSizeMake(16,m_count,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
        [fft endEncoding];

        auto avg = Encode(cmd,4,@[m_powers,m_result]);
        std::array<uint32_t,4> shape = {first, engine, m_binCount, 0};
        [avg setBytes:shape.data() length:sizeof(shape) atIndex:2];
        [avg dispatchThreads:MTLSizeMake(engine ? x_engineBins : x_bins,m_count,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
        [avg endEncoding];
    }

    void Epoch(const float* noise, bool capture, bool measure, bool first)
    {
        std::memcpy(m_noise.contents, noise, m_waves*x_native*sizeof(float));
        auto cmd = [m_queue commandBuffer];
        for (uint32_t start = 0; start < x_native; start += 16384)
        {
            auto e = Encode(cmd,0,@[m_states,m_noise,m_native]);
            std::array<uint32_t,4> shape = {start, static_cast<uint32_t>(m_waves), static_cast<uint32_t>(m_wet), capture};
            [e setBytes:shape.data() length:sizeof(shape) atIndex:3];
            [e dispatchThreads:MTLSizeMake(m_count,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
            [e endEncoding];
        }

        if (capture)
        {
            Decimate(cmd,m_native,m_host,m_fastFIR,{4,256,32768,128});
            Decimate(cmd,m_host,m_bass,m_slowFIR,{8,128,4096,0});
            auto overlap = Encode(cmd,2,@[m_native,m_host]);
            [overlap dispatchThreads:MTLSizeMake(256,m_count,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
            [overlap endEncoding];
        }

        if (measure)
        {
            Analyze(cmd,first,0);
            if (m_engineSpectrum)
            {
                Analyze(cmd,first,1);
            }
        }

        [cmd commit];
        [cmd waitUntilCompleted];
        if (cmd.status != MTLCommandBufferStatusCompleted)
        {
            throw std::runtime_error(cmd.error.localizedDescription.UTF8String ?: "Metal spectrum command failed");
        }
    }
};
}

extern "C" void* OpenSpectrum(const char* source, unsigned capacity, unsigned waves, unsigned words, bool engineSpectrum,
    const float* fast, const float* slow, const float* window, const float* twiddle)
{
    @autoreleasepool
    {
        try
        {
            return new Runner(source, capacity, waves, words, engineSpectrum, fast, slow, window, twiddle);
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "%s\n", error.what());
            return nullptr;
        }
    }
}

extern "C" void CloseSpectrum(Runner* r)
{
    delete r;
}

extern "C" bool ResetSpectrum(Runner* r, unsigned wet, unsigned waves, const float* states)
{
    if (!wet || !waves || waves > r->m_waveCapacity || size_t(wet) + waves > r->m_capacity)
    {
        return false;
    }

    r->m_wet = wet;
    r->m_waves = waves;
    r->m_count = size_t(wet) + waves;
    std::memcpy(r->m_states.contents, states, wet*r->m_words*sizeof(float));
    for (size_t i = 0; i < r->m_count; ++i)
    {
        std::memset(static_cast<float*>(r->m_native.contents) + i*(x_native+256), 0, 256*sizeof(float));
        std::memset(static_cast<float*>(r->m_host.contents) + i*(x_host+128), 0, 128*sizeof(float));
    }

    return true;
}

extern "C" bool SpectrumEpoch(Runner* r, const float* noise, bool capture, bool measure, bool first)
{
    @autoreleasepool
    {
        try
        {
            r->Epoch(noise, capture, measure, first);
            return true;
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "%s\n", error.what());
            return false;
        }
    }
}

extern "C" void ReadSpectrum(Runner* r, float* powers)
{
    std::memcpy(powers, r->m_result.contents, r->m_count*r->m_binCount*sizeof(float));
}

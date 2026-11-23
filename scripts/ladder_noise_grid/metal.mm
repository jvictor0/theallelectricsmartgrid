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
struct Runner
{
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_queue;
    std::array<id<MTLComputePipelineState>, 5> m_pipelines;
    id<MTLBuffer> m_jobs, m_states, m_noise, m_native, m_host, m_bass, m_powers, m_result;
    id<MTLBuffer> m_fastFIR, m_slowFIR, m_window, m_twiddle;
    size_t m_capacity, m_seeds, m_count = 0;

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

    Runner(const char* file, size_t capacity, size_t seeds, const float* fast, const float* slow,
        const float* window, const float* twiddle) : m_capacity(capacity), m_seeds(seeds)
    {
        m_device = MTLCreateSystemDefaultDevice();
        if (!m_device)
        {
            throw std::runtime_error("No Metal GPU");
        }

        m_queue = [m_device newCommandQueue];
        NSError* err = nil;
        auto source = [NSString stringWithContentsOfFile:@(file) encoding:NSUTF8StringEncoding error:&err];
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

        m_jobs = Buffer(capacity*48);
        m_states = Buffer(capacity*14*4);
        m_noise = Buffer(seeds*x_native*4);
        m_native = Buffer(capacity*(x_native+256)*4);
        m_host = Buffer(capacity*(x_host+128)*4);
        m_bass = Buffer(capacity*x_fft*4);
        m_powers = Buffer(capacity*(15*2049+513)*4);
        m_result = Buffer(capacity*x_bins*4);
        m_fastFIR = Table(fast,257);
        m_slowFIR = Table(slow,129);
        m_window = Table(window,4096);
        m_twiddle = Table(twiddle,4096);
        std::fprintf(stderr, "%s: %zu trajectories per tile\n", m_device.name.UTF8String, capacity);
    }

    id<MTLComputeCommandEncoder> Encode(id<MTLCommandBuffer> cmd, size_t pipeline, NSArray* buffers)
    {
        auto enc = [cmd computeCommandEncoder];
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

    void Epoch(const float* noise, float* result)
    {
        std::memcpy(m_noise.contents, noise, m_noise.length);
        auto cmd = [m_queue commandBuffer];
        for (uint32_t start = 0; start < x_native; start += 16384)
        {
            auto e = Encode(cmd,0,@[m_jobs,m_states,m_noise,m_native]);
            [e setBytes:&start length:sizeof(start) atIndex:4];
            [e dispatchThreads:MTLSizeMake(m_count,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
            [e endEncoding];
        }

        Decimate(cmd,m_native,m_host,m_fastFIR,{4,256,32768,128});
        Decimate(cmd,m_host,m_bass,m_slowFIR,{8,128,4096,0});
        auto overlap = Encode(cmd,2,@[m_native,m_host]);
        [overlap dispatchThreads:MTLSizeMake(256,m_count,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
        [overlap endEncoding];
        auto fft = Encode(cmd,3,@[m_host,m_bass,m_powers,m_window,m_twiddle]);
        [fft dispatchThreadgroups:MTLSizeMake(16,m_count,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
        [fft endEncoding];
        auto avg = Encode(cmd,4,@[m_powers,m_result]);
        [avg dispatchThreads:MTLSizeMake(x_bins,m_count,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
        [avg endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];
        if (cmd.status != MTLCommandBufferStatusCompleted)
        {
            throw std::runtime_error(cmd.error.localizedDescription.UTF8String);
        }

        std::memcpy(result, m_result.contents, m_count*x_bins*4);
    }
};
}

extern "C" void* Open(const char* path, int capacity, int seeds, const float* f1, const float* f2,
    const float* window, const float* twiddle)
{
    @autoreleasepool
    {
        try
        {
            return new Runner(path, capacity, seeds, f1, f2, window, twiddle);
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "%s\n", error.what());
            return nullptr;
        }
    }
}

extern "C" void Reset(Runner* r, unsigned count, const void* jobs)
{
    r->m_count = count;
    std::memcpy(r->m_jobs.contents, jobs, count*48);
    std::memset(r->m_states.contents, 0, count*14*4);
    for (size_t i = 0; i < count; ++i)
    {
        std::memset(static_cast<float*>(r->m_native.contents) + i*(x_native+256), 0, 256*4);
        std::memset(static_cast<float*>(r->m_host.contents) + i*(x_host+128), 0, 128*4);
    }
}

extern "C" int Epoch(Runner* r, const float* noise, float* result)
{
    @autoreleasepool
    {
        try
        {
            r->Epoch(noise,result);
            return 0;
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
}

extern "C" void Capture(Runner* r, unsigned job, float* data)
{
    std::memcpy(data, static_cast<float*>(r->m_native.contents)+job*(x_native+256)+256, x_native*4);
}

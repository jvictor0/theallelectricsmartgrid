// Shared host-side Metal execution for the benchmark and resumable survey.
//
#pragma once
#define LADDER_ENERGY_GRID_EMBEDDED
#include "ladder_energy_grid.cpp"
#include "ladder_gpu_common.hpp"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace
{
using Clock = std::chrono::steady_clock;

float RequestedGPUFeedback(double resonance)
{
    return static_cast<float>(3.9*std::expm1(std::log(49)*resonance)/48);
}

LadderGPU::Parameters PrepareGPUParameters(double drive, double resonance, double cutoff)
{
    const double sine = std::sin(x_pi*cutoff/192000);
    LadderGPU::Parameters p;
    p.m_alpha = static_cast<float>(2*sine*(std::sqrt(1+sine*sine)-sine));
    p.m_decay = 1-p.m_alpha;
    p.m_gain = static_cast<float>(0.25*std::pow(20,drive));
    p.m_tanhGain = LadderGPU::RationalTanh(p.m_gain);
    const float slope = p.m_gain/p.m_tanhGain;
    const float ratio = p.m_alpha/(2-p.m_alpha)*slope;
    const float square = ratio*ratio;
    const float safe = 0.9f/(square*square+1e-8f);
    const float requested = RequestedGPUFeedback(resonance);
    p.m_feedback = std::min(requested,safe);
    return p;
}

LadderGPU::History PrepareGPUHistory(LadderGPU::Parameters p, uint64_t point, int trial,
    uint64_t seed, double amplitude)
{
    const double bound = 1.0/p.m_tanhGain;
    const uint64_t key = Mix(seed ^ Mix(point) ^ Mix(0x100000000ULL+trial));
    return {
        static_cast<float>((2*Uniform(key)-1)*(amplitude+p.m_feedback*bound)),
        static_cast<float>((2*Uniform(key+1)-1)*bound),
        static_cast<float>((2*Uniform(key+2)-1)*bound),
        static_cast<float>((2*Uniform(key+3)-1)*bound)};
}

struct Batch
{
    Frequency m_frequency;
    std::vector<float> m_input;
    std::vector<float> m_quadrature;
    std::vector<LadderGPU::Parameters> m_parameters;
    std::vector<LadderGPU::History> m_initial;
    std::vector<uint64_t> m_pointIDs;
    std::vector<int> m_starts;
};

std::string ReadSource(const fs::path& path)
{
    std::ifstream stream(path);
    Require(stream.good(), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

struct MetalRunner
{
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_queue;
    id<MTLComputePipelineState> m_pipeline;
    id<MTLBuffer> m_parameters;
    id<MTLBuffer> m_histories;
    id<MTLBuffer> m_input;
    id<MTLBuffer> m_quadrature;
    id<MTLBuffer> m_partials;
    size_t m_count;
    size_t m_capacity;
    size_t m_sampleCapacity;
    double m_gpuSeconds = 0;
    double m_wallSeconds = 0;

    MetalRunner(const Batch& batch)
    {
        m_device = MTLCreateSystemDefaultDevice();
        Require(m_device != nil, "No Metal device");
        m_queue = [m_device newCommandQueue];
        const auto directory = fs::path(__FILE__).parent_path();
        const auto source = ReadSource(directory/"ladder_gpu_common.hpp")+"\n"+ReadSource(directory/"ladder_gpu.metal");
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.mathMode = MTLMathModeSafe;
        options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
        NSError* error = nil;
        id<MTLLibrary> library = [m_device newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()]
            options:options error:&error];
        Require(library != nil, error ? error.localizedDescription.UTF8String : "Metal library creation failed");
        id<MTLFunction> function = [library newFunctionWithName:@"LadderMeasure"];
        m_pipeline = [m_device newComputePipelineStateWithFunction:function error:&error];
        Require(m_pipeline != nil, error ? error.localizedDescription.UTF8String : "Metal pipeline creation failed");
        m_count = batch.m_initial.size();
        m_capacity = m_count;
        m_sampleCapacity = batch.m_input.size();
        m_parameters = [m_device newBufferWithBytes:batch.m_parameters.data()
            length:m_count*sizeof(LadderGPU::Parameters) options:MTLResourceStorageModeShared];
        m_histories = [m_device newBufferWithLength:m_count*sizeof(LadderGPU::History) options:MTLResourceStorageModeShared];
        m_input = [m_device newBufferWithBytes:batch.m_input.data()
            length:batch.m_input.size()*sizeof(float) options:MTLResourceStorageModeShared];
        m_quadrature = [m_device newBufferWithBytes:batch.m_quadrature.data()
            length:batch.m_quadrature.size()*sizeof(float) options:MTLResourceStorageModeShared];
        m_partials = [m_device newBufferWithLength:m_capacity*32*sizeof(LadderGPU::Partial)
            options:MTLResourceStorageModeShared];
        Require(m_queue && m_parameters && m_histories && m_input && m_quadrature && m_partials, "Metal buffer allocation failed");
    }

    void Run(const Batch& batch, bool synthetic, bool shortRun)
    {
        const auto started = Clock::now();
        m_count = batch.m_initial.size();
        Require(m_count > 0 && m_count <= m_capacity && batch.m_frequency.m_segments.size() <= 32
            && batch.m_input.size() <= m_sampleCapacity, "Batch exceeds allocated Metal capacity");
        std::memcpy(m_parameters.contents,batch.m_parameters.data(),m_count*sizeof(LadderGPU::Parameters));
        std::memcpy(m_input.contents,batch.m_input.data(),batch.m_input.size()*sizeof(float));
        std::memcpy(m_quadrature.contents,batch.m_quadrature.data(),batch.m_quadrature.size()*sizeof(float));
        std::memcpy(m_histories.contents, batch.m_initial.data(), m_count*sizeof(LadderGPU::History));
        m_gpuSeconds = 0;
        const auto& segments = batch.m_frequency.m_segments;
        for (size_t segment = 0; segment < (shortRun ? 1 : segments.size()); ++segment)
        {
            @autoreleasepool
            {
                const LadderGPU::Dispatch dispatch{static_cast<unsigned>(m_count),
                    static_cast<unsigned>(segments[segment].m_begin),
                    static_cast<unsigned>(shortRun ? 8 : segments[segment].m_end), synthetic ? 1u : 0u};
                id<MTLCommandBuffer> command = [m_queue commandBuffer];
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                [encoder setComputePipelineState:m_pipeline];
                [encoder setBuffer:m_parameters offset:0 atIndex:0];
                [encoder setBuffer:m_histories offset:0 atIndex:1];
                [encoder setBuffer:m_input offset:0 atIndex:2];
                [encoder setBuffer:m_quadrature offset:0 atIndex:3];
                [encoder setBuffer:m_partials offset:segment*m_count*sizeof(LadderGPU::Partial) atIndex:4];
                [encoder setBytes:&dispatch length:sizeof(dispatch) atIndex:5];
                [encoder dispatchThreads:MTLSizeMake(m_count,1,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
                [encoder endEncoding];
                [command commit];
                [command waitUntilCompleted];
                Require(command.status == MTLCommandBufferStatusCompleted,
                    command.error ? command.error.localizedDescription.UTF8String : "Metal execution failed");
                m_gpuSeconds += command.GPUEndTime-command.GPUStartTime;
            }
        }

        m_wallSeconds = std::chrono::duration<double>(Clock::now()-started).count();
    }
};

}

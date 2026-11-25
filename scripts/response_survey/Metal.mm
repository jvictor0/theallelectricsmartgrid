#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstring>
#include <string>

struct SurveyMetal
{
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_queue;
    id<MTLComputePipelineState> m_pipeline;
};

static thread_local std::string s_error;

extern "C" const char* SurveyMetalError()
{
    return s_error.c_str();
}

extern "C" void* SurveyMetalOpen(const char* source)
{
    @autoreleasepool
    {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device)
        {
            s_error = "Metal device unavailable";
            return nullptr;
        }

        NSError* error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.mathMode = MTLMathModeSafe;
        id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:source]
                                                     options:options error:&error];
        id<MTLFunction> function = [library newFunctionWithName:@"SurveyRun"];
        id<MTLComputePipelineState> pipeline = function ? [device newComputePipelineStateWithFunction:function error:&error] : nil;
        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!pipeline || !queue)
        {
            s_error = error.localizedDescription.UTF8String ?: "Could not create Metal pipeline";
            return nullptr;
        }

        return new SurveyMetal{device, queue, pipeline};
    }
}

extern "C" void SurveyMetalClose(SurveyMetal* gpu)
{
    delete gpu;
}

extern "C" bool SurveyMetalRun(SurveyMetal* gpu, const float* initial, unsigned words,
                              const float* input, float* output, unsigned rows, unsigned count)
{
    @autoreleasepool
    {
        const size_t bytes = static_cast<size_t>(count) * rows * sizeof(float);
        id<MTLBuffer> state = [gpu->m_device newBufferWithBytes:initial length:rows * sizeof(float) * words options:MTLResourceStorageModeShared];
        id<MTLBuffer> in = [gpu->m_device newBufferWithBytes:input length:bytes options:MTLResourceStorageModeShared];
        id<MTLBuffer> out = [gpu->m_device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
        if (!state || !in || !out)
        {
            s_error = "Metal allocation failed";
            return false;
        }

        id<MTLCommandBuffer> cmd = [gpu->m_queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        if (!cmd || !enc)
        {
            s_error = "Could not schedule Metal work";
            return false;
        }

        [enc setComputePipelineState:gpu->m_pipeline];
        [enc setBuffer:state offset:0 atIndex:0];
        [enc setBuffer:in offset:0 atIndex:1];
        [enc setBuffer:out offset:0 atIndex:2];
        [enc setBytes:&count length:sizeof(count) atIndex:3];
        [enc dispatchThreads:MTLSizeMake(rows, 1, 1) threadsPerThreadgroup:MTLSizeMake(MIN(rows, gpu->m_pipeline.threadExecutionWidth), 1, 1)];
        [enc endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];
        if (cmd.status != MTLCommandBufferStatusCompleted)
        {
            s_error = cmd.error.localizedDescription.UTF8String ?: "Metal execution failed";
            return false;
        }

        std::memcpy(output, out.contents, bytes);
        return true;
    }
}

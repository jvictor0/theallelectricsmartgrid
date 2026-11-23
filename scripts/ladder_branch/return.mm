#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <cstdio>
#include <vector>

static bool Read(void* dest, size_t unit, size_t size)
{
    return std::fread(dest, unit, size, stdin) == size;
}

int main(int argc, const char** argv)
{
    @autoreleasepool
    {
        if (argc != 2)
        {
            return 1;
        }

        auto device = MTLCreateSystemDefaultDevice();
        auto queue = [device newCommandQueue];
        NSError* error = nil;
        auto source = [NSString stringWithContentsOfFile:@(argv[1]) encoding:NSUTF8StringEncoding error:&error];
        auto options = [MTLCompileOptions new];
        options.mathMode = MTLMathModeSafe;
        options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
        auto library = source ? [device newLibraryWithSource:source options:options error:&error] : nil;
        auto function = [library newFunctionWithName:@"Period"];
        auto pipeline = function ? [device newComputePipelineStateWithFunction:function error:&error] : nil;
        if (!queue || !pipeline)
        {
            std::fprintf(stderr, "Metal startup failed: %s\n", error ? error.localizedDescription.UTF8String : "no device");
            return 1;
        }

        std::fprintf(stderr, "Period evaluator: %s\n", device.name.UTF8String);
        std::fwrite("LRP1", 1, 4, stdout);
        std::fflush(stdout);
        uint32_t count;
        while (Read(&count, sizeof(count), 1))
        {
            @autoreleasepool
            {
                if (count == 0 || count > 128)
                {
                    return 2;
                }

                std::vector<std::array<uint32_t, 2>> bounds(count);
                std::vector<float> data;
                for (auto& region : bounds)
                {
                    uint32_t q;
                    if (!Read(&q, sizeof(q), 1) || q < 3 || q > 10000)
                    {
                        return 2;
                    }

                    region = {static_cast<uint32_t>(data.size()), q};
                    data.resize(data.size()+25+q);
                    if (!Read(data.data()+region[0], sizeof(float), 25+q))
                    {
                        return 2;
                    }
                }

                auto packets = [device newBufferWithBytes:data.data() length:data.size()*sizeof(float) options:MTLResourceStorageModeShared];
                auto output = [device newBufferWithLength:count*90*sizeof(float) options:MTLResourceStorageModeShared];
                auto offsets = [device newBufferWithBytes:bounds.data() length:count*sizeof(bounds[0]) options:MTLResourceStorageModeShared];
                if (!packets || !output || !offsets)
                {
                    return 3;
                }

                auto command = [queue commandBuffer];
                auto encoder = [command computeCommandEncoder];
                [encoder setComputePipelineState:pipeline];
                [encoder setBuffer:packets offset:0 atIndex:0];
                [encoder setBuffer:output offset:0 atIndex:1];
                [encoder setBuffer:offsets offset:0 atIndex:2];
                [encoder dispatchThreadgroups:MTLSizeMake(count, 1, 1) threadsPerThreadgroup:MTLSizeMake(10, 1, 1)];
                [encoder endEncoding];
                [command commit];
                [command waitUntilCompleted];
                if (command.status != MTLCommandBufferStatusCompleted)
                {
                    std::fprintf(stderr, "Metal dispatch failed: %s\n", command.error.localizedDescription.UTF8String);
                    return 3;
                }

                if (std::fwrite(output.contents, sizeof(float), count*90, stdout) != count*90 || std::fflush(stdout))
                {
                    return 4;
                }
            }
        }

        return std::ferror(stdin) ? 2 : 0;
    }
}

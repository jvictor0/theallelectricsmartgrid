// Spectra use continuations of the ten-minute run, not fresh initial conditions.
// Reuse the long-run host setup and shared DSP exactly.
//
#define LADDER_GPU_LONG_EMBEDDED
#include "ladder_gpu_long.mm"

namespace
{
struct CaptureRequest
{
    unsigned m_index;
    unsigned m_samples;
};

struct CaptureDispatch
{
    unsigned m_count;
    unsigned m_starts;
    unsigned m_absoluteBegin;
    unsigned m_offset;
    unsigned m_chunk;
};

int Capture(int argc, char** argv)
{
    Require(argc == 7,"Usage: ladder-gpu-capture POINT_IDS FINAL_STATES REQUESTS ABSOLUTE_SAMPLE OUTPUT_DIRECTORY METADATA_SOURCE");
    const auto cases = PrepareCases(argv[1],1);
    const unsigned absoluteBegin = static_cast<unsigned>(std::stoul(argv[4]));
    Require(absoluteBegin < 1000000000,"Absolute sample out of range");
    const fs::path output = argv[5];
    Require(!fs::exists(output),"Capture output directory already exists");
    LongRunner runner(cases,ReadSource(fs::path(__FILE__).parent_path()/"ladder_gpu_capture.metal"));
    Require(fs::file_size(argv[2]) == runner.m_count*sizeof(LadderGPU::History),"Wrong saved-state size");
    std::ifstream stateFile(argv[2],std::ios::binary);
    stateFile.read(static_cast<char*>(runner.m_buffers[1].contents),runner.m_count*sizeof(LadderGPU::History));
    Require(stateFile.good(),"Cannot read saved states");
    std::vector<LadderGPU::History> saved(runner.m_count);
    std::memcpy(saved.data(),runner.m_buffers[1].contents,saved.size()*sizeof(saved[0]));
    std::vector<CaptureRequest> requests;
    std::vector<uint64_t> pointIDs;
    std::set<unsigned> seen;
    std::ifstream requestFile(argv[3]);
    uint64_t pointID;
    unsigned index,samples;
    while (requestFile >> pointID >> index >> samples)
    {
        Require(index < runner.m_count && cases[index/x_starts].m_id == pointID,"Request state does not match point");
        Require(seen.insert(index).second,"Duplicate history would cause a GPU race");
        Require(samples >= 64 && samples <= 100000000,"Invalid sample count");
        requests.push_back({index,samples});
        pointIDs.push_back(pointID);
    }

    Require(requestFile.eof() && !requests.empty(),"Invalid capture requests");
    NSError* error = nil;
    id<MTLComputePipelineState> pipeline = [runner.m_device newComputePipelineStateWithFunction:[runner.m_library newFunctionWithName:@"LadderCapture"] error:&error];
    Require(pipeline != nil,error ? error.localizedDescription.UTF8String : "Capture pipeline failed");
    constexpr unsigned x_chunk = 16384;
    id<MTLBuffer> requestBuffer = [runner.m_device newBufferWithBytes:requests.data() length:requests.size()*sizeof(requests[0]) options:MTLResourceStorageModeShared];
    id<MTLBuffer> sampleBuffer = [runner.m_device newBufferWithLength:requests.size()*x_chunk*sizeof(float) options:MTLResourceStorageModeShared];
    Require(requestBuffer != nil && sampleBuffer != nil,"Capture allocation failed");
    const auto advance = [&](unsigned offset, unsigned chunk)
    {
        @autoreleasepool
        {
            const CaptureDispatch dispatch{static_cast<unsigned>(requests.size()),x_starts,absoluteBegin,offset,chunk};
            id<MTLCommandBuffer> command = [runner.m_queue commandBuffer];
            id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
            [encoder setComputePipelineState:pipeline];
            for (unsigned i : {0,1,2,6})
            {
                [encoder setBuffer:runner.m_buffers[i] offset:0 atIndex:i];
            }

            [encoder setBuffer:requestBuffer offset:0 atIndex:3];
            [encoder setBuffer:sampleBuffer offset:0 atIndex:4];
            [encoder setBytes:&dispatch length:sizeof(dispatch) atIndex:5];
            [encoder dispatchThreads:MTLSizeMake(requests.size(),1,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
            [encoder endEncoding];
            [command commit];
            [command waitUntilCompleted];
            Require(command.status == MTLCommandBufferStatusCompleted,command.error ? command.error.localizedDescription.UTF8String : "Capture command failed");
        }
    };
    advance(0,64);
    double maxError = 0;
    const auto* actual = static_cast<const float*>(sampleBuffer.contents);
    for (size_t i = 0; i < requests.size(); ++i)
    {
        const auto request = requests[i];
        auto state = saved[request.m_index];
        const auto c = request.m_index/x_starts;
        const auto tone = runner.m_tones[c];
        for (unsigned n = 0; n < 64; ++n)
        {
            const float expected = LadderGPU::Step(state,cases[c].m_parameters,runner.m_input[tone.m_offset+(absoluteBegin+n)%tone.m_period]);
            maxError = std::max(maxError,std::abs(static_cast<double>(actual[i*64+n])-expected)/std::max(1.0f,std::abs(expected)));
        }
    }

    Require(maxError < 0.002,"Continued GPU/CPU samples disagree");
    std::vector<LadderGPU::History> capturedStates(runner.m_count);
    std::memcpy(capturedStates.data(),runner.m_buffers[1].contents,capturedStates.size()*sizeof(capturedStates[0]));
    std::memcpy(runner.m_buffers[1].contents,saved.data(),saved.size()*sizeof(saved[0]));
    runner.Advance(std::vector<LongInterval>(cases.size(),{absoluteBegin,absoluteBegin+64}),1,false);
    const auto* measuredStates = static_cast<const LadderGPU::History*>(runner.m_buffers[1].contents);
    for (auto request : requests)
    {
        Require(std::memcmp(&capturedStates[request.m_index],&measuredStates[request.m_index],sizeof(LadderGPU::History)) == 0,
            "Capture and original measurement kernel states differ");
    }

    std::memcpy(runner.m_buffers[1].contents,saved.data(),saved.size()*sizeof(saved[0]));
    fs::create_directories(output);
    fs::copy_file(argv[6],output/"selection.json");
    std::vector<int> files;
    unsigned maxSamples = 0;
    for (size_t i = 0; i < requests.size(); ++i)
    {
        const auto r = requests[i];
        const auto name = "wave-"+std::to_string(pointIDs[i])+"-"+std::to_string(r.m_index%x_starts)+".f32";
        const int fd = open((output/name).c_str(),O_WRONLY|O_CREAT|O_EXCL,0644);
        Require(fd >= 0,"Cannot create waveform");
        files.push_back(fd);
        maxSamples = std::max(maxSamples,r.m_samples);
    }

    const auto begin = Clock::now();
    for (unsigned offset = 0; offset < maxSamples; offset += x_chunk)
    {
        advance(offset,x_chunk);
        const auto* values = static_cast<const float*>(sampleBuffer.contents);
        for (size_t i = 0; i < requests.size(); ++i)
        {
            const auto n = std::min(x_chunk,requests[i].m_samples > offset ? requests[i].m_samples-offset : 0u);
            WriteAll(files[i],values+i*x_chunk,n*sizeof(float));
        }
    }

    for (int fd : files)
    {
        Require(fsync(fd) == 0,"Waveform sync failed");
        close(fd);
    }

    std::ostringstream validation;
    validation << std::setprecision(17) << "{\"max_cpu_gpu_error\":" << maxError
        << ",\"measurement_kernel_state_match\":true,\"absolute_begin\":" << absoluteBegin << ",\"captures\":" << requests.size()
        << ",\"elapsed_seconds\":" << std::chrono::duration<double>(Clock::now()-begin).count() << "}\n";
    WriteText(output/"validation.json",validation.str());
    std::cout << validation.str();
    return 0;
}
}

int main(int argc, char** argv)
{
    try
    {
        return Capture(argc,argv);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// Selected long trajectories, sharing the survey's coefficients, RNG and DSP.
// All per-start interval sums are retained for later coherent aggregation.
//
#include "ladder_gpu_host.hpp"
#include <set>

namespace
{
constexpr unsigned x_starts = 101;
constexpr unsigned x_chunkSlots = 32;
constexpr std::array<int,7> x_checkpoints{1,2,5,10,30,60,120};

struct LongTone
{
    unsigned m_offset;
    unsigned m_period;
};

struct LongInterval
{
    unsigned m_begin;
    unsigned m_end;
};

struct LongDispatch
{
    unsigned m_count;
    unsigned m_starts;
    unsigned m_cases;
    unsigned m_slot;
    unsigned m_synthetic;
};

struct LongCase
{
    uint64_t m_id;
    Frequency m_frequency;
    LadderGPU::Parameters m_parameters;
    std::vector<LongInterval> m_intervals;
};

std::vector<LongCase> PrepareCases(const fs::path& ids, int seconds)
{
    Options options;
    std::ifstream stream(ids);
    Require(stream.good(),"Cannot read point IDs");
    std::set<uint64_t> seen;
    std::vector<LongCase> result;
    uint64_t id;
    while (stream >> id)
    {
        Require(id < 100000000 && seen.insert(id).second,"Invalid or duplicate point ID");
        LongCase c;
        c.m_id = id;
        c.m_frequency = PrepareFrequency(20*std::pow(1000,(id/1000000)/99.0),options);
        c.m_parameters = PrepareGPUParameters((id/10000%100)/99.0,(id/100%100)/99.0,
            20*std::pow(1000,(id%100)/99.0));
        const unsigned end = static_cast<unsigned>(seconds*192000);
        std::vector<unsigned> boundaries{0,end};
        for (const auto& segment : c.m_frequency.m_segments)
        {
            boundaries.push_back(static_cast<unsigned>(segment.m_begin));
            boundaries.push_back(static_cast<unsigned>(segment.m_end));
        }

        for (unsigned n = c.m_frequency.m_windowSamples; n < end; n += c.m_frequency.m_windowSamples)
        {
            boundaries.push_back(n);
        }

        std::vector<int> checkpoints(x_checkpoints.begin(),x_checkpoints.end());
        checkpoints.push_back(seconds);
        const unsigned q = c.m_frequency.m_period;
        for (int checkpoint : checkpoints)
        {
            if (checkpoint > seconds)
            {
                continue;
            }

            const unsigned t = static_cast<unsigned>(checkpoint*192000);
            boundaries.push_back(t/q*q);
            for (unsigned duration : {t/2,96000u,384000u,1920000u})
            {
                const unsigned begin = t > duration ? t-duration : 0;
                boundaries.push_back((begin+q-1)/q*q);
            }
        }

        std::sort(boundaries.begin(),boundaries.end());
        boundaries.erase(std::unique(boundaries.begin(),boundaries.end()),boundaries.end());
        for (size_t i = 1; i < boundaries.size(); ++i)
        {
            c.m_intervals.push_back({boundaries[i-1],boundaries[i]});
        }

        result.push_back(std::move(c));
    }

    Require(!result.empty() && result.size() <= 512,"Expected 1..512 cases");
    return result;
}

struct LongRunner
{
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_queue;
    id<MTLLibrary> m_library;
    id<MTLComputePipelineState> m_pipeline;
    std::array<id<MTLBuffer>,8> m_buffers;
    unsigned m_cases;
    unsigned m_count;
    size_t m_slots;
    std::vector<LadderGPU::History> m_initial;
    std::vector<float> m_input;
    std::vector<float> m_quadrature;
    std::vector<LongTone> m_tones;

    LongRunner(const std::vector<LongCase>& cases) : LongRunner(cases, "")
    {
    }

    LongRunner(const std::vector<LongCase>& cases, const std::string& extraSource)
        : m_cases(static_cast<unsigned>(cases.size())), m_count(m_cases*x_starts), m_slots(0)
    {
        std::vector<LadderGPU::Parameters> parameters;
        for (const auto& c : cases)
        {
            parameters.push_back(c.m_parameters);
            const auto& f = c.m_frequency;
            m_tones.push_back({static_cast<unsigned>(m_input.size()),static_cast<unsigned>(f.m_period)});
            m_input.insert(m_input.end(),f.m_input.begin(),f.m_input.begin()+f.m_period);
            m_quadrature.insert(m_quadrature.end(),f.m_quadrature.begin(),f.m_quadrature.begin()+f.m_period);
            for (unsigned start = 0; start < x_starts-1; ++start)
            {
                m_initial.push_back(PrepareGPUHistory(c.m_parameters,c.m_id,start,20261119,0.25));
            }

            m_initial.push_back({0,0,0,0});
            m_slots = std::max(m_slots,c.m_intervals.size());
        }

        m_device = MTLCreateSystemDefaultDevice();
        Require(m_device != nil,"Metal device unavailable");
        m_queue = [m_device newCommandQueue];
        const auto directory = fs::path(__FILE__).parent_path();
        const auto source = ReadSource(directory/"ladder_gpu_common.hpp")+"\n"+ReadSource(directory/"ladder_gpu_long.metal")+"\n"+extraSource;
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.mathMode = MTLMathModeSafe;
        options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
        NSError* error = nil;
        m_library = [m_device newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()] options:options error:&error];
        Require(m_library != nil,error ? error.localizedDescription.UTF8String : "Metal source failed");
        m_pipeline = [m_device newComputePipelineStateWithFunction:[m_library newFunctionWithName:@"LadderLongMeasure"] error:&error];
        Require(m_pipeline != nil,error ? error.localizedDescription.UTF8String : "Metal pipeline failed");
        m_buffers[0] = [m_device newBufferWithBytes:parameters.data() length:parameters.size()*sizeof(parameters[0]) options:MTLResourceStorageModeShared];
        m_buffers[1] = [m_device newBufferWithBytes:m_initial.data() length:m_initial.size()*sizeof(m_initial[0]) options:MTLResourceStorageModeShared];
        m_buffers[2] = [m_device newBufferWithBytes:m_input.data() length:m_input.size()*sizeof(float) options:MTLResourceStorageModeShared];
        m_buffers[3] = [m_device newBufferWithBytes:m_quadrature.data() length:m_quadrature.size()*sizeof(float) options:MTLResourceStorageModeShared];
        m_buffers[4] = [m_device newBufferWithLength:x_chunkSlots*m_count*sizeof(LadderGPU::Partial) options:MTLResourceStorageModeShared];
        m_buffers[6] = [m_device newBufferWithBytes:m_tones.data() length:m_tones.size()*sizeof(LongTone) options:MTLResourceStorageModeShared];
        m_buffers[7] = [m_device newBufferWithLength:x_chunkSlots*m_cases*sizeof(LongInterval) options:MTLResourceStorageModeShared];
        for (int i : {0,1,2,3,4,6,7})
        {
            Require(m_buffers[i] != nil,"Metal allocation failed");
        }
    }

    void Reset()
    {
        std::memcpy(m_buffers[1].contents,m_initial.data(),m_initial.size()*sizeof(m_initial[0]));
    }

    void Advance(const std::vector<LongInterval>& intervals, unsigned slots, bool synthetic)
    {
        Require(slots <= x_chunkSlots && intervals.size() == slots*m_cases,"Invalid interval batch");
        std::memcpy(m_buffers[7].contents,intervals.data(),intervals.size()*sizeof(LongInterval));
        @autoreleasepool
        {
            id<MTLCommandBuffer> command = [m_queue commandBuffer];
            for (unsigned slot = 0; slot < slots; ++slot)
            {
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                [encoder setComputePipelineState:m_pipeline];
                for (unsigned i : {0,1,2,3,4,6,7})
                {
                    [encoder setBuffer:m_buffers[i] offset:i == 4 ? slot*m_count*sizeof(LadderGPU::Partial) : 0 atIndex:i];
                }

                const LongDispatch dispatch{m_count,x_starts,m_cases,slot,synthetic ? 1u : 0u};
                [encoder setBytes:&dispatch length:sizeof(dispatch) atIndex:5];
                [encoder dispatchThreads:MTLSizeMake(m_count,1,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
                [encoder endEncoding];
            }

            [command commit];
            [command waitUntilCompleted];
            Require(command.status == MTLCommandBufferStatusCompleted,
                command.error ? command.error.localizedDescription.UTF8String : "Metal command failed");
        }
    }
};

void Validate(LongRunner& runner, const std::vector<LongCase>& cases, const fs::path& path)
{
    std::vector<LongInterval> intervals;
    for (auto interval : {LongInterval{0,7},LongInterval{7,40},LongInterval{40,71}})
    {
        intervals.insert(intervals.end(),cases.size(),interval);
    }

    runner.Advance(intervals,3,false);
    const auto* actual = static_cast<const LadderGPU::Partial*>(runner.m_buffers[4].contents);
    const auto* states = static_cast<const LadderGPU::History*>(runner.m_buffers[1].contents);
    double error = 0;
    for (unsigned i = 0; i < runner.m_count; ++i)
    {
        auto state = runner.m_initial[i];
        const auto point = i/x_starts;
        const auto tone = runner.m_tones[point];
        for (unsigned j = 0; j < 3; ++j)
        {
            LadderGPU::Measurement measured;
            const auto segment = intervals[j*cases.size()+point];
            for (unsigned n = segment.m_begin; n < segment.m_end; ++n)
            {
                const auto index = tone.m_offset+n%tone.m_period;
                measured.Add(LadderGPU::Step(state,cases[point].m_parameters,runner.m_input[index]),
                    runner.m_input[index],runner.m_quadrature[index]);
            }

            const auto expected = measured.Get();
            const auto observed = actual[j*runner.m_count+i];
            const std::array<float,5> a{expected.m_energy,expected.m_real,expected.m_imaginary,expected.m_dc,expected.m_peak};
            const std::array<float,5> b{observed.m_energy,observed.m_real,observed.m_imaginary,observed.m_dc,observed.m_peak};
            for (size_t k = 0; k < a.size(); ++k)
            {
                error = std::max(error,std::abs(static_cast<double>(a[k])-b[k])/std::max(1.0f,std::abs(a[k])));
            }
        }

        const std::array<float,4> a{state.m_a,state.m_b,state.m_c,state.m_d};
        const std::array<float,4> b{states[i].m_a,states[i].m_b,states[i].m_c,states[i].m_d};
        for (size_t k = 0; k < a.size(); ++k)
        {
            error = std::max(error,std::abs(static_cast<double>(a[k])-b[k])/std::max(1.0f,std::abs(a[k])));
        }
    }

    Require(error < 2e-5,"Segmented CPU/GPU comparison failed");
    intervals.clear();
    for (const auto& c : cases)
    {
        intervals.push_back({0,static_cast<unsigned>(c.m_frequency.m_windowSamples)});
    }

    runner.Advance(intervals,1,true);
    double syntheticError = 0;
    for (unsigned i = 0; i < runner.m_count; ++i)
    {
        const double samples = intervals[i/x_starts].m_end;
        const auto p = actual[i];
        syntheticError = std::max({syntheticError,std::abs(p.m_energy/samples-.560625),
            std::abs(8*p.m_real/samples-.6),std::abs(8*p.m_imaginary/samples-.8),std::abs(p.m_dc/samples-.125)});
    }

    Require(syntheticError < 2e-6,"Synthetic projection comparison failed");
    runner.Reset();
    std::ostringstream text;
    text << std::setprecision(17) << "{\"status\":\"passed\",\"cpu_gpu_max_scaled_error\":" << error
        << ",\"synthetic_max_error\":" << syntheticError << "}\n";
    WriteText(path,text.str());
}

int Run(int argc, char** argv)
{
    Require(argc == 4,"Usage: ladder-gpu-long POINT_IDS SECONDS OUTPUT_DIRECTORY");
    const int seconds = std::stoi(argv[2]);
    Require(seconds >= 1 && seconds <= 3600,"Duration must be 1..3600 seconds");
    const fs::path output = argv[3];
    fs::create_directories(output);
    Require(!fs::exists(output/"partials.bin"),"Refusing to overwrite existing measurements");
    const auto cases = PrepareCases(argv[1],seconds);
    LongRunner runner(cases);
    Validate(runner,cases,output/"validation.json");
    std::ostringstream metadata;
    metadata << std::setprecision(17) << "{\"seconds\":" << seconds << ",\"sample_rate\":192000,\"starts\":" << x_starts
        << ",\"random_starts\":100,\"reset_start_index\":100,\"seed\":20261119,\"amplitude\":0.25,\"slots\":" << runner.m_slots
        << ",\"partial_fields\":[\"energy_sum\",\"cosine_product_sum\",\"negative_sine_product_sum\",\"dc_sum\",\"peak\"],\"cases\":[";
    for (size_t i = 0; i < cases.size(); ++i)
    {
        const auto& c = cases[i];
        const auto& f = c.m_frequency;
        metadata << (i ? "," : "") << "{\"point_id\":" << c.m_id << ",\"actual_hz\":" << f.m_actual
            << ",\"period_samples\":" << f.m_period << ",\"window_samples\":" << f.m_windowSamples << ",\"intervals\":[";
        for (size_t j = 0; j < c.m_intervals.size(); ++j)
        {
            metadata << (j ? "," : "") << '[' << c.m_intervals[j].m_begin << ',' << c.m_intervals[j].m_end << ']';
        }

        metadata << "]}";
    }

    metadata << "]}\n";
    WriteText(output/"metadata.json",metadata.str());
    const int file = open((output/"partials.bin").c_str(),O_WRONLY|O_CREAT|O_EXCL,0644);
    Require(file >= 0,"Could not open measurements");
    const std::array<uint64_t,6> header{0x31474e4f4c44414cULL,cases.size(),x_starts,runner.m_slots,192000,sizeof(LadderGPU::Partial)};
    WriteAll(file,header.data(),sizeof(header));
    const auto started = Clock::now();
    for (size_t first = 0; first < runner.m_slots; first += x_chunkSlots)
    {
        const unsigned count = static_cast<unsigned>(std::min<size_t>(x_chunkSlots,runner.m_slots-first));
        std::vector<LongInterval> intervals;
        for (unsigned j = 0; j < count; ++j)
        {
            for (const auto& c : cases)
            {
                intervals.push_back(first+j < c.m_intervals.size() ? c.m_intervals[first+j] : LongInterval{0,0});
            }
        }

        runner.Advance(intervals,count,false);
        WriteAll(file,runner.m_buffers[4].contents,count*runner.m_count*sizeof(LadderGPU::Partial));
        const double elapsed = std::chrono::duration<double>(Clock::now()-started).count();
        std::ostringstream status;
        status << "{\"status\":\"" << (first+count == runner.m_slots ? "complete" : "running")
            << "\",\"slots_complete\":" << first+count << ",\"slots\":" << runner.m_slots << ",\"elapsed_seconds\":" << elapsed << "}\n";
        WriteText(output/"progress.json",status.str());
        std::cout << status.str() << std::flush;
    }

    Require(fsync(file) == 0,"Measurement sync failed");
    close(file);
    const int stateFile = open((output/"final-states.bin").c_str(),O_WRONLY|O_CREAT|O_EXCL,0644);
    Require(stateFile >= 0,"Could not save final states");
    WriteAll(stateFile,runner.m_buffers[1].contents,runner.m_count*sizeof(LadderGPU::History));
    close(stateFile);
    return 0;
}
}

#ifndef LADDER_GPU_LONG_EMBEDDED
int main(int argc, char** argv)
{
    try
    {
        return Run(argc,argv);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#endif

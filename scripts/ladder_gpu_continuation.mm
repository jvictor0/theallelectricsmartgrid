// Resumable one-history resonance continuation; research only.
//
#include "ladder_gpu_host.hpp"
#include "ladder_continuation_common.hpp"
#include <memory>

namespace
{
namespace LC = LadderContinuation;
constexpr uint64_t x_contMagic = 0x314e54434c44414cULL;
constexpr uint64_t x_checkMagic = 0x31504b434c44414cULL;
constexpr uint64_t x_reserve = 10ULL*1024*1024*1024;

struct Settings
{
    unsigned m_grid = 100;
    unsigned m_fine = 10000;
    unsigned m_samples = 192000;
    unsigned m_warm = 48000;
    unsigned m_batch = 65536;
    double m_amplitude = std::pow(10.0,-18.0/20.0);
    double m_threshold = 3.5;
    uint64_t m_maxSteps = std::numeric_limits<uint64_t>::max();
    fs::path m_output;
    bool m_plan = false;
    bool m_benchmark = false;
    bool m_cpu = false;
    bool m_synthetic = false;
};

Settings ParseSettings(int argc,char** argv)
{
    Settings o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argv[i];
        if (key == "--plan")
        {
            o.m_plan = true;
        }
        else if (key == "--benchmark")
        {
            o.m_benchmark = true;
        }
        else if (key == "--cpu-reference")
        {
            o.m_cpu = true;
        }
        else if (key == "--synthetic")
        {
            o.m_synthetic = true;
        }
        else
        {
            Require(i+1 < argc,"Missing value for "+key);
            const std::string value = argv[++i];
            if (key == "--grid")
            {
                o.m_grid = std::stoul(value);
            }
            else if (key == "--fine")
            {
                o.m_fine = std::stoul(value);
            }
            else if (key == "--samples-per-step")
            {
                o.m_samples = std::stoul(value);
            }
            else if (key == "--warm-samples")
            {
                o.m_warm = std::stoul(value);
            }
            else if (key == "--batch")
            {
                o.m_batch = std::stoul(value);
            }
            else if (key == "--amplitude")
            {
                o.m_amplitude = std::stod(value);
            }
            else if (key == "--threshold")
            {
                o.m_threshold = std::stod(value);
            }
            else if (key == "--max-steps")
            {
                o.m_maxSteps = std::stoull(value);
            }
            else if (key == "--output")
            {
                o.m_output = value;
            }
            else
            {
                throw std::runtime_error("Unknown option "+key);
            }
        }
    }

    Require(o.m_grid >= 2 && o.m_grid <= 100 && o.m_fine >= o.m_grid
        && (o.m_fine-1)%(o.m_grid-1) == 0,"Fine grid must contain the coarse grid exactly");
    Require(o.m_batch > 0 && o.m_batch <= 262144 && o.m_samples > 0,"Invalid batch/sample count");
    Require(static_cast<uint64_t>(o.m_fine)*o.m_samples+o.m_warm+100000 < 0xffffffffULL,"Sample index overflow");
    Require(o.m_amplitude > 0 && std::isfinite(o.m_amplitude) && std::isfinite(o.m_threshold),"Invalid amplitude/threshold");
    Require(o.m_plan || !o.m_output.empty(),"--output is required");
    return o;
}

struct ContinuationPlan
{
    std::vector<LC::Curve> m_curves;
    std::vector<unsigned> m_eligible;
    std::vector<LC::Tone> m_tones;
    std::vector<float> m_input;
    std::vector<float> m_quadrature;
    std::vector<float> m_feedback;
    uint64_t m_points = 0;
    std::string m_frequencyJSON;
};

ContinuationPlan Prepare(const Settings& o)
{
    ContinuationPlan plan;
    const unsigned stride = (o.m_fine-1)/(o.m_grid-1);
    for (unsigned i = 0; i < o.m_fine; ++i)
    {
        const double knob = i%stride == 0 ? static_cast<double>(i/stride)/(o.m_grid-1) : static_cast<double>(i)/(o.m_fine-1);
        plan.m_feedback.push_back(RequestedGPUFeedback(knob));
    }

    std::ostringstream frequencies;
    frequencies << std::setprecision(17) << '[';
    Options toneOptions;
    toneOptions.m_amplitude = o.m_amplitude;
    for (unsigned f = 0; f < o.m_grid; ++f)
    {
        const double target = 20*std::pow(1000,static_cast<double>(f)/(o.m_grid-1));
        const auto frequency = PrepareFrequency(target,toneOptions);
        LC::Tone tone{};
        tone.m_offset = static_cast<unsigned>(plan.m_input.size());
        tone.m_period = frequency.m_period;
        tone.m_cycles = frequency.m_cycles;
        tone.m_lag = tone.m_period/tone.m_cycles;
        Require(tone.m_lag >= 7 && o.m_samples >= 6*tone.m_period,"Dwell too short for two measurement halves");
        const double fraction = static_cast<double>(tone.m_period%tone.m_cycles)/tone.m_cycles;
        for (int j = 0; j < 16; ++j)
        {
            double weight = 1;
            for (int k = 0; k < 16; ++k)
            {
                if (k != j)
                {
                    weight *= (fraction-(k-7))/(j-k);
                }
            }

            tone.m_weights[j] = static_cast<float>(weight);
        }

        double real = 0,imag = 0;
        const double omega = 2*x_pi*tone.m_cycles/tone.m_period;
        for (int j = 0; j < 16; ++j)
        {
            real += tone.m_weights[j]*std::cos(omega*(tone.m_lag+j-7));
            imag += tone.m_weights[j]*std::sin(omega*(tone.m_lag+j-7));
        }

        frequencies << (f ? "," : "") << "{\"index\":" << f << ",\"target_hz\":" << target
            << ",\"actual_hz\":" << frequency.m_actual << ",\"period_samples\":" << tone.m_period
            << ",\"input_cycles\":" << tone.m_cycles << ",\"cycle_lag_samples\":" << static_cast<double>(tone.m_period)/tone.m_cycles
            << ",\"pure_tone_interpolation_relative_rms\":" << std::hypot(real-1,imag) << '}';
        plan.m_tones.push_back(tone);
        plan.m_input.insert(plan.m_input.end(),frequency.m_input.begin(),frequency.m_input.begin()+frequency.m_period);
        plan.m_quadrature.insert(plan.m_quadrature.end(),frequency.m_quadrature.begin(),frequency.m_quadrature.begin()+frequency.m_period);
    }

    frequencies << ']';
    plan.m_frequencyJSON = frequencies.str();
    std::vector<LC::Curve> parameters;
    for (unsigned d = 0; d < o.m_grid; ++d)
    {
        for (unsigned c = 0; c < o.m_grid; ++c)
        {
            const double drive = static_cast<double>(d)/(o.m_grid-1);
            const double cutoff = 20*std::pow(1000,static_cast<double>(c)/(o.m_grid-1));
            LC::Curve curve{};
            curve.m_base = PrepareGPUParameters(drive,0,cutoff);
            curve.m_cap = PrepareGPUParameters(drive,1,cutoff).m_feedback;
            curve.m_firstRecord = o.m_grid;
            const double slope = static_cast<double>(curve.m_base.m_gain)/curve.m_base.m_tanhGain;
            for (unsigned r = 0; r < o.m_grid; ++r)
            {
                const double feedback = std::min(plan.m_feedback[r*stride],curve.m_cap);
                if (feedback*slope*slope*slope > o.m_threshold)
                {
                    curve.m_firstRecord = r;
                    break;
                }
            }

            parameters.push_back(curve);
        }
    }

    for (unsigned f = 0; f < o.m_grid; ++f)
    {
        for (auto curve : parameters)
        {
            curve.m_tone = f;
            curve.m_id = static_cast<unsigned>(plan.m_curves.size());
            plan.m_curves.push_back(curve);
            if (curve.m_firstRecord < o.m_grid)
            {
                plan.m_eligible.push_back(curve.m_id);
                plan.m_points += o.m_grid-curve.m_firstRecord;
            }
        }
    }

    Require(!plan.m_eligible.empty(),"No eligible output points");
    return plan;
}

std::string Identity(const Settings& o)
{
    const auto directory = fs::path(__FILE__).parent_path();
    std::string source;
    for (const auto* file : {"ladder_gpu_continuation.mm","ladder_continuation_common.hpp","ladder_continuation.metal",
        "ladder_gpu_host.hpp","ladder_gpu_common.hpp","ladder_energy_grid.cpp"})
    {
        source += ReadSource(directory/file);
    }

    std::ostringstream text;
    text << std::setprecision(17) << "continuation-v1;grid=" << o.m_grid << ";fine=" << o.m_fine
        << ";samples=" << o.m_samples << ";warm=" << o.m_warm << ";amplitude=" << o.m_amplitude
        << ";threshold=" << o.m_threshold << ";cpu=" << o.m_cpu << ";synthetic=" << o.m_synthetic
        << ";source_hash=" << Hash(source.data(),source.size());
    return text.str();
}

struct ContinuationRecord
{
    uint64_t m_point;
    float m_values[16];
    uint64_t m_checksum;
};
static_assert(sizeof(ContinuationRecord) == 80);
static_assert(sizeof(LC::Result) == 64);

ContinuationRecord Pack(const Settings& o,const LC::Curve& curve,unsigned resonance,float feedback,LC::Result value)
{
    const double n1 = value.m_firstSamples,n2 = value.m_secondSamples,n = n1+n2;
    Require(n1 > 0 && n2 > 0,"Empty measurement half");
    const auto a = value.m_first,b = value.m_second;
    const double energy = static_cast<double>(a.m_energy)+b.m_energy;
    const double power = energy/n;
    const double real = 2*(static_cast<double>(a.m_real)+b.m_real)/(o.m_amplitude*n);
    const double imag = 2*(static_cast<double>(a.m_imaginary)+b.m_imaginary)/(o.m_amplitude*n);
    const double dc = (static_cast<double>(a.m_dc)+b.m_dc)/n;
    const double fundamental = (real*real+imag*imag)/2;
    const double residual = power-fundamental-dc*dc;
    Require(residual >= -2e-6*power-1e-30,"Negative residual exceeds projection tolerance");
    const double re1 = 2*a.m_real/(o.m_amplitude*n1),im1 = 2*a.m_imaginary/(o.m_amplitude*n1);
    const double re2 = 2*b.m_real/(o.m_amplitude*n2),im2 = 2*b.m_imaginary/(o.m_amplitude*n2);
    const unsigned c = curve.m_id%o.m_grid;
    const unsigned d = curve.m_id/o.m_grid%o.m_grid;
    const unsigned f = curve.m_id/(o.m_grid*o.m_grid);
    ContinuationRecord record{};
    record.m_point = ((static_cast<uint64_t>(f)*o.m_grid+d)*o.m_grid+resonance)*o.m_grid+c;
    const double values[16] = {power,fundamental,real,imag,dc,std::max(0.0,residual),
        energy > 0 ? std::sqrt(value.m_cycleError/energy) : 0,
        energy > 0 ? std::sqrt(value.m_bufferError/energy) : 0,
        value.m_stateEnergy > 0 ? std::sqrt(value.m_stateError/value.m_stateEnergy) : 0,
        std::max(a.m_peak,b.m_peak),(re1*re1+im1*im1)/2,(re2*re2+im2*im2)/2,
        a.m_energy/n1,b.m_energy/n2,std::hypot(re2-re1,im2-im1)/o.m_amplitude,feedback};
    for (unsigned i = 0; i < 16; ++i)
    {
        Require(std::isfinite(values[i]),"Nonfinite measurement");
        record.m_values[i] = static_cast<float>(values[i]);
        Require(std::isfinite(record.m_values[i]),"Measurement overflows float");
    }

    record.m_checksum = Hash(&record,sizeof(record)-sizeof(record.m_checksum));
    return record;
}

struct Runner
{
    const ContinuationPlan& m_plan;
    const Settings& m_options;
    id<MTLDevice> m_device;
    id<MTLCommandQueue> m_queue;
    id<MTLComputePipelineState> m_advance;
    id<MTLComputePipelineState> m_measure;
    std::array<id<MTLBuffer>,8> m_buffers;
    std::vector<LC::Curve> m_curves;
    std::vector<LadderGPU::History> m_cpuStates;
    std::vector<LC::Result> m_cpuResults;
    double m_gpuSeconds = 0;

    Runner(const ContinuationPlan& plan,const Settings& options,unsigned capacity)
        : m_plan(plan),m_options(options)
    {
        if (options.m_cpu)
        {
            return;
        }
        m_device = MTLCreateSystemDefaultDevice();
        Require(m_device != nil,"Metal unavailable");
        m_queue = [m_device newCommandQueue];
        const auto directory = fs::path(__FILE__).parent_path();
        const auto source = ReadSource(directory/"ladder_gpu_common.hpp")+"\n"+
            ReadSource(directory/"ladder_continuation_common.hpp")+"\n"+ReadSource(directory/"ladder_continuation.metal");
        MTLCompileOptions* compile = [MTLCompileOptions new];
        compile.mathMode = MTLMathModeSafe;
        compile.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
        NSError* error = nil;
        id<MTLLibrary> library = [m_device newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()] options:compile error:&error];
        Require(library != nil,error ? error.localizedDescription.UTF8String : "Metal source failed");
        m_advance = [m_device newComputePipelineStateWithFunction:[library newFunctionWithName:@"ContinuationAdvance"] error:&error];
        m_measure = [m_device newComputePipelineStateWithFunction:[library newFunctionWithName:@"ContinuationMeasure"] error:&error];
        Require(m_advance != nil && m_measure != nil,error ? error.localizedDescription.UTF8String : "Metal pipeline failed");
        m_buffers[0] = [m_device newBufferWithLength:capacity*sizeof(LC::Curve) options:MTLResourceStorageModeShared];
        m_buffers[1] = [m_device newBufferWithLength:capacity*sizeof(LadderGPU::History) options:MTLResourceStorageModeShared];
        m_buffers[2] = [m_device newBufferWithBytes:plan.m_tones.data() length:plan.m_tones.size()*sizeof(LC::Tone) options:MTLResourceStorageModeShared];
        m_buffers[3] = [m_device newBufferWithBytes:plan.m_input.data() length:plan.m_input.size()*sizeof(float) options:MTLResourceStorageModeShared];
        m_buffers[4] = [m_device newBufferWithBytes:plan.m_feedback.data() length:plan.m_feedback.size()*sizeof(float) options:MTLResourceStorageModeShared];
        m_buffers[6] = [m_device newBufferWithBytes:plan.m_quadrature.data() length:plan.m_quadrature.size()*sizeof(float) options:MTLResourceStorageModeShared];
        m_buffers[7] = [m_device newBufferWithLength:capacity*sizeof(LC::Result) options:MTLResourceStorageModeShared];
        for (unsigned i : {0,1,2,3,4,6,7}) { Require(m_buffers[i] != nil,"Metal allocation failed"); }
    }

    void Load(const std::vector<unsigned>& ids,const std::vector<LadderGPU::History>& states)
    {
        Require(ids.size() == states.size() && !ids.empty(),"Invalid active checkpoint");
        m_curves.clear();
        for (auto id : ids)
        {
            m_curves.push_back(m_plan.m_curves.at(id));
        }
        if (m_options.m_cpu)
        {
            m_cpuStates = states;
            m_cpuResults.resize(ids.size());
        }
        else
        {
            std::memcpy(m_buffers[0].contents,m_curves.data(),m_curves.size()*sizeof(m_curves[0]));
            std::memcpy(m_buffers[1].contents,states.data(),states.size()*sizeof(states[0]));
        }
    }

    void Run(LC::Dispatch dispatch,bool measure)
    {
        Require(dispatch.m_count == m_curves.size(),"Dispatch count mismatch");
        if (m_options.m_cpu)
        {
            for (size_t i = 0; i < m_curves.size(); ++i)
            {
                const auto curve = m_curves[i];
                auto p = curve.m_base;
                p.m_feedback = std::min(m_plan.m_feedback[dispatch.m_level],curve.m_cap);
                const auto tone = m_plan.m_tones[curve.m_tone];
                if (measure && dispatch.m_level/dispatch.m_stride >= curve.m_firstRecord)
                {
                    m_cpuResults[i] = LC::Measure(m_cpuStates[i],p,tone,m_plan.m_input.data(),m_plan.m_quadrature.data(),dispatch);
                }
                else
                {
                    LC::Advance(m_cpuStates[i],p,tone,m_plan.m_input.data(),dispatch.m_begin,dispatch.m_samples);
                }
            }

            return;
        }

        @autoreleasepool
        {
            id<MTLCommandBuffer> command = [m_queue commandBuffer];
            id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
            [encoder setComputePipelineState:measure ? m_measure : m_advance];
            for (unsigned i : {0,1,2,3,4}) { [encoder setBuffer:m_buffers[i] offset:0 atIndex:i]; }
            [encoder setBytes:&dispatch length:sizeof(dispatch) atIndex:5];
            if (measure)
            {
                [encoder setBuffer:m_buffers[6] offset:0 atIndex:6];
                [encoder setBuffer:m_buffers[7] offset:0 atIndex:7];
            }

            [encoder dispatchThreads:MTLSizeMake(dispatch.m_count,1,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
            [encoder endEncoding];
            [command commit];
            [command waitUntilCompleted];
            Require(command.status == MTLCommandBufferStatusCompleted,command.error ? command.error.localizedDescription.UTF8String : "GPU continuation failed");
            m_gpuSeconds += command.GPUEndTime-command.GPUStartTime;
        }
    }

    std::vector<LadderGPU::History> States() const
    {
        if (m_options.m_cpu)
        {
            return m_cpuStates;
        }
        const auto* first = static_cast<const LadderGPU::History*>(m_buffers[1].contents);
        return {first,first+m_curves.size()};
    }

    const LC::Result* Results() const
    {
        return m_options.m_cpu ? m_cpuResults.data() : static_cast<const LC::Result*>(m_buffers[7].contents);
    }
};

struct CheckHeader
{
    uint64_t m_magic;
    uint64_t m_identity;
    uint64_t m_cursor;
    uint64_t m_nextLevel;
    uint64_t m_points;
    uint64_t m_completed;
    uint64_t m_updates;
    uint64_t m_count;
    uint64_t m_outputBytes;
};

struct Checkpoint
{
    CheckHeader m_header{};
    std::vector<unsigned> m_ids;
    std::vector<LadderGPU::History> m_states;
};

void SaveCheckpoint(const fs::path& directory,Checkpoint& c,int output)
{
    Require(fsync(output) == 0,"Cannot sync output records");
    c.m_header.m_count = c.m_ids.size();
    Require(c.m_ids.size() == c.m_states.size(),"Checkpoint state count mismatch");
    c.m_header.m_outputBytes = sizeof(Header)+c.m_header.m_points*sizeof(ContinuationRecord);
    std::vector<unsigned char> bytes(sizeof(CheckHeader)+c.m_ids.size()*sizeof(unsigned)+c.m_states.size()*sizeof(LadderGPU::History));
    std::memcpy(bytes.data(),&c.m_header,sizeof(c.m_header));
    if (!c.m_ids.empty())
    {
        std::memcpy(bytes.data()+sizeof(c.m_header),c.m_ids.data(),c.m_ids.size()*sizeof(unsigned));
        std::memcpy(bytes.data()+sizeof(c.m_header)+c.m_ids.size()*sizeof(unsigned),c.m_states.data(),c.m_states.size()*sizeof(LadderGPU::History));
    }

    const uint64_t checksum = Hash(bytes.data(),bytes.size());
    const auto temporary = directory/"checkpoint.tmp";
    const int file = open(temporary.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
    Require(file >= 0,"Cannot create checkpoint");
    WriteAll(file,bytes.data(),bytes.size());
    WriteAll(file,&checksum,sizeof(checksum));
    Require(fsync(file) == 0,"Cannot sync checkpoint");
    close(file);
    fs::rename(temporary,directory/"checkpoint.bin");
    const int folder = open(directory.c_str(),O_RDONLY);
    Require(folder >= 0 && fsync(folder) == 0,"Cannot sync checkpoint directory");
    close(folder);
}

Checkpoint LoadCheckpoint(const fs::path& directory,uint64_t identity,const Settings& o,const ContinuationPlan& plan)
{
    Checkpoint c;
    const auto path = directory/"checkpoint.bin";
    if (!fs::exists(path))
    {
        c.m_header = {x_checkMagic,identity,0,0,0,0,0,0,sizeof(Header)};
        Require(!fs::exists(directory/"points.bin") || fs::file_size(directory/"points.bin") == sizeof(Header),"Output exists without a committed checkpoint");
        return c;
    }

    const auto bytes = ReadSource(path);
    Require(bytes.size() >= sizeof(CheckHeader)+8,"Truncated checkpoint");
    uint64_t checksum;
    std::memcpy(&checksum,bytes.data()+bytes.size()-8,8);
    Require(checksum == Hash(bytes.data(),bytes.size()-8),"Corrupt checkpoint");
    std::memcpy(&c.m_header,bytes.data(),sizeof(c.m_header));
    const auto& h = c.m_header;
    Require(h.m_magic == x_checkMagic && h.m_identity == identity,"Checkpoint configuration/source mismatch");
    Require(h.m_count <= 262144 && h.m_cursor <= plan.m_eligible.size() && h.m_completed <= h.m_cursor
        && h.m_nextLevel < o.m_fine && h.m_points <= plan.m_points
        && h.m_cursor == h.m_completed+h.m_count && (h.m_count != 0 || h.m_nextLevel == 0)
        && bytes.size() == sizeof(CheckHeader)+h.m_count*(sizeof(unsigned)+sizeof(LadderGPU::History))+8,"Invalid checkpoint bounds");
    c.m_ids.resize(h.m_count);c.m_states.resize(h.m_count);
    if (h.m_count)
    {
        std::memcpy(c.m_ids.data(),bytes.data()+sizeof(CheckHeader),h.m_count*sizeof(unsigned));
        std::memcpy(c.m_states.data(),bytes.data()+sizeof(CheckHeader)+h.m_count*sizeof(unsigned),h.m_count*sizeof(LadderGPU::History));
        for (auto id : c.m_ids)
        {
            Require(id < plan.m_curves.size(),"Invalid checkpoint curve");
        }
    }

    Require(h.m_outputBytes == sizeof(Header)+h.m_points*sizeof(ContinuationRecord)
        && fs::exists(directory/"points.bin") && fs::file_size(directory/"points.bin") >= h.m_outputBytes,"Missing committed output");
    if (fs::file_size(directory/"points.bin") > h.m_outputBytes)
    {
        fs::resize_file(directory/"points.bin",h.m_outputBytes);
        std::cout << "{\"rolled_back_uncommitted_tail\":true}\n";
    }

    std::ifstream stream(directory/"points.bin",std::ios::binary);
    Header header;
    stream.read(reinterpret_cast<char*>(&header),sizeof(header));
    Require(header.m_magic == x_contMagic && header.m_identity == identity && header.m_recordBytes == sizeof(ContinuationRecord),"Output header mismatch");
    const uint64_t gridPoints = static_cast<uint64_t>(o.m_grid)*o.m_grid*o.m_grid*o.m_grid;
    std::vector<unsigned char> seen(gridPoints,0);
    for (uint64_t i = 0; i < h.m_points; ++i)
    {
        ContinuationRecord record;
        stream.read(reinterpret_cast<char*>(&record),sizeof(record));
        Require(stream.good() && record.m_point < gridPoints && !seen[record.m_point]
            && record.m_checksum == Hash(&record,sizeof(record)-8),"Corrupt or duplicate committed output");
        seen[record.m_point] = 1;
    }

    return c;
}

void Benchmark(const Settings& o,const ContinuationPlan& plan)
{
    fs::create_directories(o.m_output);
    std::vector<unsigned> ids;
    const size_t count = std::min(static_cast<size_t>(o.m_batch),plan.m_eligible.size());
    for (size_t i = 0; i < count; ++i)
    {
        ids.push_back(plan.m_eligible[i*plan.m_eligible.size()/count]);
    }
    Runner runner(plan,o,static_cast<unsigned>(count));
    runner.Load(ids,std::vector<LadderGPU::History>(count));
    const unsigned stride = (o.m_fine-1)/(o.m_grid-1);
    std::ostringstream result;
    result << std::setprecision(17) << "{\"curves\":" << count << ",\"profiles\":[";
    bool first = true;
    for (unsigned level : {0u,(o.m_grid/2)*stride,o.m_fine-1})
    {
        for (bool measure : {false,true})
        {
            const auto begin = Clock::now();
            for (unsigned repeat = 0; repeat < 4; ++repeat)
            {
                runner.Run({static_cast<unsigned>(count),level,repeat*o.m_samples,o.m_samples,stride,0,static_cast<float>(1/o.m_amplitude)},measure);
            }

            const double seconds = std::chrono::duration<double>(Clock::now()-begin).count();
            result << (first ? "" : ",") << "{\"level\":" << level << ",\"measure\":" << (measure ? "true" : "false")
                << ",\"seconds\":" << seconds << ",\"scalar_samples_per_second\":" << 4.0*count*o.m_samples/seconds << '}';
            first = false;
        }
    }

    result << "]}";
    WriteText(o.m_output/"benchmark.json",result.str());
    std::cout << result.str() << '\n';
}

void RunContinuation(const Settings& o,const ContinuationPlan& plan)
{
    fs::create_directories(o.m_output);
    const int lock = open((o.m_output/"run.lock").c_str(),O_WRONLY|O_CREAT,0644);
    Require(lock >= 0 && flock(lock,LOCK_EX|LOCK_NB) == 0,"Another process owns this run");
    const auto identity = Identity(o);
    const uint64_t identityHash = Hash(identity.data(),identity.size());
    auto checkpoint = LoadCheckpoint(o.m_output,identityHash,o,plan);
    auto& h = checkpoint.m_header;
    Require(fs::space(o.m_output).available > (plan.m_points-h.m_points)*sizeof(ContinuationRecord)+x_reserve,"Insufficient disk reserve");
    const int output = open((o.m_output/"points.bin").c_str(),O_WRONLY|O_CREAT|O_APPEND,0644);
    Require(output >= 0,"Cannot open point output");
    if (fs::file_size(o.m_output/"points.bin") == 0)
    {
        const Header header{x_contMagic,identityHash,sizeof(ContinuationRecord)};
        WriteAll(output,&header,sizeof(header));
    }
    else
    {
        Header header;
        std::ifstream existing(o.m_output/"points.bin",std::ios::binary);
        existing.read(reinterpret_cast<char*>(&header),sizeof(header));
        Require(existing.good() && header.m_magic == x_contMagic && header.m_identity == identityHash
            && header.m_recordBytes == sizeof(ContinuationRecord),"Existing output header mismatch");
    }

    std::ostringstream manifest;
    manifest << std::setprecision(17) << "{\"format\":\"resonance-continuation-v1\",\"identity\":\"" << identity
        << "\",\"grid\":" << o.m_grid << ",\"fine_positions\":" << o.m_fine << ",\"samples_per_step\":" << o.m_samples
        << ",\"warm_samples\":" << o.m_warm << ",\"sample_rate\":192000,\"amplitude\":" << o.m_amplitude
        << ",\"threshold\":" << o.m_threshold << ",\"curves\":" << plan.m_eligible.size() << ",\"points\":" << plan.m_points
        << ",\"record_bytes\":" << sizeof(ContinuationRecord) << ",\"header_bytes\":" << sizeof(Header)
        << ",\"initial_state\":\"reset, target drive and cutoff, zero feedback; no kicks\""
        << ",\"measurement\":\"last half of each recorded fine dwell, rounded inward to complete input buffers\""
        << ",\"periodicity\":\"relative RMS output mismatch at one fractional input cycle (16-tap Lagrange lookahead), exact digital-buffer output mismatch, and exact buffer state mismatch\""
        << ",\"fields\":[\"total_power\",\"fundamental_power\",\"fundamental_real\",\"fundamental_imaginary\",\"dc\",\"residual_power\",\"cycle_repeat_rms\",\"buffer_repeat_rms\",\"state_repeat_rms\",\"peak\",\"fundamental_first\",\"fundamental_second\",\"total_first\",\"total_second\",\"coherent_drift_over_input\",\"feedback\"]"
        << ",\"frequencies\":" << plan.m_frequencyJSON << '}';
    WriteText(o.m_output/"manifest.json",manifest.str());
    SaveCheckpoint(o.m_output,checkpoint,output);
    Runner runner(plan,o,std::max(o.m_batch,static_cast<unsigned>(checkpoint.m_ids.size())));
    if (!o.m_cpu)
    {
        WriteText(o.m_output/"device.txt",runner.m_device.name.UTF8String);
    }
    std::signal(SIGTERM,Stop);std::signal(SIGINT,Stop);
    uint64_t permutation = 104729;
    while (std::gcd(permutation,static_cast<uint64_t>(plan.m_eligible.size())) != 1)
    {
        permutation += 2;
    }
    const unsigned stride = (o.m_fine-1)/(o.m_grid-1);
    const auto started = Clock::now();
    const uint64_t previousUpdates = h.m_updates;
    uint64_t steps = 0;
    const uint64_t totalUpdates = static_cast<uint64_t>(plan.m_eligible.size())*(o.m_warm+static_cast<uint64_t>(o.m_fine)*o.m_samples);
    auto progress = [&](const char* status)
    {
        const double seconds = std::chrono::duration<double>(Clock::now()-started).count();
        const double rate = seconds > 0 ? (h.m_updates-previousUpdates)/seconds : 0;
        std::ostringstream text;
        text << std::setprecision(12) << "{\"status\":\"" << status << "\",\"completed_curves\":" << h.m_completed
            << ",\"total_curves\":" << plan.m_eligible.size() << ",\"completed_points\":" << h.m_points << ",\"total_points\":" << plan.m_points
            << ",\"active_curves\":" << checkpoint.m_ids.size() << ",\"next_fine_position\":" << h.m_nextLevel
            << ",\"scalar_updates\":" << h.m_updates << ",\"total_scalar_updates\":" << totalUpdates
            << ",\"elapsed_seconds\":" << seconds << ",\"scalar_samples_per_second\":" << rate << ",\"eta_seconds\":";
        if (rate > 0)
        {
            text << (totalUpdates-h.m_updates)/rate;
        }
        else
        {
            text << "null";
        }
        text << '}';
        WriteText(o.m_output/"progress.json",text.str());
        std::cout << text.str() << std::endl;
    };
    progress("running");
    try
    {
        while (h.m_completed < plan.m_eligible.size() && !g_stop.load() && steps < o.m_maxSteps)
        {
            if (checkpoint.m_ids.empty())
            {
                const uint64_t end = std::min(static_cast<uint64_t>(plan.m_eligible.size()),h.m_cursor+o.m_batch);
                for (;h.m_cursor < end;++h.m_cursor)
                {
                    checkpoint.m_ids.push_back(plan.m_eligible[(h.m_cursor*permutation+17)%plan.m_eligible.size()]);
                }

                std::sort(checkpoint.m_ids.begin(),checkpoint.m_ids.end());
                checkpoint.m_states.assign(checkpoint.m_ids.size(),{});
                runner.Load(checkpoint.m_ids,checkpoint.m_states);
                runner.Run({static_cast<unsigned>(checkpoint.m_ids.size()),0,0,o.m_warm,stride,0,static_cast<float>(1/o.m_amplitude)},false);
                h.m_updates += checkpoint.m_ids.size()*static_cast<uint64_t>(o.m_warm);
                h.m_nextLevel = 0;
            }
            else
            {
                runner.Load(checkpoint.m_ids,checkpoint.m_states);
            }

            while (h.m_nextLevel < o.m_fine && !g_stop.load() && steps < o.m_maxSteps)
            {
                const unsigned level = static_cast<unsigned>(h.m_nextLevel);
                const bool measure = level%stride == 0;
                runner.Run({static_cast<unsigned>(checkpoint.m_ids.size()),level,o.m_warm+level*o.m_samples,o.m_samples,stride,
                    o.m_synthetic ? 1u : 0u,static_cast<float>(1/o.m_amplitude)},measure);
                h.m_updates += checkpoint.m_ids.size()*static_cast<uint64_t>(o.m_samples);
                if (measure)
                {
                    const auto* results = runner.Results();
                    std::vector<ContinuationRecord> records;
                    for (size_t i = 0; i < runner.m_curves.size(); ++i)
                    {
                        const auto curve = runner.m_curves[i];
                        if (level/stride >= curve.m_firstRecord)
                        {
                            records.push_back(Pack(o,curve,level/stride,std::min(plan.m_feedback[level],curve.m_cap),results[i]));
                        }
                    }

                    if (!records.empty())

                    {

                        WriteAll(output,records.data(),records.size()*sizeof(records[0]));

                    }
                    h.m_points += records.size();
                }

                ++h.m_nextLevel;++steps;
                if (measure && h.m_nextLevel < o.m_fine)
                {
                    checkpoint.m_states = runner.States();
                    SaveCheckpoint(o.m_output,checkpoint,output);
                    Require(fs::space(o.m_output).available > x_reserve,"Low disk reserve");
                    progress("running");
                }
            }

            if (h.m_nextLevel == o.m_fine)
            {
                h.m_completed += checkpoint.m_ids.size();
                checkpoint.m_ids.clear();checkpoint.m_states.clear();h.m_nextLevel = 0;
            }
            else
            {
                checkpoint.m_states = runner.States();
            }
            SaveCheckpoint(o.m_output,checkpoint,output);
        }

        progress(h.m_completed == plan.m_eligible.size() ? "complete" : "stopped");
    }
    catch (...)
    {
        progress("failed");
        throw;
    }

    Require(h.m_completed != plan.m_eligible.size() || h.m_points == plan.m_points,"Completed point count mismatch");
    close(output);close(lock);
}
}

int main(int argc,char** argv)
{
    @autoreleasepool
    {
        try
        {
            const auto o = ParseSettings(argc,argv);
            const auto plan = Prepare(o);
            if (o.m_plan)
            {
                std::cout << "{\"curves\":" << plan.m_eligible.size() << ",\"points\":" << plan.m_points
                    << ",\"bytes\":" << sizeof(Header)+plan.m_points*sizeof(ContinuationRecord)
                    << ",\"scalar_updates\":" << plan.m_eligible.size()*(static_cast<uint64_t>(o.m_fine)*o.m_samples+o.m_warm) << "}\n";
            }
            else if (o.m_benchmark)
            {
                Benchmark(o,plan);
            }
            else
            {
                RunContinuation(o,plan);
            }
            return 0;
        }
        catch (const std::exception& error)
        {
            std::cerr << "ladder-gpu-continuation: " << error.what() << '\n';
            return 1;
        }
    }
}

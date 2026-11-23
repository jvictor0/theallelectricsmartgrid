// Resumable production-style ladder survey. The audio app is not modified.
// Format 3 identifies float/rational-tanh measurements; format 2 stays separate.
//
#include "ladder_gpu_host.hpp"
#include <memory>

namespace
{
constexpr uint64_t x_gpuFormat = 3;
constexpr uint64_t x_gpuMagic = 0x334752454444414cULL;

struct SurveyOptions
{
    Options m_base;
    size_t m_batchJobs = 160;
    bool m_cpuReference = false;
};

SurveyOptions ParseSurvey(int argc, char** argv)
{
    SurveyOptions result;
    std::vector<char*> arguments{argv[0]};
    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argv[i];
        if (key == "--cpu-reference")
        {
            result.m_cpuReference = true;
        }
        else if (key == "--batch-jobs")
        {
            Require(i+1 < argc, "Missing batch size");
            result.m_batchJobs = std::stoull(argv[++i]);
        }
        else
        {
            arguments.push_back(argv[i]);
        }
    }

    result.m_base = Parse(static_cast<int>(arguments.size()),arguments.data());
    Require(result.m_batchJobs > 0 && result.m_batchJobs <= 4096
        && result.m_batchJobs*4*result.m_base.m_starts <= 262144, "Batch must contain 1..262144 trajectories");
    return result;
}

struct GPUPlan
{
    WorkPlan m_jobs;
    std::vector<std::array<LadderGPU::Parameters,4>> m_parameters;
};

GPUPlan PlanGPU(const Options& options)
{
    GPUPlan result{{options.m_grid,{}},{}};
    std::vector<double> cutoffs;
    for (int c = 0; c < options.m_grid; ++c)
    {
        cutoffs.push_back(20*std::pow(1000,static_cast<double>(c)/(options.m_grid-1)));
    }

    for (int d = 0; d < options.m_grid; ++d)
    {
        for (int r = 0; r < options.m_grid; ++r)
        {
            for (int c = 0; c < options.m_grid; c += 4)
            {
                std::array<LadderGPU::Parameters,4> parameters;
                int eligible = 0;
                for (int lane = 0; lane < 4; ++lane)
                {
                    const auto p = PrepareGPUParameters(static_cast<double>(d)/(options.m_grid-1),
                        static_cast<double>(r)/(options.m_grid-1),cutoffs[c+lane]);
                    parameters[lane] = p;
                    const double slope = static_cast<double>(p.m_gain)/p.m_tanhGain;
                    eligible += static_cast<double>(p.m_feedback)*slope*slope*slope > options.m_minEffectiveResonance;
                }

                Require(eligible == 0 || eligible == 4, "Threshold splits a four-cutoff record; lane repacking is required");
                if (eligible == 4)
                {
                    result.m_jobs.m_parameterJobs.push_back(static_cast<uint32_t>((d*options.m_grid+r)*options.m_grid/4+c/4));
                    result.m_parameters.push_back(parameters);
                }
            }
        }
    }

    return result;
}

std::string SurveyIdentity(const SurveyOptions& args)
{
    const auto& o = args.m_base;
    const auto directory = fs::path(__FILE__).parent_path();
    std::string source;
    for (const auto* file : {"ladder_gpu_grid.mm","ladder_gpu_host.hpp","ladder_gpu_common.hpp",
        "ladder_gpu.metal","ladder_energy_grid.cpp"})
    {
        source += ReadSource(directory/file);
    }

    std::ostringstream result;
    result << std::setprecision(17) << "format=3;model=production-rational-detector-off-float-fma-v1"
        << ";execution=" << (args.m_cpuReference ? "scalar-cpu-reference" : "metal-safe-precise")
        << ";grid=" << o.m_grid << ";starts=" << o.m_starts << ";samples=" << o.m_samples
        << ";sample_rate=" << o.m_sampleRate << ";amplitude=" << o.m_amplitude << ";seed=" << o.m_seed
        << ";frequency_range=20,20000;cutoff_range=20,20000;frequency_tolerance=0.001"
        << ";min_effective_resonance=" << o.m_minEffectiveResonance
        << ";source_hash=" << Hash(source.data(),source.size());
    return result.str();
}

Batch MakeBatch(const SurveyOptions& args, const GPUPlan& plan, const Frequency& frequency,
    const std::vector<size_t>& parameters, int frequencyIndex, std::vector<uint64_t>& jobs)
{
    const auto& o = args.m_base;
    Batch batch;
    batch.m_frequency = frequency;
    batch.m_input.assign(frequency.m_input.begin(),frequency.m_input.end());
    batch.m_quadrature.assign(frequency.m_quadrature.begin(),frequency.m_quadrature.end());
    const auto count = parameters.size()*4*o.m_starts;
    batch.m_parameters.reserve(count);
    batch.m_initial.reserve(count);
    jobs.clear();
    for (const auto index : parameters)
    {
        const uint64_t job = static_cast<uint64_t>(frequencyIndex)*o.m_grid*o.m_grid*o.m_grid/4
            + plan.m_jobs.m_parameterJobs[index];
        jobs.push_back(job);
        for (int lane = 0; lane < 4; ++lane)
        {
            const auto p = plan.m_parameters[index][lane];
            for (int start = 0; start < o.m_starts; ++start)
            {
                batch.m_parameters.push_back(p);
                batch.m_initial.push_back(PrepareGPUHistory(p,job*4+lane,start,o.m_seed,o.m_amplitude));
            }
        }
    }

    return batch;
}

std::vector<LadderGPU::Partial> CPUReference(const Batch& batch)
{
    const size_t count = batch.m_initial.size();
    std::vector<LadderGPU::Partial> result(count*batch.m_frequency.m_segments.size());
    for (size_t i = 0; i < count; ++i)
    {
        auto state = batch.m_initial[i];
        for (size_t j = 0; j < batch.m_frequency.m_segments.size(); ++j)
        {
            const auto segment = batch.m_frequency.m_segments[j];
            LadderGPU::Measurement measured;
            for (int n = segment.m_begin; n < segment.m_end; ++n)
            {
                const float y = LadderGPU::Step(state,batch.m_parameters[i],batch.m_input[n]);
                measured.Add(y,batch.m_input[n],batch.m_quadrature[n]);
            }

            result[j*count+i] = measured.Get();
        }
    }

    return result;
}

Record SummarizeJob(size_t jobIndex, uint64_t job, const Batch& batch, const Options& options,
    const LadderGPU::Partial* partials)
{
    std::array<PointAccumulator,4> accumulators;
    const auto& tone = batch.m_frequency;
    const size_t count = batch.m_initial.size();
    for (int trial = 0; trial < options.m_starts; ++trial)
    {
        Vector full = Splat(0);
        Vector tail = Splat(0);
        std::array<SignalSums,x_windows> windows;
        std::array<SignalSums,x_scopes> scopes;
        for (size_t j = 0; j < tone.m_segments.size(); ++j)
        {
            const auto segment = tone.m_segments[j];
            SignalSums sums;
            for (int lane = 0; lane < 4; ++lane)
            {
                const auto p = partials[j*count+(jobIndex*4+lane)*options.m_starts+trial];
                sums.m_energy[lane] = p.m_energy;
                sums.m_real[lane] = p.m_real;
                sums.m_imaginary[lane] = p.m_imaginary;
                sums.m_sum[lane] = p.m_dc;
                sums.m_peak[lane] = p.m_peak;
            }

            full += sums.m_energy;
            if (segment.m_begin >= options.m_samples/2)
            {
                tail += sums.m_energy;
            }

            windows[segment.m_window].Add(sums);
            for (int s = 0; s < x_scopes; ++s)
            {
                if (segment.m_scopeMask & (1u << s))
                {
                    scopes[s].Add(sums);
                }
            }
        }

        for (int lane = 0; lane < 4; ++lane)
        {
            AccumulateTrajectory(accumulators[lane],options,tone,full[lane],tail[lane],windows,scopes,lane,2e-6);
        }
    }

    return PackRecord(job,tone,accumulators);
}

std::vector<Record> SummarizeBatch(const Batch& batch, const Options& options,
    const std::vector<uint64_t>& jobs, const LadderGPU::Partial* partials)
{
    std::vector<Record> records(jobs.size());
    std::atomic<size_t> next{0};
    std::vector<std::thread> workers;
    std::exception_ptr failure;
    std::mutex mutex;
    for (size_t worker = 0; worker < std::min<size_t>(options.m_threads,jobs.size()); ++worker)
    {
        workers.emplace_back([&]
        {
            try
            {
                while (true)
                {
                    const auto i = next.fetch_add(1);
                    if (i >= jobs.size())
                    {
                        break;
                    }

                    records[i] = SummarizeJob(i,jobs[i],batch,options,partials);
                }
            }
            catch (...)
            {
                std::lock_guard<std::mutex> guard(mutex);
                failure = std::current_exception();
            }
        });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    if (failure)
    {
        std::rethrow_exception(failure);
    }

    return records;
}

void RunSurvey(const SurveyOptions& args, const GPUPlan& plan)
{
    const auto& o = args.m_base;
    Require(plan.m_jobs.Jobs() > 0, "No selected configurations");
    fs::create_directories(o.m_output);
    const int lock = open((o.m_output/"run.lock").c_str(),O_WRONLY|O_CREAT,0644);
    Require(lock >= 0 && flock(lock,LOCK_EX|LOCK_NB) == 0, "Another process owns this output directory");
    const auto identity = SurveyIdentity(args);
    const Header header{x_gpuMagic,Hash(identity.data(),identity.size()),sizeof(Record)};
    auto completed = Scan(o.m_output,header,o.Jobs());
    const uint64_t previous = std::count(completed.begin(),completed.end(),static_cast<unsigned char>(1));
    Require(previous <= plan.m_jobs.Jobs(), "More completed records than selected jobs");
    const uint64_t remainingBytes = (plan.m_jobs.Jobs()-previous)*sizeof(Record);
    Require(fs::space(o.m_output).available > remainingBytes+10ULL*1024*1024*1024,
        "Insufficient disk for remaining summaries plus 10 GiB reserve");
    std::vector<Frequency> frequencies;
    for (int f = 0; f < o.m_grid; ++f)
    {
        frequencies.push_back(PrepareFrequency(20*std::pow(1000,static_cast<double>(f)/(o.m_grid-1)),o));
    }

    Manifest(o,frequencies,plan.m_jobs,{x_gpuFormat,identity,"production-rational-detector-off-float-fma-v1",
        "strict k*(g/R(g))^3 > threshold; k after rational-normalized kSafe; evaluate threshold in double from float coefficients",
        "independent uniform histories in [-A-kC,A+kC] x [-C,C]^3, C=1/R(g); SplitMix64 keys, histories rounded to float",
        "float recurrence and compensated segment sums; double aggregation; float32 saved summaries"});
    std::cout << PlanJSON(o,plan.m_jobs) << std::endl;
    OutputFile output(o.m_output/"worker-000.bin",header);
    std::signal(SIGTERM,Stop);
    std::signal(SIGINT,Stop);
    const auto started = Clock::now();
    auto lastProgress = started;
    uint64_t finished = 0;
    double gpuSeconds = 0;
    auto progress = [&](bool final)
    {
        const double seconds = std::chrono::duration<double>(Clock::now()-started).count();
        const double rate = seconds > 0 ? finished/seconds : 0;
        std::ostringstream json;
        json << std::setprecision(12) << "{\"status\":\""
            << (previous+finished == plan.m_jobs.Jobs() ? "complete" : (final ? "stopped" : "running"))
            << "\",\"completed_jobs\":" << previous+finished << ",\"total_jobs\":" << plan.m_jobs.Jobs()
            << ",\"completed_points\":" << (previous+finished)*4
            << ",\"completed_trajectories\":" << (previous+finished)*4*o.m_starts
            << ",\"jobs_this_process\":" << finished << ",\"elapsed_seconds\":" << seconds
            << ",\"jobs_per_second\":" << rate
            << ",\"scalar_samples_per_second\":" << rate*4*o.m_starts*o.m_samples
            << ",\"gpu_seconds\":" << gpuSeconds << ",\"eta_seconds\":";
        if (rate > 0)
        {
            json << (plan.m_jobs.Jobs()-previous-finished)/rate;
        }
        else
        {
            json << "null";
        }

        json << ",\"starts\":" << o.m_starts << ",\"batch_jobs\":" << args.m_batchJobs
            << ",\"postprocess_threads\":" << o.m_threads << '}';
        WriteText(o.m_output/"progress.json",json.str());
        std::cout << json.str() << std::endl;
        lastProgress = Clock::now();
    };
    progress(false);
    const size_t perFrequency = plan.m_jobs.m_parameterJobs.size();
    uint64_t parameterStride = 104729;
    while (std::gcd(parameterStride,static_cast<uint64_t>(perFrequency)) != 1)
    {
        parameterStride += 2;
    }

    int frequencyStride = 37;
    while (std::gcd(frequencyStride,o.m_grid) != 1)
    {
        frequencyStride += 2;
    }

    std::unique_ptr<MetalRunner> metal;
    if (!args.m_cpuReference && previous < plan.m_jobs.Jobs() && o.m_maxJobs > 0)
    {
        Batch capacity;
        capacity.m_frequency = frequencies[0];
        capacity.m_input.resize(o.m_samples);
        capacity.m_quadrature.resize(o.m_samples);
        capacity.m_parameters.resize(args.m_batchJobs*4*o.m_starts);
        capacity.m_initial.resize(capacity.m_parameters.size());
        metal = std::make_unique<MetalRunner>(capacity);
        WriteText(o.m_output/"device.txt",metal->m_device.name.UTF8String);
    }

    try
    {
        for (size_t begin = 0; begin < perFrequency && !g_stop.load() && finished < o.m_maxJobs; begin += args.m_batchJobs)
        {
            for (int f = 0; f < o.m_grid && !g_stop.load() && finished < o.m_maxJobs; ++f)
            {
                const int frequency = (f*frequencyStride+17)%o.m_grid;
                std::vector<size_t> parameters;
                for (size_t i = begin; i < std::min(perFrequency,begin+args.m_batchJobs); ++i)
                {
                    const size_t index = (i*parameterStride+17)%perFrequency;
                    const auto job = static_cast<uint64_t>(frequency)*o.m_grid*o.m_grid*o.m_grid/4
                        + plan.m_jobs.m_parameterJobs[index];
                    if (!completed[job] && parameters.size() < o.m_maxJobs-finished)
                    {
                        parameters.push_back(index);
                    }
                }

                if (parameters.empty())
                {
                    continue;
                }

                Require(fs::space(o.m_output).available > 10ULL*1024*1024*1024, "Less than 10 GiB free");
                std::vector<uint64_t> jobs;
                const auto batch = MakeBatch(args,plan,frequencies[frequency],parameters,frequency,jobs);
                std::vector<LadderGPU::Partial> reference;
                const LadderGPU::Partial* partials;
                if (args.m_cpuReference)
                {
                    reference = CPUReference(batch);
                    partials = reference.data();
                }
                else
                {
                    metal->Run(batch,false,false);
                    gpuSeconds += metal->m_gpuSeconds;
                    partials = static_cast<const LadderGPU::Partial*>(metal->m_partials.contents);
                }

                const auto records = SummarizeBatch(batch,o,jobs,partials);
                output.Append(records);
                for (const auto job : jobs)
                {
                    Require(!completed[job], "Repeated job in schedule");
                    completed[job] = 1;
                }

                finished += jobs.size();
                if (Clock::now()-lastProgress > std::chrono::seconds(5))
                {
                    progress(false);
                }
            }
        }
    }
    catch (...)
    {
        progress(true);
        throw;
    }

    Require(g_stop.load() || finished == o.m_maxJobs || previous+finished == plan.m_jobs.Jobs(), "Schedule ended before covering selected jobs");
    progress(true);
    close(lock);
}
}

int main(int argc,char** argv)
{
    @autoreleasepool
    {
        try
        {
            const auto options = ParseSurvey(argc,argv);
            const auto plan = PlanGPU(options.m_base);
            if (options.m_base.m_planOnly)
            {
                std::cout << PlanJSON(options.m_base,plan.m_jobs) << std::endl;
            }
            else
            {
                Require(!options.m_base.m_selfTest,"Use ladder-gpu-bench --validate and the survey validation script");
                RunSurvey(options,plan);
            }

            return 0;
        }
        catch (const std::exception& error)
        {
            std::cerr << "ladder-gpu-grid: " << error.what() << '\n';
            return 1;
        }
    }
}

// Standalone Metal benchmark; no app DSP or CPU survey records are modified.
// Reuse the survey's grid, frequency preparation, RNG and measurement definitions.
//
#include "ladder_gpu_host.hpp"

namespace
{
using FloatVector = simd_float4;

struct BenchmarkOptions
{
    int m_trajectories = 16384;
    int m_repeats = 3;
    int m_threads = 28;
    int m_frequency = 50;
    bool m_validate = false;
    bool m_cpu = false;
    bool m_compareDouble = false;
    fs::path m_output;
};

BenchmarkOptions ParseBenchmark(int argc, char** argv)
{
    BenchmarkOptions result;
    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argv[i];
        if (key == "--validate")
        {
            result.m_validate = true;
        }
        else if (key == "--cpu")
        {
            result.m_cpu = true;
        }
        else if (key == "--compare-double")
        {
            result.m_compareDouble = true;
        }
        else
        {
            Require(i+1 < argc, "Missing argument for " + key);
            const std::string value = argv[++i];
            if (key == "--trajectories")
            {
                result.m_trajectories = std::stoi(value);
            }
            else if (key == "--repeats")
            {
                result.m_repeats = std::stoi(value);
            }
            else if (key == "--threads")
            {
                result.m_threads = std::stoi(value);
            }
            else if (key == "--frequency-index")
            {
                result.m_frequency = std::stoi(value);
            }
            else if (key == "--output")
            {
                result.m_output = value;
            }
            else
            {
                throw std::runtime_error("Unknown benchmark argument " + key);
            }
        }
    }

    Require(result.m_trajectories >= 4 && result.m_trajectories%4 == 0
        && result.m_trajectories <= 262144, "Trajectory count must be a multiple of four in [4,262144]");
    Require(result.m_repeats > 0 && result.m_threads > 0 && result.m_threads <= 128
        && result.m_frequency >= 0 && result.m_frequency < 100, "Invalid benchmark setting");
    Require(!result.m_output.empty(), "--output is required");
    return result;
}

Batch PrepareBatch(const BenchmarkOptions& args)
{
    Options options;
    options.m_starts = 10;
    options.m_minEffectiveResonance = 3.5;
    const auto plan = Plan(options);
    Batch result;
    result.m_frequency = PrepareFrequency(20*std::pow(1000, args.m_frequency/99.0), options);
    result.m_input.assign(result.m_frequency.m_input.begin(), result.m_frequency.m_input.end());
    result.m_quadrature.assign(result.m_frequency.m_quadrature.begin(), result.m_frequency.m_quadrature.end());
    for (int i = 0; i < args.m_trajectories; ++i)
    {
        const int pointIndex = i/options.m_starts;
        const int trial = i%options.m_starts;
        const uint64_t job = static_cast<uint64_t>(args.m_frequency)*250000
            + plan.m_parameterJobs[(static_cast<uint64_t>(pointIndex/4)*104729+17)%plan.m_parameterJobs.size()];
        const int lane = pointIndex%4;
        const int cutoff = static_cast<int>(job%25)*4+lane;
        const int resonance = static_cast<int>(job/25%100);
        const int drive = static_cast<int>(job/2500%100);
        const double hz = 20*std::pow(1000, cutoff/99.0);
        const auto p = PrepareGPUParameters(drive/99.0,resonance/99.0,hz);
        const auto initial = PrepareGPUHistory(p,job*4+lane,trial,options.m_seed,options.m_amplitude);
        result.m_parameters.push_back(p);
        result.m_initial.push_back(initial);
        result.m_pointIDs.push_back(job*4+lane);
        result.m_starts.push_back(trial);
    }

    return result;
}

FloatVector TanhVector(FloatVector x)
{
    const auto value = x*(x*x+27.0f)/simd::fma(x*9.0f, x, FloatVector{27,27,27,27});
    return simd::min(FloatVector{1,1,1,1}, simd::max(FloatVector{-1,-1,-1,-1}, value));
}

struct VectorSum
{
    FloatVector m_value{0,0,0,0};
    FloatVector m_error{0,0,0,0};

    void Add(FloatVector input)
    {
        const auto adjusted = input-m_error;
        const auto next = m_value+adjusted;
        m_error = (next-m_value)-adjusted;
        m_value = next;
    }
};

// CPU comparison uses four trajectories per NEON vector and all requested
// workers. There are no lane loops inside the sample loop.
//
double RunCPU(const Batch& batch, int threads, std::vector<LadderGPU::Partial>& output)
{
    const auto started = Clock::now();
    const size_t count = batch.m_initial.size();
    output.resize(count*batch.m_frequency.m_segments.size());
    std::atomic<size_t> next{0};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < threads; ++worker)
    {
        workers.emplace_back([&]
        {
            while (true)
            {
                const size_t first = next.fetch_add(4);
                if (first >= count)
                {
                    break;
                }

                FloatVector a,b,c,d,alpha,decay,feedback,gain,normal;
                for (int lane = 0; lane < 4; ++lane)
                {
                    const auto h = batch.m_initial[first+lane];
                    const auto p = batch.m_parameters[first+lane];
                    a[lane]=h.m_a; b[lane]=h.m_b; c[lane]=h.m_c; d[lane]=h.m_d;
                    alpha[lane]=p.m_alpha; decay[lane]=p.m_decay; feedback[lane]=p.m_feedback;
                    gain[lane]=p.m_gain; normal[lane]=p.m_tanhGain;
                }

                for (size_t j = 0; j < batch.m_frequency.m_segments.size(); ++j)
                {
                    const auto segment = batch.m_frequency.m_segments[j];
                    VectorSum energy,real,imaginary,dc;
                    FloatVector peak{0,0,0,0};
                    for (int n = segment.m_begin; n < segment.m_end; ++n)
                    {
                        a = simd::fma(alpha, batch.m_input[n]-feedback*d, decay*a);
                        b = simd::fma(alpha, TanhVector(gain*a)/normal, decay*b);
                        c = simd::fma(alpha, TanhVector(gain*b)/normal, decay*c);
                        d = simd::fma(alpha, TanhVector(gain*c)/normal, decay*d);
                        const auto y = (1.0f+feedback)*(TanhVector(gain*d)/normal);
                        energy.Add(y*y);
                        real.Add(y*batch.m_input[n]);
                        imaginary.Add(-y*batch.m_quadrature[n]);
                        dc.Add(y);
                        peak = simd::max(peak,simd::abs(y));
                    }

                    for (int lane = 0; lane < 4; ++lane)
                    {
                        output[j*count+first+lane] = {energy.m_value[lane], real.m_value[lane],
                            imaginary.m_value[lane], dc.m_value[lane], peak[lane]};
                    }
                }
            }
        });
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    return std::chrono::duration<double>(Clock::now()-started).count();
}

struct Summary
{
    std::array<std::array<double,6>,4> m_scopes;
    double m_temporalCV;
    double m_fundamentalCV;
    double m_coherence;
    double m_peak;
};

std::vector<Summary> Summarize(const Batch& batch, const LadderGPU::Partial* partials)
{
    const size_t count = batch.m_initial.size();
    std::vector<Summary> summaries(count);
    for (size_t i = 0; i < count; ++i)
    {
        std::array<SignalSums,x_scopes> scopes;
        std::array<SignalSums,x_windows> windows;
        for (size_t j = 0; j < batch.m_frequency.m_segments.size(); ++j)
        {
            const auto p = partials[j*count+i];
            const auto segment = batch.m_frequency.m_segments[j];
            SignalSums sums;
            sums.m_energy[0]=p.m_energy; sums.m_real[0]=p.m_real; sums.m_imaginary[0]=p.m_imaginary;
            sums.m_sum[0]=p.m_dc; sums.m_peak[0]=p.m_peak;
            windows[segment.m_window].Add(sums);
            for (int s = 0; s < x_scopes; ++s)
            {
                if (segment.m_scopeMask & (1u << s))
                {
                    scopes[s].Add(sums);
                }
            }
        }

        auto& result = summaries[i];
        for (int s = 0; s < x_scopes; ++s)
        {
            const auto interval = batch.m_frequency.m_scopes[s];
            // Float partial sums require a float-sized projection tolerance.
            // Preserve measured total power; only residual roundoff is clipped.
            //
            result.m_scopes[s] = Metrics(scopes[s],0,interval.m_end-interval.m_begin,0.25,2e-6);
        }

        Moments total,fundamental,real,imaginary;
        for (int w = batch.m_frequency.m_tailWindow; w < batch.m_frequency.m_fullWindows; ++w)
        {
            const double n = batch.m_frequency.m_windowSamples;
            const auto& sums = windows[w];
            const double r = 2*sums.m_real[0]/(n*0.25);
            const double im = 2*sums.m_imaginary[0]/(n*0.25);
            total.Add(sums.m_energy[0]/n);
            fundamental.Add((r*r+im*im)/2);
            real.Add(r);
            imaginary.Add(im);
        }

        result.m_temporalCV = total.m_mean ? total.Deviation()/total.m_mean : 0;
        result.m_fundamentalCV = fundamental.m_mean ? fundamental.Deviation()/fundamental.m_mean : 0;
        result.m_coherence = fundamental.m_mean ? (real.m_mean*real.m_mean+imaginary.m_mean*imaginary.m_mean)/(2*fundamental.m_mean) : 0;
        result.m_peak = scopes[3].m_peak[0];
        Require(std::isfinite(result.m_temporalCV) && std::isfinite(result.m_fundamentalCV)
            && result.m_coherence <= 1+1e-9, "Invalid summary");
    }

    return summaries;
}

void SaveSummaries(const fs::path& path, const Batch& batch, const std::vector<Summary>& summaries)
{
    std::ofstream output(path);
    output << "point_id,start,drive,resonance,cutoff_hz,total,fundamental,real,imaginary,dc,residual,total_cv,fundamental_cv,coherence,peak\n";
    output << std::setprecision(12);
    for (size_t i = 0; i < summaries.size(); ++i)
    {
        const auto id = batch.m_pointIDs[i];
        output << id << ',' << batch.m_starts[i] << ',' << id/10000%100/99.0 << ',' << id/100%100/99.0
            << ',' << 20*std::pow(1000,(id%100)/99.0);
        const auto& s = summaries[i];
        for (double metric : s.m_scopes[3])
        {
            output << ',' << metric;
        }

        output << ',' << s.m_temporalCV << ',' << s.m_fundamentalCV << ',' << s.m_coherence << ',' << s.m_peak << '\n';
    }

    Require(output.good(), "Failed to save " + path.string());
}

void ValidateGPU(Batch batch, MetalRunner& metal, const fs::path& directory)
{
    metal.Run(batch,false,true);
    const auto* actual = static_cast<const LadderGPU::History*>(metal.m_histories.contents);
    const auto* partials = static_cast<const LadderGPU::Partial*>(metal.m_partials.contents);
    double maxStateError = 0;
    double maxMeasurementError = 0;
    for (size_t i = 0; i < batch.m_initial.size(); ++i)
    {
        auto state = batch.m_initial[i];
        LadderGPU::Measurement measured;
        for (int n = 0; n < 8; ++n)
        {
            measured.Add(LadderGPU::Step(state,batch.m_parameters[i],batch.m_input[n]),batch.m_input[n],batch.m_quadrature[n]);
        }

        const float expected[]{state.m_a,state.m_b,state.m_c,state.m_d};
        const float observed[]{actual[i].m_a,actual[i].m_b,actual[i].m_c,actual[i].m_d};
        for (int j = 0; j < 4; ++j)
        {
            const double error = std::abs(expected[j]-observed[j])/std::max(1.0f,std::abs(expected[j]));
            maxStateError = std::max(maxStateError,error);
        }

        const auto e = measured.Get();
        const auto o = partials[i];
        const float es[]{e.m_energy,e.m_real,e.m_imaginary,e.m_dc,e.m_peak};
        const float os[]{o.m_energy,o.m_real,o.m_imaginary,o.m_dc,o.m_peak};
        for (int j = 0; j < 5; ++j)
        {
            maxMeasurementError = std::max(maxMeasurementError,
                std::abs(static_cast<double>(es[j])-os[j])/std::max(1.0f,std::abs(es[j])));
        }
    }

    Require(maxStateError < 2e-5 && maxMeasurementError < 2e-5, "Short CPU/Metal comparison failed");
    metal.Run(batch,true,false);
    const auto synthetic = Summarize(batch,static_cast<const LadderGPU::Partial*>(metal.m_partials.contents));
    double syntheticError = 0;
    const std::array<double,6> expected{0.5+0.045+0.015625,0.5,0.6,0.8,0.125,0.045};
    for (const auto& result : synthetic)
    {
        for (int s = 0; s < 4; ++s)
        {
            for (int m = 0; m < 6; ++m)
            {
                syntheticError = std::max(syntheticError,std::abs(result.m_scopes[s][m]-expected[m]));
            }
        }
    }

    Require(syntheticError < 2e-6, "Synthetic GPU spectral measurement failed");

    // Stable zero-feedback trajectories must agree for the full second, not
    // just the short-step test. Chaotic cases are compared as statistics later.
    //
    for (auto& p : batch.m_parameters)
    {
        p.m_feedback = 0;
    }

    std::memcpy(metal.m_parameters.contents,batch.m_parameters.data(),batch.m_parameters.size()*sizeof(LadderGPU::Parameters));
    metal.Run(batch,false,false);
    const auto stableGPU = Summarize(batch,static_cast<const LadderGPU::Partial*>(metal.m_partials.contents));
    std::vector<LadderGPU::Partial> stablePartials;
    RunCPU(batch,28,stablePartials);
    const auto stableCPU = Summarize(batch,stablePartials.data());
    double stableError = 0;
    for (size_t i = 0; i < stableGPU.size(); ++i)
    {
        for (int scope = 0; scope < 4; ++scope)
        {
            for (int metric = 0; metric < 6; ++metric)
            {
                const double reference = stableCPU[i].m_scopes[scope][metric];
                stableError = std::max(stableError,std::abs(stableGPU[i].m_scopes[scope][metric]-reference)
                    /std::max(0.03125,std::abs(reference)));
            }
        }
    }

    Require(stableError < 1e-3, "Full-second zero-feedback CPU/Metal comparison failed");
    std::ostringstream json;
    json << std::setprecision(12) << "{\"status\":\"passed\",\"short_state_scaled_error\":" << maxStateError
        << ",\"short_measurement_scaled_error\":" << maxMeasurementError
        << ",\"synthetic_measurement_absolute_error\":" << syntheticError
        << ",\"stable_full_second_scaled_error\":" << stableError << '}';
    WriteText(directory/"validation.json",json.str());
    std::cout << json.str() << std::endl;
}

void Benchmark(const BenchmarkOptions& options)
{
    fs::create_directories(options.m_output);
    const auto batch = PrepareBatch(options);
    MetalRunner metal(batch);
    std::cout << "{\"gpu\":\"" << metal.m_device.name.UTF8String << "\",\"trajectories\":" << batch.m_initial.size()
        << ",\"frequency_hz\":" << batch.m_frequency.m_actual << ",\"samples\":192000}" << std::endl;
    if (options.m_validate)
    {
        ValidateGPU(batch,metal,options.m_output);
        std::memcpy(metal.m_parameters.contents,batch.m_parameters.data(),batch.m_parameters.size()*sizeof(LadderGPU::Parameters));
    }

    metal.Run(batch,false,true);
    const double updates = static_cast<double>(batch.m_initial.size())*192000;
    std::ostringstream records;
    records << std::setprecision(12) << "[";
    for (int repeat = 0; repeat < options.m_repeats; ++repeat)
    {
        metal.Run(batch,false,false);
        const auto summarizeStart = Clock::now();
        const auto summaries = Summarize(batch,static_cast<const LadderGPU::Partial*>(metal.m_partials.contents));
        const double summarySeconds = std::chrono::duration<double>(Clock::now()-summarizeStart).count();
        if (repeat == 0)
        {
            SaveSummaries(options.m_output/"gpu-trajectories.csv",batch,summaries);
        }

        std::ostringstream line;
        const double totalSeconds = metal.m_wallSeconds+summarySeconds;
        line << std::setprecision(12) << "{\"repeat\":" << repeat << ",\"gpu_seconds\":" << metal.m_gpuSeconds
            << ",\"dispatch_wall_seconds\":" << metal.m_wallSeconds << ",\"summary_seconds\":" << summarySeconds
            << ",\"sample_updates_per_second\":" << updates/totalSeconds
            << ",\"speedup_vs_double_survey\":" << updates/totalSeconds/1.06450366855e9
            << ",\"projected_10_start_hours\":" << 71193600000000.0/(updates/totalSeconds)/3600 << '}';
        if (repeat)
        {
            records << ',';
        }

        records << line.str();
        std::cout << line.str() << std::endl;
    }

    records << ']';
    WriteText(options.m_output/"gpu-timing.json",records.str());
    if (options.m_cpu)
    {
        std::vector<LadderGPU::Partial> partials;
        const double seconds = RunCPU(batch,options.m_threads,partials);
        const auto begin = Clock::now();
        const auto summaries = Summarize(batch,partials.data());
        const double summarySeconds = std::chrono::duration<double>(Clock::now()-begin).count();
        SaveSummaries(options.m_output/"cpu-float-trajectories.csv",batch,summaries);
        std::ostringstream line;
        line << std::setprecision(12) << "{\"cpu_float_rational_seconds\":" << seconds
            << ",\"summary_seconds\":" << summarySeconds << ",\"threads\":" << options.m_threads
            << ",\"sample_updates_per_second\":" << updates/(seconds+summarySeconds) << '}';
        WriteText(options.m_output/"cpu-timing.json",line.str());
        std::cout << line.str() << std::endl;
    }

    if (options.m_compareDouble)
    {
        Options reference;
        reference.m_starts = 10;
        std::vector<Frequency> frequencies(100);
        frequencies[options.m_frequency] = batch.m_frequency;
        std::vector<uint64_t> jobs;
        for (size_t i = 0; i < std::min<size_t>(batch.m_pointIDs.size(),1280); i += 40)
        {
            jobs.push_back(batch.m_pointIDs[i]/4);
        }

        std::vector<Record> records(jobs.size());
        std::atomic<size_t> next{0};
        std::vector<std::thread> workers;
        for (int t = 0; t < options.m_threads; ++t)
        {
            workers.emplace_back([&]
            {
                while (true)
                {
                    const auto i = next.fetch_add(1);
                    if (i >= jobs.size())
                    {
                        break;
                    }

                    records[i] = Evaluate(jobs[i],reference,frequencies);
                }
            });
        }

        for (auto& worker : workers)
        {
            worker.join();
        }

        std::ofstream output(options.m_output/"double-real-tanh-points.csv");
        output << "point_id,total,total_sd,fundamental,fundamental_sd\n" << std::setprecision(12);
        for (const auto& record : records)
        {
            for (int lane = 0; lane < 4; ++lane)
            {
                const auto& scope = record.m_points[lane].m_scopes[3];
                output << record.m_job*4+lane << ',' << scope[0][0] << ',' << scope[0][1]
                    << ',' << scope[1][0] << ',' << scope[1][1] << '\n';
            }
        }

        Require(output.good(), "Could not save double reference");
        std::cout << "{\"double_reference_points\":" << jobs.size()*4 << "}" << std::endl;
    }
}
}

int main(int argc,char** argv)
{
    @autoreleasepool
    {
        try
        {
            Benchmark(ParseBenchmark(argc,argv));
            return 0;
        }
        catch (const std::exception& error)
        {
            std::cerr << "ladder-gpu-bench: " << error.what() << '\n';
            return 1;
        }
    }
}

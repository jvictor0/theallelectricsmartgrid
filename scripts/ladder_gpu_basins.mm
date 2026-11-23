// Research basin and continuation experiments using the unchanged survey shader.
//
#include "ladder_gpu_host.hpp"
#include <random>
#include <set>

namespace
{
struct Trial
{
    std::string m_family;
    double m_epsilon;
    unsigned m_index;
    LadderGPU::History m_initial;
};

void Advance(MetalRunner& runner, unsigned samples, unsigned slots)
{
    Require(slots > 0 && slots <= 32 && samples <= runner.m_sampleCapacity,"Invalid basin dispatch");
    @autoreleasepool
    {
        id<MTLCommandBuffer> command = [runner.m_queue commandBuffer];
        for (unsigned slot = 0; slot < slots; ++slot)
        {
            id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
            [encoder setComputePipelineState:runner.m_pipeline];
            [encoder setBuffer:runner.m_parameters offset:0 atIndex:0];
            [encoder setBuffer:runner.m_histories offset:0 atIndex:1];
            [encoder setBuffer:runner.m_input offset:0 atIndex:2];
            [encoder setBuffer:runner.m_quadrature offset:0 atIndex:3];
            [encoder setBuffer:runner.m_partials offset:slot*runner.m_count*sizeof(LadderGPU::Partial) atIndex:4];
            const LadderGPU::Dispatch dispatch{static_cast<unsigned>(runner.m_count),0,samples,0};
            [encoder setBytes:&dispatch length:sizeof(dispatch) atIndex:5];
            [encoder dispatchThreads:MTLSizeMake(runner.m_count,1,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
            [encoder endEncoding];
        }

        [command commit];
        [command waitUntilCompleted];
        Require(command.status == MTLCommandBufferStatusCompleted,command.error ? command.error.localizedDescription.UTF8String : "Basin command failed");
    }
}

struct Totals
{
    double m_power = 0;
    double m_real = 0;
    double m_imag = 0;
    double m_dc = 0;
    uint64_t m_samples = 0;

    void Add(LadderGPU::Partial p, unsigned samples)
    {
        m_power += p.m_energy;
        m_real += p.m_real;
        m_imag += p.m_imaginary;
        m_dc += p.m_dc;
        m_samples += samples;
    }
};

void SetParameters(MetalRunner& runner, LadderGPU::Parameters p)
{
    auto* parameters = static_cast<LadderGPU::Parameters*>(runner.m_parameters.contents);
    std::fill_n(parameters,runner.m_count,p);
}

Batch MakeBatch(uint64_t point, const std::vector<Trial>& trials)
{
    Options options;
    Batch batch;
    batch.m_frequency = PrepareFrequency(20*std::pow(1000,(point/1000000)/99.0),options);
    const auto& f = batch.m_frequency;
    const auto q = f.m_period;
    for (int n = 0; n < options.m_samples; ++n)
    {
        const auto phase = (115200000+n)%q;
        batch.m_input.push_back(static_cast<float>(f.m_input[phase]));
        batch.m_quadrature.push_back(static_cast<float>(f.m_quadrature[phase]));
    }

    const auto p = PrepareGPUParameters((point/10000%100)/99.0,(point/100%100)/99.0,20*std::pow(1000,(point%100)/99.0));
    for (const auto& trial : trials)
    {
        batch.m_initial.push_back(trial.m_initial);
        batch.m_parameters.push_back(p);
    }

    return batch;
}

void Experiment(uint64_t point, const std::vector<Trial>& trials, const std::string& route,
    double rampSeconds, double holdSeconds, const fs::path& output)
{
    auto batch = MakeBatch(point,trials);
    MetalRunner runner(batch);
    std::memcpy(runner.m_histories.contents,batch.m_initial.data(),batch.m_initial.size()*sizeof(batch.m_initial[0]));
    const auto target = batch.m_parameters[0];
    const unsigned q = batch.m_frequency.m_period;
    const auto wholePeriods = [q](double seconds)
    {
        return q*static_cast<unsigned>(std::ceil(seconds*192000/q));
    };
    double actualRamp = 0;
    if (route != "direct")
    {
        const auto parametersAt = [&](double t)
        {
            auto p = target;
            if (route == "drive" || route == "both")
            {
                p = PrepareGPUParameters(t*(point/10000%100)/99.0,(point/100%100)/99.0,
                    20*std::pow(1000,(point%100)/99.0));
            }

            if (route == "feedback" || route == "both")
            {
                p.m_feedback = static_cast<float>(t*target.m_feedback);
            }

            return p;
        };
        SetParameters(runner,parametersAt(0));
        Advance(runner,wholePeriods(0.25),1);
        constexpr unsigned x_steps = 64;
        const unsigned samples = wholePeriods(rampSeconds/x_steps);
        actualRamp = static_cast<double>(samples)*x_steps/192000;
        for (unsigned i = 1; i <= x_steps; ++i)
        {
            SetParameters(runner,parametersAt(static_cast<double>(i)/x_steps));
            Advance(runner,samples,1);
        }
    }

    SetParameters(runner,target);
    const unsigned chunk = wholePeriods(0.05);
    const unsigned chunks = static_cast<unsigned>(std::ceil(holdSeconds*192000/chunk));
    std::vector<std::array<Totals,2>> totals(trials.size());
    for (unsigned offset = 0; offset < chunks; offset += 16)
    {
        const unsigned slots = std::min(16u,chunks-offset);
        Advance(runner,chunk,slots);
        const auto* values = static_cast<const LadderGPU::Partial*>(runner.m_partials.contents);
        for (unsigned slot = 0; slot < slots; ++slot)
        {
            const unsigned half = offset+slot < chunks/2 ? 0 : 1;
            for (size_t i = 0; i < trials.size(); ++i)
            {
                totals[i][half].Add(values[slot*trials.size()+i],chunk);
            }
        }
    }

    const auto* final = static_cast<const LadderGPU::History*>(runner.m_histories.contents);
    std::ofstream csv(output);
    Require(csv.good(),"Cannot write basin results");
    csv << std::setprecision(17) << "point,family,epsilon,index,route,ramp_seconds,half,samples,power,fundamental,real,imaginary,dc,initial_a,initial_b,initial_c,initial_d,final_a,final_b,final_c,final_d\n";
    for (size_t i = 0; i < trials.size(); ++i)
    {
        const auto& trial = trials[i];
        for (unsigned half = 0; half < 2; ++half)
        {
            const auto v = totals[i][half];
            const double real = 8*v.m_real/v.m_samples;
            const double imag = 8*v.m_imag/v.m_samples;
            csv << point << ',' << trial.m_family << ',' << trial.m_epsilon << ',' << trial.m_index << ','
                << route << ',' << actualRamp << ',' << half << ',' << v.m_samples << ','
                << v.m_power/v.m_samples << ',' << (real*real+imag*imag)/2 << ',' << real << ',' << imag << ',' << v.m_dc/v.m_samples << ','
                << trial.m_initial.m_a << ',' << trial.m_initial.m_b << ',' << trial.m_initial.m_c << ',' << trial.m_initial.m_d << ','
                << final[i].m_a << ',' << final[i].m_b << ',' << final[i].m_c << ',' << final[i].m_d << '\n';
        }
    }

    std::cout << "{\"point\":" << point << ",\"route\":\"" << route << "\",\"ramp\":" << actualRamp
        << ",\"trials\":" << trials.size() << ",\"hold_seconds\":" << static_cast<double>(chunks)*chunk/192000 << "}\n" << std::flush;
}

int RunBasins(int argc, char** argv)
{
    Require(argc == 4,"Usage: ladder-gpu-basins REQUESTS SAVED_STATES OUTPUT_DIRECTORY");
    const fs::path output = argv[3];
    Require(!fs::exists(output),"Basin output already exists");
    std::vector<LadderGPU::History> saved(fs::file_size(argv[2])/sizeof(LadderGPU::History));
    std::ifstream stateFile(argv[2],std::ios::binary);
    stateFile.read(reinterpret_cast<char*>(saved.data()),saved.size()*sizeof(saved[0]));
    Require(stateFile.good(),"Cannot read saved states");
    fs::create_directories(output);
    std::ifstream requests(argv[1]);
    uint64_t point;
    unsigned lowIndex,highIndex;
    while (requests >> point >> lowIndex >> highIndex)
    {
        Require(lowIndex < saved.size() && highIndex < saved.size(),"Invalid saved index");
        const auto p = PrepareGPUParameters((point/10000%100)/99.0,(point/100%100)/99.0,20*std::pow(1000,(point%100)/99.0));
        const std::array<double,4> scale{0.25+p.m_feedback/p.m_tanhGain,1.0/p.m_tanhGain,1.0/p.m_tanhGain,1.0/p.m_tanhGain};
        std::vector<Trial> trials;
        trials.push_back({"saved_low",0,0,saved[lowIndex]});
        trials.push_back({"saved_high",0,0,saved[highIndex]});
        trials.push_back({"reset",0,0,{0,0,0,0}});
        for (unsigned i = 0; i < 4096; ++i)
        {
            trials.push_back({"fresh_random",0,i,PrepareGPUHistory(p,point,i,20261121,0.25)});
        }

        std::mt19937_64 rng(20261121+point);
        std::normal_distribution<double> normal(0,1);
        for (const auto& base : {std::pair<std::string,LadderGPU::History>{"kick_low",saved[lowIndex]},
             std::pair<std::string,LadderGPU::History>{"kick_high",saved[highIndex]}})
        {
            for (double epsilon : {1e-7,1e-5,1e-3,0.01,0.1,0.3,1.0})
            {
                for (unsigned i = 0; i < 128; ++i)
                {
                    std::array<double,4> direction;
                    double norm = 0;
                    for (auto& v : direction)
                    {
                        v = normal(rng);
                        norm += v*v;
                    }

                    const auto b = base.second;
                    const std::array<double,4> initial{b.m_a,b.m_b,b.m_c,b.m_d};
                    std::array<float,4> value;
                    for (unsigned j = 0; j < 4; ++j)
                    {
                        value[j] = static_cast<float>(initial[j]+epsilon*scale[j]*direction[j]/std::sqrt(norm));
                    }

                    trials.push_back({base.first,epsilon,i,{value[0],value[1],value[2],value[3]}});
                }
            }
        }

        auto batch = MakeBatch(point,trials);
        std::ostringstream model;
        model << std::setprecision(17) << "{\"point_id\":" << point << ",\"coefficients\":["
            << p.m_alpha << ',' << p.m_decay << ',' << p.m_feedback << ',' << p.m_gain << ',' << p.m_tanhGain
            << "],\"period\":" << batch.m_frequency.m_period << ",\"cycles\":" << batch.m_frequency.m_cycles << ",\"input\":[";
        for (int i = 0; i < batch.m_frequency.m_period; ++i)
        {
            model << (i ? "," : "") << batch.m_input[i];
        }

        model << "],\"quadrature\":[";
        for (int i = 0; i < batch.m_frequency.m_period; ++i)
        {
            model << (i ? "," : "") << batch.m_quadrature[i];
        }

        model << "]}\n";
        WriteText(output/(std::to_string(point)+"-model.json"),model.str());
        Experiment(point,trials,"direct",0,60,output/(std::to_string(point)+"-direct.csv"));
        std::vector<Trial> pathTrials{trials[0],trials[1],trials[2]};
        for (unsigned i = 0; i < 32; ++i)
        {
            pathTrials.push_back(trials[i+3]);
        }

        for (const std::string route : {"feedback","drive","both"})
        {
            for (double seconds : {0.1,1.0,10.0})
            {
                Experiment(point,pathTrials,route,seconds,30,
                    output/(std::to_string(point)+"-"+route+"-"+std::to_string(seconds)+".csv"));
            }
        }
    }

    Require(requests.eof(),"Cannot read basin requests");
    return 0;
}
}

int main(int argc, char** argv)
{
    try
    {
        return RunBasins(argc,argv);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "Bench.hpp"
#include "ResponsesCase.hpp"
#include "FIR.hpp"
#include "Ladder4PoleShape.hpp"
#include "PhysicalModelingSource.hpp"
#include "SquiggleBoyShape.hpp"
#include "SourceMixer.hpp"
#include "MultibandSaturator.hpp"
#include <functional>
#include <iomanip>
#include <random>

namespace
{
using Response = SampledTransferFunction;

// Keep the production virtual boundary: the renderer also holds a base reference.
//
[[gnu::noinline]] double Plot(Response& response, SampledResponsePlot& plot, size_t points)
{
    plot.Evaluate(response, points, true, 23.4375, SampleTimer::x_sampleRate);
    double checksum = 0;
    for (const auto& values : plot.m_values)
    {
        checksum += values.m_lower + values.m_median + values.m_upper;
    }

    asm volatile("" : : "g"(plot.m_values.data()) : "memory");
    return checksum;
}

template<typename Snapshot>
struct ComplexReference : TransferFunctionGeneric<Snapshot>
{
    using ResponseVector = typename Snapshot::ResponseVector;

    ResponseVector MagnitudeResponses(size_t id, const ResponseFrequency& frequency) const override
    {
        return this->m_readSnapshots[id].ComplexResponses(frequency).Abs().template Convert<float>();
    }
};

struct Case
{
    std::string m_name;
    std::unique_ptr<Response> m_magnitude;
    std::unique_ptr<Response> m_complex;
    SampledResponsePlot m_plot;
};

template<typename Factory>
void Add(std::vector<Case>& cases, const std::string& name, Factory factory)
{
    using Snapshot = decltype(factory(0));
    auto magnitude = std::make_unique<TransferFunctionGeneric<Snapshot>>();
    auto complex = std::make_unique<ComplexReference<Snapshot>>();
    for (size_t i = 0; i < 32; ++i)
    {
        auto snapshot = factory(i);
        magnitude->m_history.Push(snapshot);
        complex->m_history.Push(snapshot);
    }

    cases.push_back({name, std::move(magnitude), std::move(complex), {}});
}

float Control(size_t i)
{
    return static_cast<float>((i * 13) % 32) / 31;
}

template<size_t Size>
MultiChannelNumber<float, Size> Controls(size_t i)
{
    MultiChannelNumber<float, Size> result;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        result[lane] = Control(i + lane * 7);
    }

    return result;
}

template<size_t Size>
auto LP(size_t i)
{
    OPLowPassFilterGeneric<float, Size> filter;
    auto cutoff = Controls<Size>(i);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        cutoff[lane] = 0.001f * std::exp2(5 * cutoff[lane]);
    }

    filter.SetAlphaFromNatFreq(cutoff);
    return filter.MakeSnapshot();
}

template<size_t Size>
auto HP(size_t i)
{
    OPHighPassFilterGeneric<float, Size> filter;
    auto cutoff = Controls<Size>(i);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        cutoff[lane] = 0.0001f * std::exp2(5 * cutoff[lane]);
    }

    filter.SetAlphaFromNatFreq(cutoff);
    return filter.MakeSnapshot();
}

template<size_t Size>
auto SVF(size_t i)
{
    LinearStateVariableFilterGeneric<float, Size> filter;
    auto control = Controls<Size>(i);
    auto cutoff = control;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        cutoff[lane] = 0.001f * std::exp2(5 * control[lane]);
    }

    filter.SetCutoffAndResonance(cutoff, control * 0.7f + 0.2f);
    return filter.MakeSnapshot();
}

template<size_t Size>
auto Ladder(size_t i)
{
    LadderFilterLPGeneric<float, Size> filter;
    auto control = Controls<Size>(i);
    auto cutoff = control;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        cutoff[lane] = 0.001f * std::exp2(5 * control[lane]);
    }

    filter.SetCutoff(cutoff);
    filter.SetResonance(control * 0.7f + 0.2f);
    filter.Process(MultiChannelNumber<float, Size>::Splat(0.1f));
    return filter.MakeSnapshot();
}

template<size_t Size>
auto Comb(size_t i, bool crossfade)
{
    typename TunedDecayCombGeneric<Size>::Snapshot snapshot;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        double control = Control(i + lane * 7);
        double pitch = 110 * std::exp2(3 * control);
        for (size_t branch = 0; branch < 2; ++branch)
        {
            snapshot.m_branch[branch].Set(lane, TunedDecayComb::ComputeCoefficients(
                pitch * (1 + 0.01 * branch) / SampleTimer::x_sampleRate,
                1, std::log2(0.1 + 4 * control), 1 + 6 * control, SampleTimer::x_sampleRate));
        }
    }

    snapshot.m_mix = crossfade ? Control(i) : 0;
    return snapshot;
}

template<size_t Size>
void AddWidth(std::vector<Case>& cases)
{
    std::string suffix = Size == 1 ? "/mono" : "/trio";
    Add(cases, "one_pole_lp" + suffix, LP<Size>);
    Add(cases, "one_pole_hp" + suffix, HP<Size>);
    Add(cases, "base_width" + suffix, [](size_t i)
    {
        typename OPBaseWidthFilterGeneric<float, Size>::Snapshot s;
        s.m_lp = LP<Size>(i);
        s.m_hp = HP<Size>(i);
        return s;
    });
    Add(cases, "ladder" + suffix, Ladder<Size>);
    for (size_t kind = 0; kind < 4; ++kind)
    {
        Add(cases, std::string(std::array<const char*, 4>{"svf_lp", "svf_bp", "svf_hp", "svf_morph"}[kind]) + suffix, [kind](size_t i)
        {
            auto s = SVF<Size>(i);
            s.m_weights = LinearStateVariableFilterGeneric<float, Size>::GetMorphWeights(kind == 3
                ? Controls<Size>(i) : MultiChannelNumber<float, Size>::Splat(kind * 0.5f));
            return s;
        });
    }

    Add(cases, "svf_four_pole_hp" + suffix, [](size_t i)
    {
        typename LinearSVF4PoleHighPassGeneric<float, Size>::Snapshot s;
        s.m_stage = SVF<Size>(i);
        s.m_stage.m_weights = {0, 0, 1};
        return s;
    });
    for (bool crossfade : {false, true})
    {
        auto ending = std::string(crossfade ? "_crossfade" : "_active") + suffix;
        Add(cases, "comb" + ending, [crossfade](size_t i)
        {
            return Comb<Size>(i, crossfade);
        });
        Add(cases, "physical_model" + ending, [crossfade](size_t i)
        {
            typename PhysicalModelingSourceGeneric<Size>::Snapshot s;
            s.m_mainSVF = SVF<Size>(i);
            s.m_mainSVF.m_weights = LinearStateVariableFilterGeneric<float, Size>::GetMorphWeights(Controls<Size>(i));
            s.m_combFilter = Comb<Size>(i, crossfade);
            return s;
        });
    }

    for (bool ladder : {false, true})
    {
        Add(cases, std::string(ladder ? "shape_ladder" : "shape_svf") + suffix, [ladder](size_t i)
        {
            typename SquiggleBoyShapeGeneric<Size>::Snapshot s;
            s.m_filterMachine = ladder ? VoiceMachine::FilterMachine::Ladder4Pole : VoiceMachine::FilterMachine::SVF2Pole;
            s.m_ladder.m_lp = Ladder<Size>(i);
            s.m_ladder.m_hp.m_stage = SVF<Size>(i);
            s.m_ladder.m_hp.m_stage.m_weights = {0, 0, 1};
            s.m_svf.m_lp = SVF<Size>(i);
            s.m_svf.m_hp = SVF<Size>(i + 7);
            s.m_svf.m_hp.m_weights = {0, 0, 1};
            return s;
        });
    }
}

std::vector<Case> Cases()
{
    std::vector<Case> cases;
    AddWidth<1>(cases);
    AddWidth<3>(cases);
    Add(cases, "delay_reverb_damping/quad", [](size_t i)
    {
        QuadOPBaseWidthFilter::Snapshot s;
        s.m_lp = LP<4>(i);
        s.m_hp = HP<4>(i);
        return s;
    });
    Add(cases, "input_filters/quad", [](size_t i)
    {
        SourceMixer::Snapshot s;
        s.m_lp = SVF<4>(i);
        s.m_hp = SVF<4>(i);
        s.m_hp.m_g *= 0.05f;
        s.m_hp.m_weights = {0, 0, 1};
        return s;
    });
    Add(cases, "crossover_lp_hp/pair", [](size_t i)
    {
        LinkwitzRileyCrossoverGeneric<float, 2> filter;
        filter.SetCyclesPerSample(0.001f * std::exp2(5 * Control(i)));
        return filter.MakeSnapshot();
    });
    Add(cases, "master_four_band/mono", [](size_t i)
    {
        MultibandSaturator<4, 2>::Snapshot s;
        for (size_t band = 0; band < 4; ++band)
        {
            s.m_gain[band] = 0.5f + 1.5f * Control(i + band * 7);
            if (band < 3)
            {
                LinkwitzRileyCrossoverGeneric<float, 2> filter;
                filter.SetCyclesPerSample(0.0005f * std::exp2(2 * band + Control(i)));
                s.m_crossovers.m_tangent[band] = filter.MakeSnapshot().m_coefficients.m_tangent[0];
            }
        }

        return s;
    });
    Add(cases, "fir_31/mono", [](size_t i)
    {
        FIR<31> filter;
        filter.SetParams(0.01f + 0.2f * Control(i), Control(i), 0.5f);
        std::copy_n(filter.m_targetTaps, 31, filter.m_slewedTaps);
        return filter.MakeSnapshot();
    });
    return cases;
}
}

int RunResponses(int argc, char** argv, std::ostream& output, std::ostream& errors)
{
    try
    {
        if (argc > 3)
        {
            throw std::runtime_error("Usage: perf_responses [REPEATS] [SECONDS_PER_CASE]");
        }

        PrepareCPU();
        int repeats = argc > 1 ? Integer(argv[1]) : 5;
        double seconds = argc > 2 ? PositiveSeconds(argv[2]) : 0.025;
        if (repeats < 1 || repeats > 20)
        {
            throw std::runtime_error("repeats must be 1..20");
        }

        auto cases = Cases();
        struct Job
        {
            size_t m_case;
            size_t m_points;
            bool m_complex;
        };

        std::vector<Job> jobs;
        for (size_t c = 0; c < cases.size(); ++c)
        {
            for (size_t points : {321, 641, 1024})
            {
                for (bool complex : {false, true})
                {
                    jobs.push_back({c, points, complex});
                }
            }
        }

        std::mt19937 random(314159);
        double checksum = 0;
        auto warmEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now() < warmEnd)
        {
            checksum += Plot(*cases.back().m_magnitude, cases.back().m_plot, 1024);
        }

        output << std::setprecision(9);
        for (int repeat = 0; repeat < repeats; ++repeat)
        {
            std::shuffle(jobs.begin(), jobs.end(), random);
            for (const auto& job : jobs)
            {
                auto& item = cases[job.m_case];
                auto& response = *(job.m_complex ? item.m_complex : item.m_magnitude);
                double calibration = CPUSeconds();
                checksum += Plot(response, item.m_plot, job.m_points);
                calibration = CPUSeconds() - calibration;
                size_t frames = std::clamp(static_cast<size_t>(seconds / std::max(calibration, 1e-6)), size_t{1}, size_t{1000});
                double start = CPUSeconds();
                auto wallStart = std::chrono::steady_clock::now();
                for (size_t frame = 0; frame < frames; ++frame)
                {
                    checksum += Plot(response, item.m_plot, job.m_points);
                }

                double elapsed = CPUSeconds() - start;
                double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
                output << "{\"case\":\"" << item.m_name << "\",\"points\":" << job.m_points
                    << ",\"path\":\"" << (job.m_complex ? "complex_abs" : "magnitude")
                    << "\",\"repeat\":" << repeat << ",\"frames\":" << frames
                    << ",\"lanes\":" << response.NumLanes()
                    << ",\"cpu_ms_per_plot\":" << elapsed * 1000 / frames
                    << ",\"wall_ms_per_plot\":" << wall * 1000 / frames << "}\n";
            }
        }

        if (!std::isfinite(checksum) || checksum <= 0)
        {
            throw std::runtime_error("Invalid response checksum");
        }

        errors << "checksum=" << checksum << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        errors << error.what() << '\n';
        return 1;
    }
}

#include "Bench.hpp"
#include "ResponsesCase.hpp"
#include "FIR.hpp"
#include "NaiveReuppingLadderShape.hpp"
#include "PhysicalModelingSource.hpp"
#include "SquiggleBoyShape.hpp"
#include "SourceMixer.hpp"
#include "TPTBaseWidthFilter.hpp"
#include "MultibandSaturator.hpp"
#include <functional>
#include <fstream>
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
    using Frequency = typename Snapshot::Frequency;

    ResponseVector MagnitudeSquaredResponses(size_t id, const Frequency& frequency) const override
    {
        return this->m_readSnapshots[id].ComplexResponses(frequency).Norm().template Convert<float>();
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
    std::unique_ptr<Response> complex;
    if constexpr (std::is_base_of_v<FilterResponseSnapshot, Snapshot>)
    {
        auto reference = std::make_unique<ComplexReference<Snapshot>>();
        for (size_t i = 0; i < 32; ++i)
        {
            reference->m_history.Push(factory(i));
        }

        complex = std::move(reference);
    }

    for (size_t i = 0; i < 32; ++i)
    {
        auto snapshot = factory(i);
        magnitude->m_history.Push(snapshot);
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

template<typename Filter>
auto GeneratedSVF(size_t i)
{
    Filter filter;
    auto control = Controls<Filter::x_numChannels>(i);
    auto cutoff = control;
    for (size_t lane = 0; lane < Filter::x_numChannels; ++lane)
    {
        cutoff[lane] = 0.001f * std::exp2(5 * control[lane]);
    }

    filter.SetCutoffAndResonance(cutoff, control * 0.7f + 0.2f);
    return filter.MakeSnapshot();
}

template<size_t Size>
auto GeneratedMorph(size_t i)
{
    MorphingTPTSVFFilterGeneric<float, Size> filter;
    auto control = Controls<Size>(i);
    auto cutoff = control;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        cutoff[lane] = 0.001f * std::exp2(5 * control[lane]);
    }

    filter.SetCutoffResonanceAndMorph(cutoff, control * 0.7f + 0.2f, control);
    return filter.MakeSnapshot();
}

template<size_t Size>
auto GeneratedBW(size_t i)
{
    using Filter = TPTBaseWidthGeneric<float, Size>;
    GeneratedFilterView<Filter, &Filter::MagnitudeSquared> filter{};
    auto high = Controls<Size>(i);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        high[lane] = 0.0001f * std::exp2(5 * high[lane]);
    }

    filter.ComputeCoefficients({{high}, {high * 10}});
    return filter.MakeSnapshot();
}

template<size_t Size>
auto Ladder(size_t i)
{
    GeneratedNaiveReuppingLadderGeneric<float, Size> filter;
    auto control = Controls<Size>(i);
    auto cutoff = control;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        cutoff[lane] = 0.001f * std::exp2(5 * control[lane]);
    }

    filter.SetCutoffAndResonance(cutoff, control * 0.7f + 0.2f);
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
    Add(cases, "base_width" + suffix, GeneratedBW<Size>);
    Add(cases, "ladder" + suffix, Ladder<Size>);
    Add(cases, "svf_lp" + suffix, GeneratedSVF<TPTSVFLowPassFilterGeneric<float, Size>>);
    Add(cases, "svf_bp" + suffix, GeneratedSVF<TPTSVFBandPassFilterGeneric<float, Size>>);
    Add(cases, "svf_hp" + suffix, GeneratedSVF<TPTSVFHighPassFilterGeneric<float, Size>>);
    Add(cases, "svf_morph" + suffix, GeneratedMorph<Size>);
    Add(cases, "svf_four_pole_hp" + suffix, GeneratedSVF<TPTSVF4PoleHighPassFilterGeneric<float, Size>>);
    const auto initialSourceSnapshot = PhysicalModelingSourceGeneric<Size>(42).MakeSnapshot();
    for (bool crossfade : {false, true})
    {
        auto ending = std::string(crossfade ? "_crossfade" : "_active") + suffix;
        Add(cases, "comb" + ending, [crossfade](size_t i)
        {
            return Comb<Size>(i, crossfade);
        });
        Add(cases, "physical_model" + ending, [crossfade, initialSourceSnapshot](size_t i)
        {
            auto s = initialSourceSnapshot;
            s.m_mainSVF = GeneratedMorph<Size>(i);
            s.m_combFilter = Comb<Size>(i, crossfade);
            return s;
        });
    }

    for (bool ladder : {false, true})
    {
        Add(cases, std::string(ladder ? "shape_ladder" : "shape_svf") + suffix, [ladder](size_t i)
        {
            typename SquiggleBoyShapeGeneric<Size>::Snapshot s;
            s.m_filterMachine = ladder ? VoiceMachine::FilterMachine::NaiveReuppingLadder : VoiceMachine::FilterMachine::SVF2Pole;
            s.m_naiveReuppingLadder.m_lp = Ladder<Size>(i);
            s.m_naiveReuppingLadder.m_hp = GeneratedSVF<TPTSVF4PoleHighPassFilterGeneric<float, Size>>(i);
            s.m_svf.m_lp = GeneratedSVF<TPTSVFLowPassFilterGeneric<float, Size>>(i);
            s.m_svf.m_hp = GeneratedSVF<TPTSVFHighPassFilterGeneric<float, Size>>(i + 7);
            return s;
        });
    }
}

std::vector<Case> Cases(const char* tablePath)
{
    static const std::vector<unsigned char> ladderResponse = [tablePath]
    {
        std::ifstream file(tablePath, std::ios::binary);
        return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), {});
    }();
    Require(ladderResponse.size() == NaiveLadderResponse::x_bytes, "Missing ladder response fixture");
    NaiveLadderResponse::s_table.emplace(ladderResponse.data(), ladderResponse.size());
    std::vector<Case> cases;
    AddWidth<1>(cases);
    AddWidth<3>(cases);
    Add(cases, "delay_reverb_damping/quad", GeneratedBW<4>);
    Add(cases, "input_filters/quad", [](size_t i)
    {
        SourceMixer::Snapshot s;
        s.m_lp = GeneratedSVF<TPTSVFLowPassFilterGeneric<float, 4>>(i);
        s.m_hp = GeneratedSVF<TPTSVFHighPassFilterGeneric<float, 4>>(i);
        s.m_hp.m_g *= 0.05f;
        return s;
    });
    Add(cases, "master_four_band/mono", [](size_t i)
    {
        MultibandSaturator<4, 2> filter;
        MultibandSaturator<4, 2>::Input input;
        input.m_bassFreq.m_expParam = 0.0005f * std::exp2(Control(i));
        input.m_crossoverFreqFactor[0].m_expParam = 4;
        input.m_crossoverFreqFactor[1].m_expParam = 4;
        for (size_t band = 0; band < 4; ++band)
        {
            input.m_gain[band].m_expParam = 0.5f + 1.5f * Control(i + band * 7);
        }

        filter.SetCoefficients(input);
        return filter.MakeSnapshot();
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

int RunResponses(int argc, char** argv, const char* tablePath, std::ostream& output, std::ostream& errors)
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

        auto cases = Cases(tablePath);
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
                    if (!complex || cases[c].m_complex)
                    {
                        jobs.push_back({c, points, complex});
                    }
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

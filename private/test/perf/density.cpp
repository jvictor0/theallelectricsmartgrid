#include "Bench.hpp"
#include "TheoryOfTime.hpp"
#include "PeriodicPhaseAxis.hpp"
#include "Tuning.hpp"
#include "reference/LegacyTimeWarpInverter.hpp"
#include "reference/PhaseGridTimeWarpInverter.hpp"
#include "reference/LandedTimeWarpInverter.hpp"
#include <iomanip>
#include <map>
#include <random>
#include <sstream>
#include <thread>

namespace
{
constexpr size_t x_hop = 1024 * SampleTimer::x_oversample;
constexpr double x_nan = std::numeric_limits<double>::quiet_NaN();
using Current = TheoryOfTime::Inverter;
using Legacy = LegacyTimeWarpInverter<TheoryOfTime::x_timeWarpInverterSamples>;
using Prior = PhaseGridTimeWarpInverter<TheoryOfTime::x_timeWarpInverterSamples>;
using Landed = LandedTimeWarpInverter<Current::x_maxSamples>;

struct Settings
{
    const char* m_name;
    int m_chain;
    float m_depth;
    float m_mult;
    float m_skew;
    float m_shape;
    float m_center;
    float m_slope;
    int m_fourierFrequency = 0;
    double m_fourierSlope = 0;
};

const Settings x_settings[] =
{
    {"linear", 2, 0, 1, .5f, 0, .75f, 0},
    {"gentle", 2, .125f, 1, .5f, 0, 0, 0},
    {"blend", 2, .5f, 2.5f, .35f, .5f, .55f, .25f},
    {"default_full_mult", 2, 1, 16, .5f, .5f, 0, 0},
    {"slow_reversal", 2, 1, 1, 0, 1, 1, 0},
    {"bad_fours", 4, 1, 8, .1f, .75f, 0, 0},
    {"hard_fours", 4, 1, 16, 0, 1, 0, 0},
    {"hard_fives", 5, 1, 16, 0, 1, 0, 0},
    {"hard_fives_reverse_skew", 5, 1, 16, 1, 1, 0, 0},
    {"hard_twos", 2, 1, 16, 0, 1, 0, 0},
    {"hard_blend", 5, 1, 15.5f, 0, 1, .5f, 1},
    {"global_full_mult", 2, 1, 16, 0, 1, 1, 0},
    {"global_mid_mult", 2, 1, 4, 0, 1, 1, 0},
    {"hard_threes", 3, 1, 16, 0, 1, 0, 0},
    {"global_full_mult_reverse_skew", 2, 1, 16, 1, 1, 1, 0},
    {"global_mid_mult_reverse_skew", 2, 1, 4, 1, 1, 1, 0},
    {"hard_twos_reverse_skew", 2, 1, 16, 1, 1, 0, 0},
    {"limited_gentle", 2, 0, 1, .5f, 0, 0, 0, 32, .25},
    {"limited_reversing", 2, 0, 1, .5f, 0, 0, 0, 512, 4},
    {"limited_fast", 2, 0, 1, .5f, 0, 0, 0, 16384, 4},
    {"limited_extreme", 2, 0, 1, .5f, 0, 0, 0, 50000, 16},
    {"limited_cluster", 2, 0, 1, .5f, 0, 0, 0, -3, 0}
};

struct Trace
{
    Settings m_settings;
    double m_period;
    std::vector<double> m_phase;
    std::array<double, TheoryOfTimeBase::x_numLoops> m_loopPeriods;
};

// Exercise the production PolyXFader and clock, with the same ordering and
// inverse-ratio weights as TheoryOfTime::ProcessPhaseModLFO. Recording G is
// intentionally separate so both implementations receive identical samples.
//
Trace Generate(Settings settings, double period, double cycles)
{
    TheoryOfTimeBase time;
    TheoryOfTimeBase::Input clock;
    clock.m_running = true;
    time.m_globalPeriodSamples = period;
    for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
    {
        clock.m_input[i].m_parentMult = settings.m_chain;
    }

    PolyXFaderInternal lfo;
    PolyXFaderInternal::Input input;
    input.m_theoryOfTime = &time;
    input.m_size = TheoryOfTimeBase::x_numLoops;
    input.m_phaseDomain = PhaseDomain::Unmodulated;
    input.m_phaseShift = -.75f;
    input.m_attackFrac = .1f + .8f * settings.m_skew;
    input.m_shape = settings.m_shape;
    input.m_mult = settings.m_mult;
    input.m_center = settings.m_center;
    input.m_slope = settings.m_slope;
    Trace trace{settings, period, {}, {}};
    size_t count = static_cast<size_t>(std::max(period * cycles, 2.0 * SampleTimer::x_sampleRate));
    trace.m_phase.reserve(count);
    if (settings.m_fourierFrequency != 0)
    {
        for (size_t n = 0; n < count; ++n)
        {
            double u = static_cast<double>(n) / period;
            double q = u;
            if (settings.m_fourierFrequency > 0)
            {
                double omega = 2 * M_PI * settings.m_fourierFrequency;
                q += settings.m_fourierSlope / omega * std::sin(omega * u);
            }
            else
            {
                // F' is a normalized product of three shifted cosines.
                // This degree-three Fourier warp clusters turns in phase.
                //
                double a = std::cos(.2);
                double b = std::cos(.3);
                double c = std::cos(.4);
                double sum = a + b + c;
                double scale = 1.0 / (-sum / 2 - a * b * c);
                double coefficient[] = {scale * (.75 + a * b + b * c + a * c),
                    -scale * sum / 2, scale / 4};
                for (size_t harmonic = 1; harmonic <= 3; ++harmonic)
                {
                    double omega = 2 * M_PI * harmonic;
                    q += coefficient[harmonic - 1] / omega * std::sin(omega * u);
                }
            }

            trace.m_phase.push_back(q);
        }

        for (size_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
        {
            trace.m_loopPeriods[i] = std::pow(static_cast<double>(settings.m_chain),
                -static_cast<double>(TheoryOfTimeBase::x_globalLoop - i));
        }

        return trace;
    }

    for (size_t n = 0; n < count; ++n)
    {
        clock.m_unmodulatedPhase += 1.0 / period;
        for (size_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
        {
            input.m_externalWeights[i] = 1.0 / static_cast<double>(time.GetCycleRatio(i));
        }

        lfo.Process(input);
        clock.m_phaseOffset = -2.0f * settings.m_depth * lfo.m_rawOutput[0];
        time.Process(clock);
        trace.m_phase.push_back(time.m_phaseClock.m_phase);
    }

    for (size_t i = 0; i < TheoryOfTimeBase::x_numLoops; ++i)
    {
        trace.m_loopPeriods[i] = 1.0 / time.GetCycleRatio(i);
    }

    return trace;
}

// An independent reference keeps complete sampled ascending runs. It clips
// overwritten coordinate intervals, but never grids or resamples the inverse.
// Lookup binary-searches the original forward samples within the latest run.
//
struct ExactInverse
{
    struct Run
    {
        double m_end;
        size_t m_first;
        size_t m_last;
    };

    const std::vector<double>& m_phase;
    std::map<double, Run> m_runs;
    size_t m_first = 0;
    size_t m_last = 0;
    bool m_live = false;

    explicit ExactInverse(const std::vector<double>& phase) : m_phase(phase)
    {
    }

    void Commit()
    {
        double low = m_phase[m_first];
        double high = m_phase[m_last];
        if (low == high)
        {
            return;
        }

        auto it = m_runs.upper_bound(low);
        if (it != m_runs.begin())
        {
            auto previous = std::prev(it);
            Run old = previous->second;
            if (old.m_end > low)
            {
                previous->second.m_end = low;
                if (old.m_end > high)
                {
                    m_runs.emplace(high, old);
                }
            }
        }

        it = m_runs.lower_bound(low);
        while (it != m_runs.end() && it->first < high)
        {
            Run old = it->second;
            it = m_runs.erase(it);
            if (old.m_end > high)
            {
                m_runs.emplace(high, old);
            }
        }

        m_runs[low] = {high, m_first, m_last};
    }

    void Advance(size_t n)
    {
        if (n == 0)
        {
            return;
        }

        if (m_phase[n] > m_phase[n - 1])
        {
            if (!m_live)
            {
                m_first = n - 1;
                m_live = true;
            }

            m_last = n;
        }
        else if (m_phase[n] < m_phase[n - 1] && m_live)
        {
            Commit();
            m_live = false;
        }
    }

    double Search(double q, size_t first, size_t last) const
    {
        if (q == m_phase[last])
        {
            return static_cast<double>(last);
        }

        auto upper = std::upper_bound(m_phase.begin() + first, m_phase.begin() + last + 1, q);
        size_t index = static_cast<size_t>(upper - m_phase.begin()) - 1;
        return index + (q - m_phase[index]) / (m_phase[index + 1] - m_phase[index]);
    }

    double Read(double q) const
    {
        double result = x_nan;
        auto include = [&result](double time)
        {
            result = std::isfinite(result) ? std::max(result, time) : time;
        };
        if (m_live && m_phase[m_first] <= q && q <= m_phase[m_last])
        {
            include(Search(q, m_first, m_last));
        }

        auto it = m_runs.upper_bound(q);
        if (it != m_runs.begin())
        {
            --it;
            if (q <= it->second.m_end)
            {
                include(Search(q, it->second.m_first, it->second.m_last));
            }

            if (it->first == q && it != m_runs.begin())
            {
                --it;
                if (q == it->second.m_end)
                {
                    include(Search(q, it->second.m_first, it->second.m_last));
                }
            }
        }

        return result;
    }
};

double Percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
    {
        return 0;
    }

    size_t index = static_cast<size_t>((values.size() - 1) * fraction);
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

struct Errors
{
    size_t m_queries = 0;
    size_t m_available = 0;
    size_t m_missingOld = 0;
    size_t m_missingNew = 0;
    size_t m_spuriousOld = 0;
    size_t m_spuriousNew = 0;
    size_t m_oldGoodNewMissing = 0;
    size_t m_firstMissingSample = 0;
    double m_firstMissingQ = 0;
    size_t m_oldGoodNewOver[4]{};
    std::vector<double> m_old;
    std::vector<double> m_new;
    double m_worstRegression = 0;
    double m_worstQ = 0;
    size_t m_worstSample = 0;
    double m_worstOld = 0;
    double m_worstNew = 0;
    double m_worstReference = 0;

    void Add(double before, double after, double reference, size_t sample, double q)
    {
        ++m_queries;
        if (!std::isfinite(reference))
        {
            m_spuriousOld += std::isfinite(before);
            m_spuriousNew += std::isfinite(after);
            return;
        }

        ++m_available;
        m_missingOld += !std::isfinite(before);
        m_missingNew += !std::isfinite(after);
        if (std::isfinite(before) && std::abs(before - reference) <= 1.0 && !std::isfinite(after))
        {
            if (m_oldGoodNewMissing == 0)
            {
                m_firstMissingSample = sample;
                m_firstMissingQ = q;
            }

            ++m_oldGoodNewMissing;
        }

        if (std::isfinite(before))
        {
            m_old.push_back(std::abs(before - reference));
        }

        if (std::isfinite(after))
        {
            m_new.push_back(std::abs(after - reference));
        }

        if (std::isfinite(before) && std::isfinite(after) && std::abs(before - reference) <= 1.0)
        {
            const double limits[] = {1, 16, 48, 1024};
            double error = std::abs(after - reference);
            for (size_t i = 0; i < 4; ++i)
            {
                m_oldGoodNewOver[i] += error > limits[i];
            }

            if (error > m_worstRegression)
            {
                m_worstRegression = error;
                m_worstQ = q;
                m_worstSample = sample;
                m_worstOld = before;
                m_worstNew = after;
                m_worstReference = reference;
            }
        }
    }

    void Print(const std::string& name, const char* kind) const
    {
        std::cout << "{\"case\":\"" << name << "\",\"kind\":\"" << kind
            << "\",\"queries\":" << m_queries << ",\"available\":" << m_available
            << ",\"missing_old\":" << m_missingOld << ",\"missing_new\":" << m_missingNew
            << ",\"spurious_old\":" << m_spuriousOld << ",\"spurious_new\":" << m_spuriousNew
            << ",\"old_good_new_missing\":" << m_oldGoodNewMissing
            << ",\"first_missing_sample\":" << m_firstMissingSample
            << ",\"first_missing_q\":" << m_firstMissingQ
            << ",\"old_p99\":" << Percentile(m_old, .99) << ",\"new_p99\":" << Percentile(m_new, .99)
            << ",\"old_max\":" << Percentile(m_old, 1) << ",\"new_max\":" << Percentile(m_new, 1)
            << ",\"old_le1_new_gt1\":" << m_oldGoodNewOver[0]
            << ",\"old_le1_new_gt16\":" << m_oldGoodNewOver[1]
            << ",\"old_le1_new_gt48\":" << m_oldGoodNewOver[2]
            << ",\"old_le1_new_gt1024\":" << m_oldGoodNewOver[3]
            << ",\"worst_regression\":" << m_worstRegression
            << ",\"worst_q\":" << m_worstQ << ",\"worst_sample\":" << m_worstSample
            << ",\"worst_old\":" << m_worstOld << ",\"worst_new\":" << m_worstNew
            << ",\"worst_reference\":" << m_worstReference << "}" << std::endl;
    }
};

double ReadPhase(double write, double period, double speed, double fraction)
{
    return PhaseUtils::WrapMod(write - period, write, write * speed - period * fraction);
}

double SafeStart(double time, size_t sample)
{
    if (!std::isfinite(time))
    {
        return x_nan;
    }

    double latest = static_cast<double>(sample / SampleTimer::x_oversample) - 4097.0;
    double oldest = std::max(0.0, static_cast<double>(sample / SampleTimer::x_oversample)
        - static_cast<double>((1 << 24) - 1));
    time = std::min(time, latest);
    return time >= oldest + 1025.0 ? time : x_nan;
}

void Correctness(const Trace& trace, const std::string& name)
{
    auto old = std::make_unique<Legacy>();
    auto current = std::make_unique<Current>(TheoryOfTime::x_phaseResolution);
    auto prior = std::make_unique<Prior>(TheoryOfTime::x_phaseResolution);
    ExactInverse reference(trace.m_phase);
    Errors raw;
    Errors safe;
    Errors steady;
    Errors scope;
    Errors priorRaw;
    Errors priorSafe;
    Errors priorSteady;
    Errors priorScope;
    double maxPhaseError = 0;
    size_t overCell = 0;
    size_t noncausal = 0;
    const double speeds[] = {-4, -3, -2, -1.5, -4.0 / 3.0, -1, -.5, -.25,
        .25, .5, 1, 4.0 / 3.0, 1.5, 2, 3, 4};
    const double fractions[] = {.8, 2.0 / 3.0, 1, .75, .625};
    for (size_t n = 0; n < trace.m_phase.size(); ++n)
    {
        double write = trace.m_phase[n];
        old->Write(write * trace.m_period, static_cast<double>(n));
        current->Write(write, static_cast<double>(n));
        prior->Write(write, static_cast<double>(n));
        reference.Advance(n);
        if (n % x_hop != 0)
        {
            continue;
        }

        for (double period : trace.m_loopPeriods)
        {
            for (double speed : speeds)
            {
                for (double fraction : fractions)
                {
                    double q = ReadPhase(write, period, speed, fraction);
                    double before = old->GetRealTime(q * trace.m_period) / SampleTimer::x_oversample;
                    double after = current->GetRealTime(q) / SampleTimer::x_oversample;
                    double previous = prior->GetRealTime(q) / SampleTimer::x_oversample;
                    double exact = reference.Read(q) / SampleTimer::x_oversample;
                    raw.Add(before, after, exact, n, q);
                    priorRaw.Add(previous, after, exact, n, q);
                    double engineTime = after * SampleTimer::x_oversample;
                    noncausal += std::isfinite(engineTime) && (engineTime < 0 || engineTime > n + .001);
                    if (std::isfinite(engineTime) && engineTime >= 0 && engineTime <= n && n > 0)
                    {
                        size_t i = std::min(static_cast<size_t>(std::floor(engineTime)), n - 1);
                        double value = trace.m_phase[i] + (engineTime - i) * (trace.m_phase[i + 1] - trace.m_phase[i]);
                        double cells = std::abs(value - q) * TheoryOfTime::x_phaseResolution;
                        maxPhaseError = std::max(maxPhaseError, cells);
                        overCell += cells > 1.0001;
                    }
                    before = SafeStart(before, n);
                    after = SafeStart(after, n);
                    exact = SafeStart(exact, n);
                    previous = SafeStart(previous, n);
                    safe.Add(before, after, exact, n, q);
                    priorSafe.Add(previous, after, exact, n, q);
                    if (n >= 2 * trace.m_period)
                    {
                        steady.Add(before, after, exact, n, q);
                        priorSteady.Add(previous, after, exact, n, q);
                    }
                }
            }

            PeriodicPhaseAxis axis{-1.0 / trace.m_period, period};
            for (size_t point = 0; point < 32; ++point)
            {
                double q = axis.Coordinate(write, point / 32.0);
                double before = old->ReadInverse(q * trace.m_period, n) / SampleTimer::x_oversample;
                double after = current->ReadInverse(q, n) / SampleTimer::x_oversample;
                double exact = reference.Read(q) / SampleTimer::x_oversample;
                scope.Add(before, after, exact, n, q);
                priorScope.Add(prior->ReadInverse(q, n) / SampleTimer::x_oversample, after, exact, n, q);
            }
        }
    }

    raw.Print(name, "delay_raw_host_frames");
    safe.Print(name, "delay_safe_host_frames");
    steady.Print(name, "delay_steady_host_frames");
    scope.Print(name, "scope_time_host_frames");
    priorRaw.Print(name, "prior_delay_raw_host_frames");
    priorSafe.Print(name, "prior_delay_safe_host_frames");
    priorSteady.Print(name, "prior_delay_steady_host_frames");
    priorScope.Print(name, "prior_scope_time_host_frames");
    std::cout << "{\"case\":\"" << name << "\",\"kind\":\"storage\",\"payload_bytes\":" << Current::x_payloadBytes
        << ",\"max_cell_pieces\":" << Current::x_maxCellPieces
        << ",\"island_reassignments\":" << current->m_islandReassignments
        << ",\"noncausal\":" << noncausal
        << ",\"max_phase_error_cells\":" << maxPhaseError
        << ",\"over_one_cell\":" << overCell << "}" << std::endl;
}

template<typename Inverse>
void TimeOne(Inverse& inverse, const Trace& trace, const std::string& name,
    const char* implementation, double scale, int repeat)
{
    const size_t warm = SampleTimer::x_sampleRate / 2 / x_hop * x_hop;
    std::vector<double> blockCPU;
    std::vector<double> blockWall;
    double checksum = 0;
    size_t unavailable = 0;
    double totalCPU = 0;
    size_t count = trace.m_phase.size() / x_hop * x_hop;
    for (size_t start = 0; start < count; start += x_hop)
    {
        auto wall = std::chrono::steady_clock::now();
        double cpu = CPUSeconds();
        for (size_t n = start; n < start + x_hop; ++n)
        {
            inverse.Write(trace.m_phase[n] * scale, static_cast<double>(n));
            if (n == start)
            {
                const double speeds[] = {-4, -.5, 1, 4};
                const double fractions[] = {.8, 2.0 / 3.0, 1, .625};
                for (size_t lane = 0; lane < 4; ++lane)
                {
                    double q = ReadPhase(trace.m_phase[n], trace.m_loopPeriods[lane], speeds[lane], fractions[lane]);
                    double read = inverse.GetRealTime(q * scale);
                    if (std::isfinite(read))
                    {
                        checksum += read * 1e-9;
                    }
                    else
                    {
                        ++unavailable;
                    }
                }
            }
        }

        double elapsed = CPUSeconds() - cpu;
        if (start >= warm)
        {
            totalCPU += elapsed;
            blockCPU.push_back(elapsed * 1e6);
            blockWall.push_back(std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - wall).count());
        }
    }

    std::cout << "{\"case\":\"" << name << "\",\"kind\":\"performance\",\"implementation\":\""
        << implementation << "\",\"repeat\":" << repeat << ",\"cpu_ms_per_audio_second\":"
        << totalCPU * 1000.0 * SampleTimer::x_sampleRate / (count - warm)
        << ",\"block_cpu_p50_us\":" << Percentile(blockCPU, .5)
        << ",\"block_cpu_p99_us\":" << Percentile(blockCPU, .99)
        << ",\"block_cpu_max_us\":" << Percentile(blockCPU, 1)
        << ",\"block_wall_max_us\":" << Percentile(blockWall, 1)
        << ",\"checksum\":" << checksum << ",\"unavailable\":" << unavailable << "}" << std::endl;
}

void WriteCorrectness(const Trace& trace, const std::string& name)
{
    auto current = std::make_unique<Current>(TheoryOfTime::x_phaseResolution);
    auto landed = std::make_unique<Landed>(TheoryOfTime::x_phaseResolution);
    size_t queries = 0;
    auto compare = [&](double phase, double cutoff)
    {
        double a = current->ReadInverse(phase, cutoff);
        double b = landed->ReadInverse(phase, cutoff);
        if (!(a == b || (std::isnan(a) && std::isnan(b))))
        {
            throw std::runtime_error("Landed/current inverse mismatch at " + std::to_string(cutoff));
        }

        ++queries;
    };
    for (size_t n = 0; n < trace.m_phase.size(); ++n)
    {
        current->Write(trace.m_phase[n], static_cast<double>(n));
        landed->Write(trace.m_phase[n], static_cast<double>(n));
        if (n % x_hop == 0)
        {
            for (size_t k = 0; k <= 128; ++k)
            {
                double q = trace.m_phase[n] - static_cast<double>(k) / 128;
                compare(q, n);
                compare(q, static_cast<double>(n) - x_hop);
            }

            for (double offset : {-1.25, -.75, -.25, 0.0, .25})
            {
                compare(trace.m_phase[n] + offset / TheoryOfTime::x_phaseResolution, n);
            }

            const double speeds[] = {-4, -.5, 1, 4};
            const double fractions[] = {.8, 2.0 / 3.0, 1, .625};
            for (size_t lane = 0; lane < 4; ++lane)
            {
                compare(ReadPhase(trace.m_phase[n], trace.m_loopPeriods[lane], speeds[lane], fractions[lane]), n);
            }
        }
    }

    std::cout << "{\"case\":\"" << name << "\",\"kind\":\"write_correctness\",\"queries\":" << queries
        << ",\"mismatches\":0}" << std::endl;
}

template<typename Inverse>
void TimeContended(Inverse& inverse, const Trace& trace, const std::string& name,
    const char* implementation, int repeat)
{
    std::atomic<bool> ready{false};
    std::atomic<bool> done{false};
    std::atomic<double> query{trace.m_phase.front()};
    std::vector<double> latency;
    latency.reserve(1000000);
    double checksum = 0;
    double readerCPU = 0;
    size_t reads = 0;
    std::thread reader([&]
    {
        PrepareCPU();
        double cpu = CPUSeconds();
        ready.store(true);
        while (!done.load())
        {
            double q = query.load();
            auto start = std::chrono::steady_clock::now();
            double value = inverse.GetRealTime(q);
            double elapsed = std::chrono::duration<double, std::micro>(
                std::chrono::steady_clock::now() - start).count();
            latency.push_back(elapsed);
            if (std::isfinite(value))
            {
                checksum += value * 1e-9;
            }

            ++reads;
        }

        readerCPU = CPUSeconds() - cpu;
    });
    while (!ready.load())
    {
        std::this_thread::yield();
    }

    double cpu = CPUSeconds();
    for (size_t n = 0; n < trace.m_phase.size(); ++n)
    {
        inverse.Write(trace.m_phase[n], static_cast<double>(n));
        if (n % x_hop == 0)
        {
            query.store(trace.m_phase[n] - .125);
        }
    }

    double writerCPU = CPUSeconds() - cpu;
    done.store(true);
    reader.join();
    std::cout << "{\"case\":\"" << name << "\",\"kind\":\"contention\",\"implementation\":\""
        << implementation << "\",\"repeat\":" << repeat << ",\"writer_cpu_ms_per_audio_second\":"
        << writerCPU * 1000.0 * SampleTimer::x_sampleRate / trace.m_phase.size()
        << ",\"reader_cpu_ms\":" << readerCPU * 1000 << ",\"reads\":" << reads
        << ",\"read_p50_us\":" << Percentile(latency, .5)
        << ",\"read_p99_us\":" << Percentile(latency, .99)
        << ",\"read_max_us\":" << Percentile(latency, 1)
        << ",\"checksum\":" << checksum << "}" << std::endl;
}

void Work(const Trace& trace, const std::string& name)
{
    const char* names[] = {"legacy", "prior_phase", "fixed_cell"};
    for (size_t implementation = 0; implementation < std::size(names); ++implementation)
    {
        double density = implementation == 0 ? trace.m_period : TheoryOfTime::x_phaseResolution;
        uint64_t total = 0;
        uint64_t peak = 0;
        uint64_t block = 0;
        uint64_t peakBlock = 0;
        for (size_t n = 1; n < trace.m_phase.size(); ++n)
        {
            double before = trace.m_phase[n - 1] * density;
            double after = trace.m_phase[n] * density;
            double edges = implementation == 2
                ? std::floor(after) - std::ceil(before) + 1
                : std::ceil(after) - std::ceil(before);
            uint64_t writes = after > before ? static_cast<uint64_t>(edges) : 0;
            total += writes;
            block += writes;
            peak = std::max(peak, writes);
            if ((n + 1) % x_hop == 0)
            {
                peakBlock = std::max(peakBlock, block);
                block = 0;
            }
        }

        peakBlock = std::max(peakBlock, block);
        std::cout << "{\"case\":\"" << name << "\",\"kind\":\"scatter_work\",\"implementation\":\""
            << names[implementation] << "\",\"density\":" << density
            << ",\"grid_visits_per_audio_second\":" << total * static_cast<double>(SampleTimer::x_sampleRate) / trace.m_phase.size()
            << ",\"max_grid_visits_per_sample\":" << peak << ",\"max_grid_visits_per_block\":" << peakBlock << "}" << std::endl;
    }
}

void SelfTest()
{
    std::mt19937 random(19);
    std::vector<double> phase{0};
    for (size_t n = 1; n < 4000; ++n)
    {
        phase.push_back(phase.back() + (static_cast<int>(random() % 17) - 8) / 8.0);
    }

    ExactInverse reference(phase);
    for (size_t n = 0; n < phase.size(); ++n)
    {
        reference.Advance(n);
        for (size_t test = 0; test < 8; ++test)
        {
            double q = phase[n] + (static_cast<int>(random() % 200) - 100) / 16.0;
            double expected = x_nan;
            for (size_t i = n; i > 0; --i)
            {
                if (phase[i - 1] <= q && q <= phase[i] && phase[i - 1] < phase[i])
                {
                    expected = i - 1 + (q - phase[i - 1]) / (phase[i] - phase[i - 1]);
                    break;
                }
            }

            double actual = reference.Read(q);
            if (std::isfinite(expected) != std::isfinite(actual)
                || (std::isfinite(expected) && std::abs(expected - actual) > 1e-9))
            {
                throw std::runtime_error("Exact inverse disagrees with independent backwards search");
            }
        }
    }

    for (const auto& settings : {x_settings[1], x_settings[7], x_settings[10]})
    {
        Trace trace = Generate(settings, 65536, .01);
        TheoryOfTime actual;
        TheoryOfTime::Input input;
        input.m_running = true;
        input.m_freq = 1.0 / 65536;
        input.m_modIndex.m_expParam = settings.m_depth;
        input.m_lfoMult.m_expParam = settings.m_mult;
        input.m_phaseModLFOInput.m_attackFrac = settings.m_skew;
        input.m_phaseModLFOInput.m_shape = settings.m_shape;
        input.m_phaseModLFOInput.m_center = settings.m_center;
        input.m_phaseModLFOInput.m_slope = settings.m_slope;
        for (size_t i = 0; i < TheoryOfTimeBase::x_globalLoop; ++i)
        {
            input.m_input[i].m_parentMult = settings.m_chain;
        }

        for (size_t n = 0; n < 8192; ++n)
        {
            SampleTimer::s_instance->m_sample = n;
            actual.Process(input);
            if (actual.m_phaseClock.m_phase != trace.m_phase[n])
            {
                throw std::runtime_error("Trace generator differs from production TheoryOfTime");
            }
        }
    }

    std::cout << "{\"kind\":\"self_test\",\"status\":\"passed\"}" << std::endl;
}

std::string JSONNumber(double value)
{
    if (!std::isfinite(value))
    {
        return "null";
    }

    std::ostringstream text;
    text << std::setprecision(17) << value;
    return text.str();
}

void Inspect(const Trace& trace, size_t sample, double q)
{
    if (sample >= trace.m_phase.size() || !std::isfinite(q))
    {
        throw std::runtime_error("Inspection query is outside the generated trace");
    }

    auto old = std::make_unique<Legacy>();
    auto current = std::make_unique<Current>(TheoryOfTime::x_phaseResolution);
    ExactInverse reference(trace.m_phase);
    int64_t index = static_cast<int64_t>(std::floor(q * TheoryOfTime::x_phaseResolution));
    size_t slot = Current::WrapIndex(index);
    std::array<double, 5> previous{x_nan, x_nan, x_nan, x_nan, x_nan};
    for (size_t n = 0; n <= sample; ++n)
    {
        old->Write(trace.m_phase[n] * trace.m_period, n);
        current->Write(trace.m_phase[n], n);
        reference.Advance(n);
        std::array<double, 5> state{current->m_writeHeadInverse[slot].load(),
            current->m_fallbackRightTime[slot].load(), current->m_turnPhase[slot].load(),
            current->m_turnTime[slot].load(), current->m_currentRightTime[slot].load()};
        bool changed = false;
        for (size_t i = 0; i < state.size(); ++i)
        {
            changed |= state[i] != previous[i]
                && !(std::isnan(state[i]) && std::isnan(previous[i]));
        }

        if (changed)
        {
            std::cout << "{\"kind\":\"cell_transition\",\"sample\":" << n
                << ",\"writer_cell_position\":" << trace.m_phase[n] * TheoryOfTime::x_phaseResolution - index
                << ",\"fallback_start\":" << JSONNumber(state[0])
                << ",\"fallback_end\":" << JSONNumber(state[1])
                << ",\"turn_cell_position\":" << JSONNumber(state[2] - index)
                << ",\"replacement_start\":" << JSONNumber(state[0] + state[3])
                << ",\"replacement_end\":" << JSONNumber(state[0] + state[4])
                << ",\"query_time\":" << JSONNumber(current->GetRealTime(q)) << "}" << std::endl;
            previous = state;
        }
    }

    double brute = x_nan;
    for (size_t n = sample; n > 0; --n)
    {
        if (trace.m_phase[n - 1] <= q && q <= trace.m_phase[n] && trace.m_phase[n - 1] < trace.m_phase[n])
        {
            brute = n - 1 + (q - trace.m_phase[n - 1]) / (trace.m_phase[n] - trace.m_phase[n - 1]);
            break;
        }
    }

    double exact = reference.Read(q);
    if (std::isfinite(brute) != std::isfinite(exact) || (std::isfinite(brute) && brute != exact))
    {
        throw std::runtime_error("Full trace backwards search differs from oracle");
    }

    if (std::isfinite(brute))
    {
        size_t crossing = static_cast<size_t>(std::floor(brute));
        size_t firstSample = crossing > 8 ? crossing - 8 : 0;
        for (size_t n = firstSample; n <= std::min(crossing + 12, sample); ++n)
        {
            std::cout << "{\"kind\":\"crossing_observation\",\"sample\":" << n
                << ",\"cell_position\":" << trace.m_phase[n] * TheoryOfTime::x_phaseResolution - index
                << ",\"phase_increment\":" << (n > 0 ? trace.m_phase[n] - trace.m_phase[n - 1] : 0)
                << "}" << std::endl;
        }
    }

    double first = current->m_writeHeadInverse[Current::WrapIndex(index)].load();
    double second = current->m_fallbackRightTime[slot].load();
    std::cout << "{\"kind\":\"inspection\",\"sample\":" << sample << ",\"q\":" << q
        << ",\"brute_engine_time\":" << JSONNumber(brute) << ",\"reference_engine_time\":" << JSONNumber(exact)
        << ",\"old_engine_time\":" << JSONNumber(old->GetRealTime(q * trace.m_period))
        << ",\"new_engine_time\":" << JSONNumber(current->GetRealTime(q))
        << ",\"current_left_q\":" << index / TheoryOfTime::x_phaseResolution
        << ",\"current_right_q\":" << (index + 1) / TheoryOfTime::x_phaseResolution
        << ",\"current_left_engine_time\":" << JSONNumber(first) << ",\"current_right_engine_time\":" << JSONNumber(second)
        << ",\"writer_q\":" << trace.m_phase[sample] << "}" << std::endl;

    // Inspect the selected fixed-cell traversal. The latest-forward oracle is
    // a timestamp diagnostic; reassignment may deliberately choose an older run.
    //
    double seed = current->m_seedPhase.load();
    double lowPhase = std::floor(seed) == index ? seed : static_cast<double>(index);
    double highPhase = static_cast<double>(index) + 1;
    double lowTime = first;
    double highTime = second;
    double turn = current->m_turnPhase[slot].load();
    bool replacement = std::isfinite(turn) && q * TheoryOfTime::x_phaseResolution >= turn;
    if (replacement)
    {
        lowPhase = turn;
        lowTime = first + current->m_turnTime[slot].load();
        highTime = first + current->m_currentRightTime[slot].load();
    }

    double frontier = current->m_frontierPhase.load();
    if (!std::isfinite(highTime) && lowPhase <= frontier && frontier < highPhase)
    {
        highPhase = frontier;
        highTime = current->m_frontierTime.load();
    }

    std::cout << "{\"kind\":\"cell_inspection\",\"replacement\":" << (replacement ? "true" : "false")
        << ",\"turn_cells\":" << JSONNumber(turn)
        << ",\"island_reassignments\":" << current->m_islandReassignments << "}" << std::endl;
    if (std::isfinite(exact))
    {
        size_t runFirst = static_cast<size_t>(std::floor(exact));
        size_t runLast = static_cast<size_t>(std::ceil(exact));
        while (runFirst > 0 && trace.m_phase[runFirst - 1] <= trace.m_phase[runFirst])
        {
            --runFirst;
        }

        while (runLast < sample && trace.m_phase[runLast] <= trace.m_phase[runLast + 1])
        {
            ++runLast;
        }

        lowPhase /= TheoryOfTime::x_phaseResolution;
        highPhase /= TheoryOfTime::x_phaseResolution;
        double actual = current->GetRealTime(q);
        double windowPhaseError = 0;
        auto phaseAt = [&trace](double time)
        {
            if (time < 0 || time >= trace.m_phase.size() - 1)
            {
                return x_nan;
            }

            size_t n = static_cast<size_t>(std::floor(time));
            return trace.m_phase[n] + (time - n) * (trace.m_phase[n + 1] - trace.m_phase[n]);
        };
        for (int k = -1024; k < 4096; ++k)
        {
            double offset = static_cast<double>(k) * SampleTimer::x_oversample;
            double difference = std::abs(phaseAt(actual + offset) - phaseAt(exact + offset));
            if (!std::isfinite(difference))
            {
                windowPhaseError = x_nan;
                break;
            }

            windowPhaseError = std::max(windowPhaseError, difference);
        }

        double interpolated = x_nan;
        double trueLow = x_nan;
        double trueHigh = x_nan;
        if (runFirst <= actual && actual <= runLast
            && trace.m_phase[runFirst] <= lowPhase && highPhase <= trace.m_phase[runLast]
            && lowPhase < highPhase)
        {
            trueLow = reference.Search(lowPhase, runFirst, runLast);
            trueHigh = reference.Search(highPhase, runFirst, runLast);
            interpolated = trueLow + (q - lowPhase) / (highPhase - lowPhase) * (trueHigh - trueLow);
        }

        std::cout << "{\"kind\":\"branch_inspection\",\"low_q\":" << lowPhase << ",\"high_q\":" << highPhase
            << ",\"stored_low_engine_time\":" << JSONNumber(lowTime)
            << ",\"stored_high_engine_time\":" << JSONNumber(highTime)
            << ",\"exact_low_engine_time\":" << JSONNumber(trueLow)
            << ",\"exact_high_engine_time\":" << JSONNumber(trueHigh)
            << ",\"forward_run_first\":" << runFirst << ",\"forward_run_last\":" << runLast
            << ",\"result_inside_exact_run\":" << ((runFirst <= actual && actual <= runLast) ? "true" : "false")
            << ",\"paired_window_max_phase_error_cells\":" << JSONNumber(windowPhaseError * TheoryOfTime::x_phaseResolution)
            << ",\"curvature_error_host_frames\":" << JSONNumber((interpolated - exact) / SampleTimer::x_oversample)
            << ",\"endpoint_error_host_frames\":" << JSONNumber((actual - interpolated) / SampleTimer::x_oversample)
            << "}" << std::endl;
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        PrepareCPU();
        SampleTimer::Init(512);
        std::cout << std::setprecision(17);
        if (argc == 2 && std::string(argv[1]) == "self-test")
        {
            SelfTest();
            return 0;
        }

        if (argc < 4 || argc > 6)
        {
            throw std::runtime_error("Usage: perf_density correctness|performance|write-correctness|write-performance|contention|work SETTING PERIOD_BITS [REPEATS] [CYCLES]; inspect SETTING PERIOD_BITS SAMPLE PHASE");
        }

        std::string mode = argv[1];
        int setting = Integer(argv[2]);
        double bits = PositiveSeconds(argv[3]);
        int repeats = mode == "inspect" ? 1 : argc > 4 ? Integer(argv[4]) : 3;
        double cycles = mode == "inspect" ? 3.25 : argc > 5 ? PositiveSeconds(argv[5]) : 3.25;
        if (setting < 0 || setting >= static_cast<int>(std::size(x_settings)) || bits < 16 || bits > 24 || repeats < 1)
        {
            throw std::runtime_error("Invalid density case parameters");
        }

        Trace trace = Generate(x_settings[setting], std::exp2(bits), cycles);
        std::string name = std::string(x_settings[setting].m_name) + "/period2^" + argv[3];
        if (mode == "correctness")
        {
            Correctness(trace, name);
        }
        else if (mode == "performance")
        {
            Work(trace, name);
            for (int repeat = 0; repeat < repeats; ++repeat)
            {
                for (int pass = 0; pass < 3; ++pass)
                {
                    if ((repeat + pass) % 3 == 0)
                    {
                        auto inverse = std::make_unique<Legacy>();
                        TimeOne(*inverse, trace, name, "legacy", trace.m_period, repeat);
                    }
                    else if ((repeat + pass) % 3 == 1)
                    {
                        auto inverse = std::make_unique<Prior>(TheoryOfTime::x_phaseResolution);
                        TimeOne(*inverse, trace, name, "phase_grid", 1.0, repeat);
                    }
                    else
                    {
                        auto inverse = std::make_unique<Current>(TheoryOfTime::x_phaseResolution);
                        TimeOne(*inverse, trace, name, "current", 1.0, repeat);
                    }
                }
            }
        }
        else if (mode == "write-correctness")
        {
            WriteCorrectness(trace, name);
        }
        else if (mode == "write-performance" || mode == "contention")
        {
            for (int repeat = 0; repeat < repeats; ++repeat)
            {
                for (int pass = 0; pass < 2; ++pass)
                {
                    if ((repeat + pass) % 2 == 0)
                    {
                        auto inverse = std::make_unique<Landed>(TheoryOfTime::x_phaseResolution);
                        if (mode == "contention")
                        {
                            TimeContended(*inverse, trace, name, "landed", repeat);
                        }
                        else
                        {
                            TimeOne(*inverse, trace, name, "landed", 1.0, repeat);
                        }
                    }
                    else
                    {
                        auto inverse = std::make_unique<Current>(TheoryOfTime::x_phaseResolution);
                        if (mode == "contention")
                        {
                            TimeContended(*inverse, trace, name, "current", repeat);
                        }
                        else
                        {
                            TimeOne(*inverse, trace, name, "current", 1.0, repeat);
                        }
                    }
                }
            }
        }
        else if (mode == "work")
        {
            Work(trace, name);
        }
        else if (mode == "inspect" && argc == 6)
        {
            Inspect(trace, static_cast<size_t>(Integer(argv[4])), std::stod(argv[5]));
        }
        else
        {
            throw std::runtime_error("Unknown density mode");
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 2;
    }
}

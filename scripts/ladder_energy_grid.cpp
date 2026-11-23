// Research-only fixed-duration energy survey of the detector-disabled ladder.
// Four adjacent cutoff values occupy four double-precision SIMD lanes.
// No approximation replaces tanh, and this does not modify the app or codegen.
//
#include <simd/simd.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace
{
namespace fs = std::filesystem;
using Vector = simd_double4;
constexpr double x_pi = 3.141592653589793238462643383279502884;
constexpr int x_windows = 20;
constexpr int x_scopes = 4;
constexpr int x_scopeMetrics = 6;
constexpr uint64_t x_formatVersion = 2;
constexpr uint64_t x_magic = 0x324752454444414cULL;
std::atomic<bool> g_stop{false};
static_assert(std::atomic<bool>::is_always_lock_free);

void Require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void Stop(int)
{
    g_stop.store(true, std::memory_order_relaxed);
}

Vector Splat(double value)
{
    return {value, value, value, value};
}

uint64_t Mix(uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

double Uniform(uint64_t key)
{
    return static_cast<double>(Mix(key) >> 11) * 0x1.0p-53;
}

uint64_t Hash(const void* data, size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    uint64_t value = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i)
    {
        value = (value ^ bytes[i]) * 1099511628211ULL;
    }

    return value;
}

struct Options
{
    int m_grid = 100;
    int m_starts = 100;
    int m_samples = 192000;
    int m_threads = 28;
    double m_sampleRate = 192000;
    double m_amplitude = 0.25;
    double m_minEffectiveResonance = -1;
    uint64_t m_seed = 20261119;
    uint64_t m_maxJobs = std::numeric_limits<uint64_t>::max();
    fs::path m_output;
    bool m_selfTest = false;
    bool m_planOnly = false;

    uint64_t Jobs() const
    {
        const auto n = static_cast<uint64_t>(m_grid);
        return n*n*n*n/4;
    }

    std::string Identity() const
    {
        std::ostringstream result;
        result << std::setprecision(17) << "format=" << x_formatVersion
            << ";model=real-tanh-detector-off-double-simd;grid=" << m_grid
            << ";starts=" << m_starts << ";samples=" << m_samples
            << ";sample_rate=" << m_sampleRate << ";amplitude=" << m_amplitude
            << ";seed=" << m_seed << ";frequency_range=20,20000;cutoff_range=20,20000"
            << ";frequency_tolerance=0.001;windows=whole-period-at-least-0.05s"
            << ";min_effective_resonance=" << m_minEffectiveResonance;
        return result.str();
    }
};

Options Parse(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argv[i];
        if (key == "--self-test")
        {
            options.m_selfTest = true;
            continue;
        }

        if (key == "--plan")
        {
            options.m_planOnly = true;
            continue;
        }

        Require(i + 1 < argc, "Missing value for " + key);
        const std::string value = argv[++i];
        if (key == "--grid")
        {
            options.m_grid = std::stoi(value);
        }
        else if (key == "--starts")
        {
            options.m_starts = std::stoi(value);
        }
        else if (key == "--samples")
        {
            options.m_samples = std::stoi(value);
        }
        else if (key == "--threads")
        {
            options.m_threads = std::stoi(value);
        }
        else if (key == "--sample-rate")
        {
            options.m_sampleRate = std::stod(value);
        }
        else if (key == "--amplitude")
        {
            options.m_amplitude = std::stod(value);
        }
        else if (key == "--min-effective-resonance")
        {
            options.m_minEffectiveResonance = std::stod(value);
        }
        else if (key == "--seed")
        {
            options.m_seed = std::stoull(value);
        }
        else if (key == "--max-jobs")
        {
            options.m_maxJobs = std::stoull(value);
        }
        else if (key == "--output")
        {
            options.m_output = value;
        }
        else
        {
            throw std::runtime_error("Unknown option: " + key);
        }
    }

    Require(options.m_grid >= 4 && options.m_grid <= 1000 && options.m_grid % 4 == 0,
        "Grid size must be a multiple of four in [4, 1000]");
    Require(options.m_starts > 0 && options.m_threads > 0 && options.m_threads <= 256,
        "Positive starts and 1..256 threads required");
    Require(options.m_samples >= 96000 && options.m_sampleRate == 192000
        && options.m_samples <= 192000, "This survey supports 0.5..1 second at 192 kHz");
    Require(std::isfinite(options.m_amplitude) && options.m_amplitude > 0, "Positive amplitude required");
    Require(std::isfinite(options.m_minEffectiveResonance) && options.m_minEffectiveResonance >= -1,
        "Minimum effective resonance must be finite and at least -1");
    Require(options.m_selfTest || options.m_planOnly || !options.m_output.empty(), "--output is required");
    return options;
}

struct Interval
{
    int m_begin;
    int m_end;
};

struct Segment
{
    int m_begin;
    int m_end;
    int m_window;
    unsigned m_scopeMask;
};

struct Frequency
{
    double m_target;
    double m_actual;
    int m_period;
    int m_cycles;
    int m_windowSamples;
    int m_fullWindows;
    int m_tailWindow;
    std::vector<double> m_input;
    std::vector<double> m_quadrature;
    std::array<Interval, x_scopes> m_scopes;
    std::vector<Segment> m_segments;
};

Frequency PrepareFrequency(double target, const Options& options)
{
    Frequency result{};
    result.m_target = target;
    for (int cycles = 1; cycles <= 10000; ++cycles)
    {
        int period = static_cast<int>(std::llround(options.m_sampleRate * cycles / target));
        const double actual = options.m_sampleRate * cycles / period;
        if (std::abs(actual / target - 1) <= 0.001)
        {
            const int divisor = std::gcd(cycles, period);
            result.m_period = period / divisor;
            result.m_cycles = cycles / divisor;
            result.m_actual = actual;
            break;
        }
    }

    Require(result.m_period > 0, "Could not prepare repeating input frequency");
    result.m_windowSamples = result.m_period * ((9600 + result.m_period - 1) / result.m_period);
    result.m_fullWindows = options.m_samples / result.m_windowSamples;
    result.m_tailWindow = (options.m_samples/2 + result.m_windowSamples - 1) / result.m_windowSamples;
    Require(result.m_fullWindows <= x_windows && result.m_fullWindows - result.m_tailWindow >= 2,
        "Insufficient complete late windows");
    std::vector<double> period(static_cast<size_t>(result.m_period));
    std::vector<double> quadrature(static_cast<size_t>(result.m_period));
    for (int n = 0; n < result.m_period; ++n)
    {
        const auto phase = static_cast<int64_t>(n) * result.m_cycles % result.m_period;
        period[n] = options.m_amplitude * std::cos(2*x_pi*phase/result.m_period);
        quadrature[n] = options.m_amplitude * std::sin(2*x_pi*phase/result.m_period);
    }

    result.m_input.resize(options.m_samples);
    result.m_quadrature.resize(options.m_samples);
    for (int n = 0; n < options.m_samples; ++n)
    {
        result.m_input[n] = period[n % result.m_period];
        result.m_quadrature[n] = quadrature[n % result.m_period];
    }

    const int q = result.m_period;
    const int complete = options.m_samples/q*q;
    result.m_scopes = {Interval{0, options.m_samples/4/q*q},
        Interval{0, options.m_samples/2/q*q}, Interval{0, complete},
        Interval{(options.m_samples/2+q-1)/q*q, complete}};
    std::vector<int> boundaries{0, options.m_samples/2, options.m_samples};
    for (const auto& scope : result.m_scopes)
    {
        Require(scope.m_end > scope.m_begin, "Empty measurement scope");
        boundaries.push_back(scope.m_begin);
        boundaries.push_back(scope.m_end);
    }

    for (int end = result.m_windowSamples; end < options.m_samples; end += result.m_windowSamples)
    {
        boundaries.push_back(end);
    }

    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
    for (size_t i = 1; i < boundaries.size(); ++i)
    {
        Segment segment{boundaries[i-1], boundaries[i], boundaries[i-1]/result.m_windowSamples, 0};
        for (int scope = 0; scope < x_scopes; ++scope)
        {
            if (segment.m_begin >= result.m_scopes[scope].m_begin && segment.m_end <= result.m_scopes[scope].m_end)
            {
                segment.m_scopeMask |= 1u << scope;
            }
        }

        result.m_segments.push_back(segment);
    }

    return result;
}

struct Coefficients
{
    Vector m_alpha;
    Vector m_decay;
    Vector m_feedback;
    Vector m_gain;
    Vector m_inverseTanh;
    Vector m_outputGain;
    std::array<Vector, 4> m_bounds;
};

Coefficients Prepare(double drive, double resonance, const std::array<double, 4>& cutoffs)
{
    Coefficients result;
    const double gain = 0.25 * std::pow(20, drive);
    const double inverse = 1 / std::tanh(gain);
    const double slope = gain * inverse;
    const double requested = 3.9 * std::expm1(std::log(49) * resonance) / 48;
    result.m_gain = Splat(gain);
    result.m_inverseTanh = Splat(inverse);
    for (int lane = 0; lane < 4; ++lane)
    {
        const double sine = std::sin(x_pi * cutoffs[lane] / 192000);
        const double alpha = 2*sine*(std::sqrt(1+sine*sine)-sine);
        const double ratio = alpha/(2-alpha)*slope;
        const double safe = 0.9/(ratio*ratio*ratio*ratio+1e-8);
        result.m_alpha[lane] = alpha;
        result.m_feedback[lane] = std::min(requested, safe);
    }

    result.m_decay = Splat(1) - result.m_alpha;
    result.m_outputGain = Splat(1) + result.m_feedback;
    result.m_bounds = {Splat(0), Splat(inverse), Splat(inverse), Splat(inverse)};
    return result;
}

struct WorkPlan
{
    int m_grid;
    std::vector<uint32_t> m_parameterJobs;

    uint64_t Jobs() const
    {
        return static_cast<uint64_t>(m_grid)*m_parameterJobs.size();
    }

    uint64_t JobAt(uint64_t index) const
    {
        const uint64_t perFrequency = static_cast<uint64_t>(m_grid)*m_grid*m_grid/4;
        return index/m_parameterJobs.size()*perFrequency + m_parameterJobs[index%m_parameterJobs.size()];
    }
};

// Selection uses the actual capped feedback k and s=g/tanh(g). For the requested
// threshold and cutoff range every four-cutoff batch is wholly in or out.
// Reject a different threshold that cuts a batch instead of silently simulating
// excluded lanes or dropping eligible configurations.
//
WorkPlan Plan(const Options& options)
{
    WorkPlan result{options.m_grid, {}};
    const int groups = options.m_grid/4;
    std::vector<std::array<double, 4>> cutoffs(groups);
    for (int group = 0; group < groups; ++group)
    {
        for (int lane = 0; lane < 4; ++lane)
        {
            cutoffs[group][lane] = 20*std::pow(1000, static_cast<double>(4*group+lane)/(options.m_grid-1));
        }
    }

    for (int drive = 0; drive < options.m_grid; ++drive)
    {
        for (int resonance = 0; resonance < options.m_grid; ++resonance)
        {
            for (int group = 0; group < groups; ++group)
            {
                const auto coefficients = Prepare(static_cast<double>(drive)/(options.m_grid-1),
                    static_cast<double>(resonance)/(options.m_grid-1), cutoffs[group]);
                const double slope = coefficients.m_gain[0]*coefficients.m_inverseTanh[0];
                const double cubedSlope = slope*slope*slope;
                int eligible = 0;
                for (int lane = 0; lane < 4; ++lane)
                {
                    eligible += coefficients.m_feedback[lane]*cubedSlope > options.m_minEffectiveResonance;
                }

                Require(eligible == 0 || eligible == 4,
                    "Selection cuts a four-cutoff batch; this threshold needs lane repacking");
                if (eligible == 4)
                {
                    result.m_parameterJobs.push_back(static_cast<uint32_t>((drive*options.m_grid+resonance)*groups+group));
                }
            }
        }
    }

    return result;
}

std::string PlanJSON(const Options& options, const WorkPlan& plan)
{
    std::ostringstream text;
    text << std::setprecision(17) << "{\"grid_points\":" << options.Jobs()*4
        << ",\"selected_jobs\":" << plan.Jobs() << ",\"selected_points\":" << plan.Jobs()*4
        << ",\"starts\":" << options.m_starts << ",\"trajectories\":" << plan.Jobs()*4*options.m_starts
        << ",\"scalar_sample_updates\":" << plan.Jobs()*4*options.m_starts*options.m_samples
        << ",\"min_effective_resonance\":" << options.m_minEffectiveResonance << '}';
    return text.str();
}

using State = std::array<Vector, 4>;

struct SignalSums
{
    Vector m_energy = Splat(0);
    Vector m_real = Splat(0);
    Vector m_imaginary = Splat(0);
    Vector m_sum = Splat(0);
    Vector m_peak = Splat(0);

    void Add(const SignalSums& other)
    {
        m_energy += other.m_energy;
        m_real += other.m_real;
        m_imaginary += other.m_imaginary;
        m_sum += other.m_sum;
        m_peak = simd::max(m_peak, other.m_peak);
    }

    void Measure(Vector output, double cosineInput, double sineInput)
    {
        m_energy += output*output;
        m_real += output*Splat(cosineInput);
        m_imaginary -= output*Splat(sineInput);
        m_sum += output;
        m_peak = simd::max(m_peak, simd::abs(output));
    }
};

// These scopes contain complete input periods, making DC, cosine and sine
// orthogonal. Coherent power is computed per trajectory before aggregating
// across starting states. Residual includes harmonics AND inharmonic activity.
//
std::array<double, x_scopeMetrics> Metrics(const SignalSums& sums, int lane, int count, double amplitude,
    double projectionTolerance)
{
    const double total = sums.m_energy[lane]/count;
    const double real = 2*sums.m_real[lane]/(count*amplitude);
    const double imaginary = 2*sums.m_imaginary[lane]/(count*amplitude);
    const double mean = sums.m_sum[lane]/count;
    const double fundamental = (real*real+imaginary*imaginary)/2;
    const double residual = total-fundamental-mean*mean;
    Require(residual >= -projectionTolerance*total, "Projection power exceeds total power beyond roundoff");
    return {total, fundamental, real, imaginary, mean, std::max(0.0, residual)};
}

std::array<double, x_scopeMetrics> Metrics(const SignalSums& sums, int lane, int count, double amplitude)
{
    return Metrics(sums,lane,count,amplitude,1e-9);
}

// This hot loop has no loop over lanes. Apple implements a four-double vector
// as two NEON registers, including the vector transcendental calls. Measurements
// use a short output buffer so their accumulators do not remain live across
// transcendental calls. Every output sample is measured; nothing is decimated.
//
SignalSums Process(State& state, const Coefficients& c, const double* input,
    const double* quadrature, int count)
{
    Vector a = state[0];
    Vector b = state[1];
    Vector d = state[2];
    Vector e = state[3];
    SignalSums result;
    std::array<Vector, 256> output;
    for (int begin = 0; begin < count; begin += static_cast<int>(output.size()))
    {
        const int length = std::min(count-begin, static_cast<int>(output.size()));
        for (int n = 0; n < length; ++n)
        {
            a = c.m_decay*a + c.m_alpha*(Splat(input[begin+n])-c.m_feedback*e);
            b = c.m_decay*b + c.m_alpha*(simd::tanh(c.m_gain*a)*c.m_inverseTanh);
            d = c.m_decay*d + c.m_alpha*(simd::tanh(c.m_gain*b)*c.m_inverseTanh);
            e = c.m_decay*e + c.m_alpha*(simd::tanh(c.m_gain*d)*c.m_inverseTanh);
            output[n] = c.m_outputGain*(simd::tanh(c.m_gain*e)*c.m_inverseTanh);
        }

        for (int n = 0; n < length; ++n)
        {
            result.Measure(output[n], input[begin+n], quadrature[begin+n]);
        }
    }

    state = {a, b, d, e};
    return result;
}

struct Moments
{
    double m_mean = 0;
    double m_m2 = 0;
    double m_min = std::numeric_limits<double>::infinity();
    double m_max = -std::numeric_limits<double>::infinity();
    int m_count = 0;

    void Add(double value)
    {
        Require(std::isfinite(value), "Non-finite energy statistic");
        ++m_count;
        const double delta = value - m_mean;
        m_mean += delta/m_count;
        m_m2 += delta*(value-m_mean);
        m_min = std::min(m_min, value);
        m_max = std::max(m_max, value);
    }

    double Deviation() const
    {
        return std::sqrt(std::max(0.0, m_m2/m_count));
    }

    std::array<float, 4> Pack() const
    {
        Require(m_count > 0, "Empty aggregate");
        return {static_cast<float>(m_mean), static_cast<float>(Deviation()),
            static_cast<float>(m_min), static_cast<float>(m_max)};
    }
};

// Each set of four aggregate values is mean, population SD, minimum, maximum
// across initial states. Unused energy windows contain NaNs. All accumulation
// is binary64; only these saved summaries are converted to binary32.
//
struct PointRecord
{
    std::array<float, 4> m_fullEnergy;
    std::array<float, 4> m_tailEnergy;
    std::array<float, 4> m_temporalCV;
    std::array<float, 4> m_fundamentalTemporalCV;
    std::array<float, 4> m_fundamentalCoherence;
    std::array<float, 4> m_tailPeak;
    std::array<float, 4> m_tailCrest;
    std::array<float, 4> m_totalPrefixDrift;
    std::array<float, 4> m_fundamentalPrefixDrift;
    std::array<float, 4> m_coherentPrefixDrift;
    std::array<std::array<std::array<float, 4>, x_scopeMetrics>, x_scopes> m_scopes;
    std::array<float, x_windows> m_windowMean;
    std::array<float, x_windows> m_windowSD;
    std::array<float, x_windows> m_windowFundamentalMean;
    std::array<float, x_windows> m_windowFundamentalSD;
};

struct Record
{
    uint64_t m_job;
    std::array<PointRecord, 4> m_points;
    uint64_t m_checksum;
};

static_assert(sizeof(PointRecord) == 864);
static_assert(sizeof(Record) == 3472);

struct PointAccumulator
{
    Moments m_fullEnergy;
    Moments m_tailEnergy;
    Moments m_temporalCV;
    Moments m_fundamentalTemporalCV;
    Moments m_fundamentalCoherence;
    Moments m_tailPeak;
    Moments m_tailCrest;
    Moments m_totalPrefixDrift;
    Moments m_fundamentalPrefixDrift;
    Moments m_coherentPrefixDrift;
    std::array<std::array<Moments, x_scopeMetrics>, x_scopes> m_scopes;
    std::array<Moments, x_windows> m_windows;
    std::array<Moments, x_windows> m_windowFundamental;
};

void AccumulateTrajectory(PointAccumulator& accumulator, const Options& options, const Frequency& tone,
    double fullEnergy, double tailEnergy, const std::array<SignalSums, x_windows>& windows,
    const std::array<SignalSums, x_scopes>& scopes, int lane, double projectionTolerance)
{
    accumulator.m_fullEnergy.Add(fullEnergy/options.m_samples);
    accumulator.m_tailEnergy.Add(tailEnergy/(options.m_samples-options.m_samples/2));
    std::array<std::array<double, x_scopeMetrics>, x_scopes> scoped;
    for (int scope = 0; scope < x_scopes; ++scope)
    {
        const auto values = Metrics(scopes[scope], lane,
            tone.m_scopes[scope].m_end-tone.m_scopes[scope].m_begin, options.m_amplitude, projectionTolerance);
        scoped[scope] = values;
        for (int metric = 0; metric < x_scopeMetrics; ++metric)
        {
            accumulator.m_scopes[scope][metric].Add(values[metric]);
        }
    }

    const double inputPower = options.m_amplitude*options.m_amplitude/2;
    accumulator.m_totalPrefixDrift.Add(std::abs(scoped[2][0]-scoped[1][0])/inputPower);
    accumulator.m_fundamentalPrefixDrift.Add(std::abs(scoped[2][1]-scoped[1][1])/inputPower);
    accumulator.m_coherentPrefixDrift.Add(std::hypot(scoped[2][2]-scoped[1][2],
        scoped[2][3]-scoped[1][3])/options.m_amplitude);
    const double peak = scopes[3].m_peak[lane];
    const double latePower = scopes[3].m_energy[lane]/(tone.m_scopes[3].m_end-tone.m_scopes[3].m_begin);
    accumulator.m_tailPeak.Add(peak);
    accumulator.m_tailCrest.Add(latePower == 0 ? 0 : peak/std::sqrt(latePower));
    Moments temporal;
    Moments fundamentalTemporal;
    Moments real;
    Moments imaginary;
    for (int b = 0; b < tone.m_fullWindows; ++b)
    {
        const auto values = Metrics(windows[b], lane, tone.m_windowSamples, options.m_amplitude, projectionTolerance);
        accumulator.m_windows[b].Add(values[0]);
        accumulator.m_windowFundamental[b].Add(values[1]);
        if (b >= tone.m_tailWindow)
        {
            temporal.Add(values[0]);
            fundamentalTemporal.Add(values[1]);
            real.Add(values[2]);
            imaginary.Add(values[3]);
        }
    }

    accumulator.m_temporalCV.Add(temporal.m_mean == 0 ? 0 : temporal.Deviation()/temporal.m_mean);
    accumulator.m_fundamentalTemporalCV.Add(fundamentalTemporal.m_mean == 0 ? 0
        : fundamentalTemporal.Deviation()/fundamentalTemporal.m_mean);
    const double coherent = (real.m_mean*real.m_mean+imaginary.m_mean*imaginary.m_mean)/2;
    const double coherence = fundamentalTemporal.m_mean == 0 ? 0 : coherent/fundamentalTemporal.m_mean;
    Require(coherence <= 1+1e-9, "Fundamental coherence exceeds one beyond roundoff");
    accumulator.m_fundamentalCoherence.Add(std::min(1.0, coherence));
}

Record PackRecord(uint64_t job, const Frequency& tone, const std::array<PointAccumulator, 4>& accumulators)
{
    Record result{};
    result.m_job = job;
    for (int lane = 0; lane < 4; ++lane)
    {
        const auto& accumulator = accumulators[lane];
        auto& point = result.m_points[lane];
        point.m_fullEnergy = accumulator.m_fullEnergy.Pack();
        point.m_tailEnergy = accumulator.m_tailEnergy.Pack();
        point.m_temporalCV = accumulator.m_temporalCV.Pack();
        point.m_fundamentalTemporalCV = accumulator.m_fundamentalTemporalCV.Pack();
        point.m_fundamentalCoherence = accumulator.m_fundamentalCoherence.Pack();
        point.m_tailPeak = accumulator.m_tailPeak.Pack();
        point.m_tailCrest = accumulator.m_tailCrest.Pack();
        point.m_totalPrefixDrift = accumulator.m_totalPrefixDrift.Pack();
        point.m_fundamentalPrefixDrift = accumulator.m_fundamentalPrefixDrift.Pack();
        point.m_coherentPrefixDrift = accumulator.m_coherentPrefixDrift.Pack();
        for (int scope = 0; scope < x_scopes; ++scope)
        {
            for (int metric = 0; metric < x_scopeMetrics; ++metric)
            {
                point.m_scopes[scope][metric] = accumulator.m_scopes[scope][metric].Pack();
            }
        }

        point.m_windowMean.fill(std::numeric_limits<float>::quiet_NaN());
        point.m_windowSD.fill(std::numeric_limits<float>::quiet_NaN());
        point.m_windowFundamentalMean.fill(std::numeric_limits<float>::quiet_NaN());
        point.m_windowFundamentalSD.fill(std::numeric_limits<float>::quiet_NaN());
        for (int b = 0; b < tone.m_fullWindows; ++b)
        {
            point.m_windowMean[b] = static_cast<float>(accumulator.m_windows[b].m_mean);
            point.m_windowSD[b] = static_cast<float>(accumulator.m_windows[b].Deviation());
            point.m_windowFundamentalMean[b] = static_cast<float>(accumulator.m_windowFundamental[b].m_mean);
            point.m_windowFundamentalSD[b] = static_cast<float>(accumulator.m_windowFundamental[b].Deviation());
        }
    }

    result.m_checksum = Hash(&result, sizeof(result)-sizeof(result.m_checksum));
    return result;
}

Record Evaluate(uint64_t job, const Options& options, const std::vector<Frequency>& frequencies)
{
    const int groups = options.m_grid/4;
    uint64_t remaining = job;
    const int firstCutoff = static_cast<int>(remaining % groups)*4;
    remaining /= groups;
    const int resonance = static_cast<int>(remaining % options.m_grid);
    remaining /= options.m_grid;
    const int drive = static_cast<int>(remaining % options.m_grid);
    const int frequency = static_cast<int>(remaining/options.m_grid);
    const auto& tone = frequencies[frequency];
    std::array<double, 4> cutoffs;
    for (int lane = 0; lane < 4; ++lane)
    {
        cutoffs[lane] = 20*std::pow(1000, static_cast<double>(firstCutoff+lane)/(options.m_grid-1));
    }

    auto c = Prepare(static_cast<double>(drive)/(options.m_grid-1),
        static_cast<double>(resonance)/(options.m_grid-1), cutoffs);
    c.m_bounds[0] = Splat(options.m_amplitude) + c.m_feedback*c.m_inverseTanh;
    std::array<PointAccumulator, 4> accumulators;
    for (int trial = 0; trial < options.m_starts; ++trial)
    {
        State state;
        for (int lane = 0; lane < 4; ++lane)
        {
            const uint64_t key = Mix(options.m_seed ^ Mix(job*4+lane) ^ Mix(0x100000000ULL+trial));
            for (int stage = 0; stage < 4; ++stage)
            {
                state[stage][lane] = (2*Uniform(key+stage)-1)*c.m_bounds[stage][lane];
            }
        }

        Vector full = Splat(0);
        Vector tail = Splat(0);
        std::array<SignalSums, x_windows> windows;
        std::array<SignalSums, x_scopes> scopes;
        for (const auto& segment : tone.m_segments)
        {
            const auto measured = Process(state, c, tone.m_input.data()+segment.m_begin,
                tone.m_quadrature.data()+segment.m_begin, segment.m_end-segment.m_begin);
            full += measured.m_energy;
            windows[segment.m_window].Add(measured);
            if (segment.m_begin >= options.m_samples/2)
            {
                tail += measured.m_energy;
            }

            for (int scope = 0; scope < x_scopes; ++scope)
            {
                if (segment.m_scopeMask & (1u << scope))
                {
                    scopes[scope].Add(measured);
                }
            }
        }

        for (int lane = 0; lane < 4; ++lane)
        {
            AccumulateTrajectory(accumulators[lane],options,tone,full[lane],tail[lane],windows,scopes,lane,1e-9);
        }
    }

    return PackRecord(job,tone,accumulators);
}

void WriteAll(int file, const void* data, size_t length)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    while (length > 0)
    {
        const ssize_t written = write(file, bytes, length);
        if (written < 0 && errno == EINTR)
        {
            continue;
        }

        Require(written > 0, "Output write failed: " + std::string(std::strerror(errno)));
        bytes += written;
        length -= static_cast<size_t>(written);
    }
}

struct Header
{
    uint64_t m_magic;
    uint64_t m_identity;
    uint64_t m_recordBytes;
};

struct OutputFile
{
    int m_descriptor;

    OutputFile(const fs::path& path, const Header& header)
        : m_descriptor(open(path.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644))
    {
        Require(m_descriptor >= 0, "Cannot open output: " + path.string());
        if (fs::file_size(path) == 0)
        {
            WriteAll(m_descriptor, &header, sizeof(header));
            Require(fsync(m_descriptor) == 0, "Cannot sync output header");
        }
    }

    ~OutputFile()
    {
        close(m_descriptor);
    }

    void Append(const Record& record)
    {
        WriteAll(m_descriptor, &record, sizeof(record));
        Require(fsync(m_descriptor) == 0, "Cannot sync completed job");
    }

    void Append(const std::vector<Record>& records)
    {
        Require(!records.empty(), "Empty checkpoint batch");
        WriteAll(m_descriptor,records.data(),records.size()*sizeof(Record));
        Require(fsync(m_descriptor) == 0, "Cannot sync completed batch");
    }
};

std::vector<unsigned char> Scan(const fs::path& directory, const Header& expected, uint64_t jobs)
{
    std::vector<unsigned char> completed(jobs, 0);
    for (const auto& entry : fs::directory_iterator(directory))
    {
        const auto name = entry.path().filename().string();
        if (name.rfind("worker-", 0) != 0 || entry.path().extension() != ".bin")
        {
            continue;
        }

        const uint64_t bytes = entry.file_size();
        Require(bytes >= sizeof(Header), "Incomplete output header: " + name);
        std::ifstream stream(entry.path(), std::ios::binary);
        Header header{};
        stream.read(reinterpret_cast<char*>(&header), sizeof(header));
        Require(stream.good() && header.m_magic == expected.m_magic
            && header.m_identity == expected.m_identity && header.m_recordBytes == sizeof(Record),
            "Run configuration or format mismatch: " + name);
        const uint64_t count = (bytes-sizeof(Header))/sizeof(Record);
        for (uint64_t i = 0; i < count; ++i)
        {
            Record record;
            stream.read(reinterpret_cast<char*>(&record), sizeof(record));
            Require(stream.good() && record.m_job < jobs && !completed[record.m_job]
                && record.m_checksum == Hash(&record, sizeof(record)-sizeof(record.m_checksum)),
                "Corrupt or duplicate completed record in " + name + " at index " + std::to_string(i));
            completed[record.m_job] = 1;
        }

        stream.close();
        const uint64_t completeBytes = sizeof(Header)+count*sizeof(Record);
        if (completeBytes != bytes)
        {
            fs::resize_file(entry.path(), completeBytes);
            std::cout << "{\"recovered_partial_record\":\"" << name << "\"}" << std::endl;
        }
    }

    return completed;
}

void WriteText(const fs::path& path, const std::string& value)
{
    const fs::path temporary = path.string()+".tmp";
    {
        std::ofstream file(temporary);
        Require(file.good(), "Cannot write " + temporary.string());
        file << value << '\n';
        file.flush();
        Require(file.good(), "Failed writing " + temporary.string());
    }

    fs::rename(temporary, path);
}

struct ManifestModel
{
    uint64_t m_format;
    std::string m_identity;
    std::string m_name;
    std::string m_selection;
    std::string m_initialStates;
    std::string m_precision;
};

void Manifest(const Options& options, const std::vector<Frequency>& frequencies, const WorkPlan& plan,
    const ManifestModel& model)
{
    std::ostringstream text;
    text << std::setprecision(17) << "{\"format\":" << model.m_format << ",\"identity\":\"" << model.m_identity
        << "\",\"model\":\"" << model.m_name
        << "\",\"grid\":" << options.m_grid << ",\"starts\":" << options.m_starts
        << ",\"samples\":" << options.m_samples << ",\"sample_rate\":" << options.m_sampleRate
        << ",\"amplitude\":" << options.m_amplitude << ",\"seed\":" << options.m_seed
        << ",\"jobs\":" << plan.Jobs() << ",\"grid_jobs\":" << options.Jobs()
        << ",\"selected_points\":" << plan.Jobs()*4
        << ",\"min_effective_resonance\":" << options.m_minEffectiveResonance
        << ",\"selection\":\"" << model.m_selection << "\""
        << ",\"lanes\":4,\"header_bytes\":" << sizeof(Header)
        << ",\"record_bytes\":" << sizeof(Record) << ",\"expected_record_bytes\":" << plan.Jobs()*sizeof(Record)
        << ",\"point_order\":[\"frequency\",\"drive\",\"resonance\",\"cutoff\"]"
        << ",\"record_layout\":\"uint64 job; four point records; uint64 FNV1a checksum\""
        << ",\"point_float_count\":216"
        << ",\"point_layout\":\"216 little-endian float32: raw full energy[4], raw tail energy[4], total temporal CV[4], fundamental temporal CV[4], fundamental coherence[4], late peak[4], late crest[4], total prefix drift[4], fundamental prefix drift[4], coherent prefix drift[4], scope metrics[4][6][4], window total mean[20], window total SD[20], window fundamental mean[20], window fundamental SD[20]\""
        << ",\"prefix_drift_definition\":\"per-start absolute difference between half and full period-aligned prefixes; power differences divided by input mean-square, complex coefficient difference norm divided by input amplitude\""
        << ",\"scope_names\":[\"prefix_quarter\",\"prefix_half\",\"prefix_full\",\"late_half\"]"
        << ",\"scope_metrics\":[\"total_power\",\"fundamental_power\",\"fundamental_real\",\"fundamental_imaginary\",\"dc_mean\",\"residual_power\"]"
        << ",\"coherent_definition\":\"C=2/N sum(y[n]*exp(-i*omega*n)); fundamental power=|C|^2/2, evaluated per start before aggregation\""
        << ",\"residual_definition\":\"total minus fundamental minus DC squared, clamped only for roundoff; includes harmonics and inharmonic energy\""
        << ",\"coherence_definition\":\"|mean late-window C|^2 / mean late-window |C|^2; zero when the denominator is zero\""
        << ",\"gain_normalization\":\"power / (amplitude^2/2); raw total includes DC, which can be removed using the recorded DC moments\""
        << ",\"aggregate_order\":[\"mean\",\"population_sd\",\"minimum\",\"maximum\"]"
        << ",\"temporal_cv\":\"population SD / mean of complete energy windows beginning in the last half; zero only for zero energy\""
        << ",\"initial_states\":\"" << model.m_initialStates << "\""
        << ",\"precision\":\"" << model.m_precision << "\",\"frequencies\":[";
    for (size_t i = 0; i < frequencies.size(); ++i)
    {
        const auto& frequency = frequencies[i];
        if (i != 0)
        {
            text << ',';
        }

        text << "{\"target_hz\":" << frequency.m_target << ",\"actual_hz\":" << frequency.m_actual
            << ",\"period_samples\":" << frequency.m_period << ",\"input_cycles\":" << frequency.m_cycles
            << ",\"window_samples\":" << frequency.m_windowSamples << ",\"complete_windows\":" << frequency.m_fullWindows
            << ",\"first_late_window\":" << frequency.m_tailWindow << ",\"scopes\":[";
        for (int scope = 0; scope < x_scopes; ++scope)
        {
            if (scope != 0)
            {
                text << ',';
            }

            text << '[' << frequency.m_scopes[scope].m_begin << ',' << frequency.m_scopes[scope].m_end << ']';
        }

        text << "]}";
    }

    text << "]}";
    WriteText(options.m_output/"manifest.json", text.str());
}

void Manifest(const Options& options, const std::vector<Frequency>& frequencies, const WorkPlan& plan)
{
    Manifest(options,frequencies,plan,{x_formatVersion,options.Identity(),"real-tanh-detector-off-double-simd",
        "strict k*(g/tanh(g))^3 > threshold, with k after kSafe",
        "independent uniform histories in [-A-kC,A+kC] x [-C,C]^3, C=1/tanh(g); deterministic SplitMix64 keys",
        "double computation; float32 saved summaries"});
}

// Keep test-only vector kernels out of main. Apple clang 17's machine scheduler
// crashes when it combines the projection and recurrence tests there at -O3.
//
[[clang::noinline]] double ProjectionSelfTest()
{
    constexpr int x_count = 9600;
    constexpr double x_amplitude = 0.25;
    const Vector real{0.6, 0.2, 0, 1};
    const Vector imaginary{0.8, -0.4, 0, 0};
    const Vector dc{0.125, 0, 0.3, 0};
    const Vector harmonic{0.3, 0.1, 0.2, 0};
    SignalSums sums;
    SignalSums positive;
    SignalSums negative;
    for (int n = 0; n < x_count; ++n)
    {
        const double theta = 2*x_pi*(n%96)/96;
        const double cosine = std::cos(theta);
        const double sine = std::sin(theta);
        const Vector output = dc+real*Splat(cosine)-imaginary*Splat(sine)+harmonic*Splat(std::cos(3*theta));
        sums.Measure(output, x_amplitude*cosine, x_amplitude*sine);
        positive.Measure(Splat(cosine), x_amplitude*cosine, x_amplitude*sine);
        negative.Measure(Splat(-cosine), x_amplitude*cosine, x_amplitude*sine);
    }

    double error = 0;
    for (int lane = 0; lane < 4; ++lane)
    {
        const double fundamental = (real[lane]*real[lane]+imaginary[lane]*imaginary[lane])/2;
        const double residual = harmonic[lane]*harmonic[lane]/2;
        const std::array<double, x_scopeMetrics> expected{
            fundamental+residual+dc[lane]*dc[lane], fundamental, real[lane], imaginary[lane], dc[lane], residual};
        const auto actual = Metrics(sums, lane, x_count, x_amplitude);
        for (int metric = 0; metric < x_scopeMetrics; ++metric)
        {
            error = std::max(error, std::abs(actual[metric]-expected[metric]));
        }
    }

    const auto local = Metrics(positive, 0, x_count, x_amplitude);
    positive.Add(negative);
    const auto combined = Metrics(positive, 0, 2*x_count, x_amplitude);
    Require(local[1] > 0.49 && combined[1] < 1e-20 && combined[0] > 0.49,
        "Long coherent power must retain cancellation between opposite phases");
    Require(error < 1e-11, "Synthetic tone/DC/harmonic projection check failed");
    return error;
}

[[clang::noinline]] void SelfTest()
{
    const double projectionError = ProjectionSelfTest();
    Options selected;
    selected.m_starts = 10;
    selected.m_minEffectiveResonance = 3.5;
    const auto selection = Plan(selected);
    Require(selection.Jobs() == 9270000, "Filtered grid count differs from independent enumeration");
    Require(std::is_sorted(selection.m_parameterJobs.begin(), selection.m_parameterJobs.end())
        && std::adjacent_find(selection.m_parameterJobs.begin(), selection.m_parameterJobs.end()) == selection.m_parameterJobs.end(),
        "Selection must contain sorted unique original parameter IDs");
    for (const auto index : {uint64_t{0}, selection.Jobs()/2, selection.Jobs()-1})
    {
        Require(selection.JobAt(index) < selected.Jobs(), "Selected job mapping out of bounds");
    }

    double tanhError = 0;
    double stepError = 0;
    for (int i = 0; i < 10000; ++i)
    {
        Vector input;
        for (int lane = 0; lane < 4; ++lane)
        {
            input[lane] = i < 100 ? (2*Uniform(i*4+lane)-1)*1e-12 : 40*(2*Uniform(i*4+lane)-1);
        }

        const auto result = simd::tanh(input);
        for (int lane = 0; lane < 4; ++lane)
        {
            tanhError = std::max(tanhError, std::abs(result[lane]-std::tanh(input[lane])));
        }

        auto c = Prepare(Uniform(i+19), Uniform(i+93), {20, 200, 2000, 20000});
        State state;
        for (int stage = 0; stage < 4; ++stage)
        {
            state[stage] = input/40;
        }

        const auto before = state;
        const double sample = 0.25*(2*Uniform(i+71)-1);
        const double quadrature = 0;
        const auto measured = Process(state, c, &sample, &quadrature, 1);
        for (int lane = 0; lane < 4; ++lane)
        {
            std::array<double, 4> scalar;
            for (int stage = 0; stage < 4; ++stage)
            {
                scalar[stage] = before[stage][lane];
            }

            scalar[0] = c.m_decay[lane]*scalar[0] + c.m_alpha[lane]*(sample-c.m_feedback[lane]*scalar[3]);
            for (int stage = 1; stage < 4; ++stage)
            {
                scalar[stage] = c.m_decay[lane]*scalar[stage]
                    + c.m_alpha[lane]*(std::tanh(c.m_gain[lane]*scalar[stage-1])*c.m_inverseTanh[lane]);
            }

            for (int stage = 0; stage < 4; ++stage)
            {
                stepError = std::max(stepError, std::abs(scalar[stage]-state[stage][lane]));
            }

            const double output = c.m_outputGain[lane]*std::tanh(c.m_gain[lane]*scalar[3])*c.m_inverseTanh[lane];
            stepError = std::max(stepError, std::abs(measured.m_energy[lane]-output*output));
        }
    }

    Require(tanhError < 2e-15 && stepError < 2e-13, "SIMD/scalar numerical check failed");
    std::cout << std::setprecision(17) << "{\"self_test\":\"passed\",\"tanh_max_absolute_error\":"
        << tanhError << ",\"step_and_energy_max_absolute_error\":" << stepError
        << ",\"synthetic_projection_max_absolute_error\":" << projectionError
        << ",\"selected_points_at_threshold_3_5\":" << selection.Jobs()*4 << "}" << std::endl;
}

void Run(const Options& options)
{
    const auto plan = Plan(options);
    Require(plan.Jobs() > 0, "No configurations satisfy the selection");
    fs::create_directories(options.m_output);
    const int lock = open((options.m_output/"run.lock").c_str(), O_WRONLY|O_CREAT, 0644);
    Require(lock >= 0 && flock(lock, LOCK_EX|LOCK_NB) == 0, "Another process owns this output directory");
    const auto identity = options.Identity();
    const Header header{x_magic, Hash(identity.data(), identity.size()), sizeof(Record)};
    const auto completed = Scan(options.m_output, header, options.Jobs());
    const uint64_t previous = std::count(completed.begin(), completed.end(), static_cast<unsigned char>(1));
    Require(fs::space(options.m_output).available > 10ULL*1024*1024*1024, "Less than 10 GiB free; refusing to start");
    std::vector<Frequency> frequencies;
    for (int i = 0; i < options.m_grid; ++i)
    {
        frequencies.push_back(PrepareFrequency(20*std::pow(1000, static_cast<double>(i)/(options.m_grid-1)), options));
    }

    Manifest(options, frequencies, plan);
    std::cout << PlanJSON(options, plan) << std::endl;
    std::signal(SIGTERM, Stop);
    std::signal(SIGINT, Stop);
    const auto started = std::chrono::steady_clock::now();
    std::atomic<uint64_t> sequence{0};
    std::atomic<uint64_t> claimed{0};
    std::atomic<uint64_t> finished{0};
    std::atomic<int> active{options.m_threads};
    std::string failure;
    std::mutex failureMutex;
    uint64_t stride = 104729;
    while (std::gcd(stride, plan.Jobs()) != 1)
    {
        stride += 2;
    }

    std::vector<std::thread> workers;
    for (int worker = 0; worker < options.m_threads; ++worker)
    {
        workers.emplace_back([&, worker]
        {
            try
            {
                std::ostringstream name;
                name << "worker-" << std::setfill('0') << std::setw(3) << worker << ".bin";
                OutputFile output(options.m_output/name.str(), header);
                while (!g_stop.load(std::memory_order_relaxed))
                {
                    const uint64_t index = sequence.fetch_add(1);
                    if (index >= plan.Jobs())
                    {
                        break;
                    }

                    const uint64_t job = plan.JobAt((index*stride+17)%plan.Jobs());
                    if (completed[job])
                    {
                        continue;
                    }

                    if (claimed.fetch_add(1) >= options.m_maxJobs)
                    {
                        break;
                    }

                    const auto record = Evaluate(job, options, frequencies);
                    output.Append(record);
                    finished.fetch_add(1);
                }
            }
            catch (const std::exception& error)
            {
                std::lock_guard<std::mutex> guard(failureMutex);
                if (failure.empty())
                {
                    failure = error.what();
                }

                g_stop.store(true, std::memory_order_relaxed);
            }

            active.fetch_sub(1);
        });
    }

    auto progress = [&](bool final)
    {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
        const uint64_t count = finished.load();
        const double rate = count/seconds;
        std::ostringstream text;
        text << std::setprecision(12) << "{\"status\":\""
            << (previous+count == plan.Jobs() ? "complete" : (final ? "stopped" : "running"))
            << "\",\"completed_jobs\":" << previous+count << ",\"total_jobs\":" << plan.Jobs()
            << ",\"completed_points\":" << 4*(previous+count) << ",\"completed_trajectories\":" << 4*(previous+count)*options.m_starts
            << ",\"jobs_this_process\":" << count << ",\"elapsed_seconds\":" << seconds
            << ",\"jobs_per_second\":" << rate << ",\"scalar_samples_per_second\":" << rate*4*options.m_starts*options.m_samples
            << ",\"eta_seconds\":";
        if (rate > 0)
        {
            text << (plan.Jobs()-previous-count)/rate;
        }
        else
        {
            text << "null";
        }

        text << ",\"threads\":" << options.m_threads << ",\"active_workers\":" << active.load() << '}';
        WriteText(options.m_output/"progress.json", text.str());
        std::cout << text.str() << std::endl;
    };
    progress(false);
    while (active.load() != 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (fs::space(options.m_output).available < 10ULL*1024*1024*1024)
        {
            std::lock_guard<std::mutex> guard(failureMutex);
            if (failure.empty())
            {
                failure = "Stopped with less than 10 GiB free";
            }

            g_stop.store(true, std::memory_order_relaxed);
        }

        progress(false);
    }

    for (auto& worker : workers)
    {
        worker.join();
    }

    progress(true);
    close(lock);
    Require(failure.empty(), failure);
}
}

#ifndef LADDER_ENERGY_GRID_EMBEDDED
int main(int argc, char** argv)
{
    try
    {
        const auto options = Parse(argc, argv);
        if (options.m_selfTest)
        {
            SelfTest();
        }
        else if (options.m_planOnly)
        {
            std::cout << PlanJSON(options, Plan(options)) << std::endl;
        }
        else
        {
            Run(options);
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ladder-energy-grid: " << error.what() << '\n';
        return 1;
    }
}
#endif

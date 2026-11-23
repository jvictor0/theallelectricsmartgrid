// Research-only binary64 recurrence for the detector-disabled Lean ladder.
// Checked against analyze_ladder_harmonic_reduction.Model before each sweep.
// This does not change the app, generated DSP, or Lean definitions.
//
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
using State = std::array<double, 4>;

struct Model
{
    double m_alpha;
    double m_feedback;
    double m_gain;
    double m_inverseTanhGain;

    double Saturate(double x) const
    {
        return std::tanh(m_gain * x) * m_inverseTanhGain;
    }

    void Step(State& state, double input) const
    {
        const double decay = 1 - m_alpha;
        state[0] = decay * state[0] + m_alpha * (input - m_feedback * state[3]);
        state[1] = decay * state[1] + m_alpha * Saturate(state[0]);
        state[2] = decay * state[2] + m_alpha * Saturate(state[1]);
        state[3] = decay * state[3] + m_alpha * Saturate(state[2]);
    }

    void TangentStep(const State& updatedState, State& tangent) const
    {
        const double decay = 1 - m_alpha;
        const double slope = m_gain * m_inverseTanhGain;
        tangent[0] = decay * tangent[0] - m_alpha * m_feedback * tangent[3];
        for (int j = 1; j < 4; ++j)
        {
            const double value = std::tanh(m_gain * updatedState[j - 1]);
            tangent[j] = decay * tangent[j] + m_alpha * slope * (1 - value * value) * tangent[j - 1];
        }
    }
};

Model Prepare(const double* coefficients)
{
    return {coefficients[0], coefficients[1], coefficients[2],
        1 / std::tanh(coefficients[2])};
}

struct Defect
{
    State m_delta{};
    State m_scale{};

    void Add(const State& current, const State& previous)
    {
        for (int j = 0; j < 4; ++j)
        {
            m_delta[j] = std::max(m_delta[j], std::abs(current[j] - previous[j]));
            m_scale[j] = std::max({m_scale[j], std::abs(current[j]), std::abs(previous[j])});
        }
    }

    double Score(double absoluteTolerance, double relativeTolerance) const
    {
        double result = 0;
        for (int j = 0; j < 4; ++j)
        {
            result = std::max(result, m_delta[j] / (absoluteTolerance + relativeTolerance * m_scale[j]));
        }

        return result;
    }

    double Absolute() const
    {
        return *std::max_element(m_delta.begin(), m_delta.end());
    }
};
}

extern "C" void LadderGridCycle(const double* coefficients, const double* tones,
    int period, double amplitude, double* stateValues, double* trace)
{
    const Model model = Prepare(coefficients);
    State state;
    std::copy_n(stateValues, 4, state.begin());
    for (int n = 0; n < period; ++n)
    {
        model.Step(state, amplitude * tones[2 * n]);
        std::copy(state.begin(), state.end(), trace + 4 * n);
    }

    std::copy(state.begin(), state.end(), stateValues);
}

// Follow the same trajectory and a renormalized infinitesimal perturbation.
// Phase of the forcing is fixed: this is the four-state conditional exponent,
// not an exponent of an augmented system containing a trivially neutral clock.
// Each feature row contains power, coherent real/imaginary peak, log stretch,
// whole-waveform one-period repeat score, absolute repeat error and output peak.
//
extern "C" void LadderGridTrace(const double* coefficients, const double* tones,
    int period, int periods, double amplitude, double* stateValues,
    double* tangentValues, double* endpoints, double* features, double* output)
{
    const Model model = Prepare(coefficients);
    State state;
    State tangent;
    std::copy_n(stateValues, 4, state.begin());
    std::copy_n(tangentValues, 4, tangent.begin());
    std::copy(state.begin(), state.end(), endpoints);
    std::vector<State> previous(static_cast<size_t>(period));
    for (int cycle = 0; cycle < periods; ++cycle)
    {
        Defect defect;
        double power = 0;
        double real = 0;
        double imag = 0;
        double stretch = 0;
        double peak = 0;
        for (int n = 0; n < period; ++n)
        {
            model.Step(state, amplitude * tones[2 * n]);
            model.TangentStep(state, tangent);
            if ((n + 1) % 32 == 0 || n + 1 == period)
            {
                const double norm = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1]
                    + tangent[2] * tangent[2] + tangent[3] * tangent[3]);
                stretch += std::log(norm);
                for (double& value : tangent)
                {
                    value /= norm;
                }
            }

            defect.Add(state, previous[n]);
            previous[n] = state;
            const double y = (1 + model.m_feedback) * model.Saturate(state[3]);
            output[static_cast<size_t>(cycle) * period + n] = y;
            power += y * y;
            real += y * tones[2 * n];
            imag -= y * tones[2 * n + 1];
            peak = std::max(peak, std::abs(y));
        }

        const std::array<double, 7> row = {power / period, 2 * real / period, 2 * imag / period,
            stretch, defect.Score(1e-12, 1e-9), defect.Absolute(), peak};
        std::copy(row.begin(), row.end(), features + static_cast<size_t>(cycle) * 7);
        std::copy(state.begin(), state.end(), endpoints + static_cast<size_t>(cycle + 1) * 4);
    }

    std::copy(state.begin(), state.end(), stateValues);
    std::copy(tangent.begin(), tangent.end(), tangentValues);
}

// Statistics: converged, settling periods, last settling score/absolute defect,
// measurement score/absolute defect, mean power, coherent real/imaginary peak,
// mean, peak absolute output, minimum/maximum single-period power, best endpoint
// lag (2..8), lag score, measurement periods, first passing period, acceptance
// before the separate verification window.
//
extern "C" void LadderGridSettle(const double* coefficients, const double* tones,
    int period, double amplitude, int maximumPeriods, int minimumPeriods,
    int consecutivePeriods, int measurementPeriods, double absoluteTolerance,
    double relativeTolerance, double* stateValues, double* statistics)
{
    const Model model = Prepare(coefficients);
    State state;
    std::copy_n(stateValues, 4, state.begin());
    std::vector<State> previous(static_cast<size_t>(period));
    std::array<State, 9> endpoints{};
    endpoints[0] = state;
    int streak = 0;
    int completed = 0;
    int firstPassing = 0;
    bool settled = false;
    Defect last;
    for (int cycle = 1; cycle <= maximumPeriods; ++cycle)
    {
        Defect defect;
        for (int n = 0; n < period; ++n)
        {
            model.Step(state, amplitude * tones[2 * n]);
            defect.Add(state, previous[n]);
            previous[n] = state;
        }

        last = defect;
        endpoints[cycle % 9] = state;
        completed = cycle;
        if (cycle >= minimumPeriods && defect.Score(absoluteTolerance, relativeTolerance) <= 1)
        {
            if (streak == 0)
            {
                firstPassing = cycle;
            }

            ++streak;
            if (streak >= consecutivePeriods)
            {
                settled = true;
                break;
            }
        }
        else
        {
            streak = 0;
        }
    }

    double lagScore = INFINITY;
    int bestLag = 0;
    for (int lag = 2; lag <= 8 && lag < completed; ++lag)
    {
        Defect defect;
        defect.Add(state, endpoints[(completed - lag) % 9]);
        const double score = defect.Score(absoluteTolerance, relativeTolerance);
        if (score < lagScore)
        {
            lagScore = score;
            bestLag = lag;
        }
    }

    double power = 0;
    double real = 0;
    double imag = 0;
    double mean = 0;
    double peak = 0;
    double minimumPower = INFINITY;
    double maximumPower = 0;
    double measurementScore = 0;
    double measurementAbsolute = 0;
    for (int cycle = 0; cycle < measurementPeriods; ++cycle)
    {
        Defect defect;
        double periodPower = 0;
        for (int n = 0; n < period; ++n)
        {
            model.Step(state, amplitude * tones[2 * n]);
            defect.Add(state, previous[n]);
            previous[n] = state;
            const double output = (1 + model.m_feedback) * model.Saturate(state[3]);
            periodPower += output * output;
            real += output * tones[2 * n];
            imag -= output * tones[2 * n + 1];
            mean += output;
            peak = std::max(peak, std::abs(output));
        }

        power += periodPower;
        minimumPower = std::min(minimumPower, periodPower / period);
        maximumPower = std::max(maximumPower, periodPower / period);
        measurementScore = std::max(measurementScore, defect.Score(absoluteTolerance, relativeTolerance));
        measurementAbsolute = std::max(measurementAbsolute, defect.Absolute());
    }

    const double count = static_cast<double>(period) * measurementPeriods;
    const std::array<double, 18> result = {
        settled && measurementScore <= 1 ? 1.0 : 0.0,
        static_cast<double>(completed), last.Score(absoluteTolerance, relativeTolerance), last.Absolute(),
        measurementScore, measurementAbsolute, power / count, 2 * real / count,
        2 * imag / count, mean / count, peak, minimumPower, maximumPower,
        static_cast<double>(bestLag), lagScore, static_cast<double>(measurementPeriods),
        static_cast<double>(settled ? firstPassing : 0), settled ? 1.0 : 0.0};
    std::copy(result.begin(), result.end(), statistics);
    std::copy(state.begin(), state.end(), stateValues);
}

#include "stock.cpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cfenv>

struct ForcingInterval
{
    float m_low;
    float m_high;

    // Real-valued endpoint bounds that reach zero admit both binary32 signs.
    //
    ForcingInterval(float low, float high)
        : m_low(low == 0.f ? -0.f : low), m_high(high == 0.f ? 0.f : high)
    {
    }

    bool Common() const
    {
        return std::bit_cast<uint32_t>(m_low) == std::bit_cast<uint32_t>(m_high);
    }
};

static ForcingInterval Pole(ForcingInterval past, ForcingInterval input, float alpha)
{
    return {std::fma(alpha, input.m_low, (1.f - alpha) * past.m_low),
        std::fma(alpha, input.m_high, (1.f - alpha) * past.m_high)};
}

static ForcingInterval Saturated(const Filter& filter, ForcingInterval x)
{
    auto sat = [&](float v) { return filter.Saturate(v)[0]; };
    if (x.m_low == 0.f && x.m_high == 0.f)
    {
        return {-0.f, 0.f};
    }

    if (x.Common())
    {
        const float value = sat(x.m_low);
        return {value, value};
    }

    const uint32_t a = std::bit_cast<uint32_t>(x.m_low);
    const uint32_t b = std::bit_cast<uint32_t>(x.m_high);
    const auto lowWord = std::min(a, b);
    const auto highWord = std::max(a, b);
    if ((a >> 31) == (b >> 31) && highWord - lowWord <= 512)
    {
        ForcingInterval result{INFINITY, -INFINITY};
        for (uint32_t i = lowWord; i <= highWord; ++i)
        {
            const float y = sat(std::bit_cast<float>(i));
            result.m_low = std::min(y, result.m_low);
            result.m_high = std::max(y, result.m_high);
        }

        return result;
    }

    // The exact rational saturator is monotone. Comparing the two rounded
    // endpoint evaluations with the exact values costs less than 3e-6 in total.
    //
    const float cap = 1.f / filter.m_saturationTanhGain[0];
    return {std::max(-cap, std::nextafter(sat(x.m_low) - 3e-6f, -INFINITY)),
        std::min(cap, std::nextafter(sat(x.m_high) + 3e-6f, INFINITY))};
}

// Use a lower bound on dry power to enclose all audio states. From the first
// common boundary generate the candidate audio trajectory at full feedback.
// The caller separately checks that the dry and wet bounds force full feedback
// throughout the continuation needed for detector agreement.
//
extern "C" int WetForcing(double drive, double resonance, double cutoff,
    int size, const float* words, const float* dryLower, float wetCap, int* audioBoundary,
    float* pre, float* preHigh)
{
    if (std::fegetround() != FE_TONEAREST)
    {
        return -1;
    }

    auto reference = Configure(drive, resonance, cutoff);
    std::array<ForcingInterval, 4> stages{{{-500, 500}, {-1.08f, 1.08f}, {-1.08f, 1.08f}, {-1.08f, 1.08f}}};
    const float alpha = reference.m_stage1.m_alpha[0];
    const float K = reference.m_baseFeedback[0];
    const float R = reference.m_referencePowerGain[0];
    *audioBoundary = -1;
    for (int t = 0; t < size; ++t)
    {
        const bool known = *audioBoundary >= 0 || R * dryLower[t] >= wetCap;
        const float lo = K * stages[3].m_low;
        const float hi = K * stages[3].m_high;
        ForcingInterval input{words[t] - (known ? hi : std::max(0.f, hi)),
            words[t] - (known ? lo : std::min(0.f, lo))};
        for (int i = 0; i < 4; ++i)
        {
            stages[i] = Pole(stages[i], input, alpha);
            if (i < 3)
            {
                input = Saturated(reference, stages[i]);
            }
        }

        if (*audioBoundary < 0)
        {
            reference.Process(words[t]);
        }

        pre[t] = stages[3].m_low;
        preHigh[t] = stages[3].m_high;
        const std::array<float, 4> actual{{reference.m_stage1.m_output[0], reference.m_stage2.m_output[0],
            reference.m_stage3.m_output[0], reference.m_stage4.m_output[0]}};
        bool common = true;
        for (int i = 0; i < 4; ++i)
        {
            if (*audioBoundary < 0 && !(actual[i] >= stages[i].m_low && actual[i] <= stages[i].m_high))
            {
                return t + 1;
            }

            common = common && stages[i].Common();
        }

        if (*audioBoundary < 0 && common)
        {
            *audioBoundary = t + 1;
        }
    }

    return 0;
}

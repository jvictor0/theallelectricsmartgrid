#include "stock.cpp"
#include <bit>
#include <cstdint>
#include <cstdio>

int main()
{
    constexpr double x_rate = 192000.0;
    const double minHz = 20 * std::pow(1000.0, 345.0 / 360.0);
    const auto first = std::bit_cast<uint32_t>(static_cast<float>(minHz / x_rate));
    const auto last = std::bit_cast<uint32_t>(static_cast<float>(20000.0 / x_rate));
    float minAlpha = 1.0f;
    float maxAlpha = 0.0f;
    for (auto bits = first; bits <= last; ++bits)
    {
        const float frequency = std::bit_cast<float>(bits);
        const auto stage = Configure(1., 1., frequency * x_rate);
        minAlpha = std::min(minAlpha, stage.m_stage4.m_alpha[0]);
        maxAlpha = std::max(maxAlpha, stage.m_stage4.m_alpha[0]);
    }

    // Positive finite float operations below are monotone. Saturation normalization
    // is bounded above by one. Use conservative endpoints for the prepared controls.
    //
    const auto edge = Configure(65.0 / 72, 57.0 / 72, minHz);
    if (!(minAlpha >= 0.381f && maxAlpha <= 0.47f
        && edge.m_saturationGain[0] >= 3.73f && edge.m_feedback[0] >= 1.68f))
    {
        return 1;
    }

    auto upper = edge;
    upper.m_stage4.m_alpha = Filter::Vector::Splat(0.381f);
    upper.m_saturationGain = Filter::Vector::Splat(3.73f);
    upper.m_saturationTanhGain = Filter::Vector::Splat(1.0f);
    upper.UpdateFeedbackCoefficients();
    if (!(upper.m_kSafe[0] < 1.68f))
    {
        return 2;
    }

    const double hz = 20 * std::pow(1000., 356.5 / 360.);
    auto a = Configure(109. / 120., 40. / 72., hz);
    auto b = Configure(109. / 120., 45. / 72., hz);
    if (!(a.m_feedback[0] > a.m_kSafe[0] && b.m_feedback[0] > a.m_kSafe[0]
        && a.m_baseFeedback[0] == a.m_kSafe[0] && b.m_baseFeedback[0] == a.m_kSafe[0]
        && a.m_referencePowerGain[0] == b.m_referencePowerGain[0]))
    {
        return 3;
    }

    uint32_t rng = 72109;
    for (size_t n = 0; n < 192000; ++n)
    {
        rng = rng * 1664525u + 1013904223u;
        const float x = static_cast<float>(rng >> 8) / 16777216.0f - 0.5f;
        if (std::bit_cast<uint32_t>(a.Process(x)[0]) != std::bit_cast<uint32_t>(b.Process(x)[0]))
        {
            return 4;
        }
    }

    std::printf("{\"float_cutoffs\":%u,\"min_alpha\":%.12g,\"max_alpha\":%.12g,"
        "\"box_prepared_gain_min\":%.12g,\"box_requested_feedback_min\":%.12g,"
        "\"box_cap_upper\":%.12g,\"witness_hz\":%.12g,\"witness_cap\":%.12g,"
        "\"witness_requests\":[%.12g,%.12g],\"waveform_samples\":192000}\n",
        last - first + 1, minAlpha, maxAlpha, edge.m_saturationGain[0], edge.m_feedback[0],
        upper.m_kSafe[0], hz, a.m_kSafe[0], a.m_feedback[0], b.m_feedback[0]);
}

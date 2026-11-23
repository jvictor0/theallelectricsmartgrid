#include "stock.cpp"
#include <algorithm>
#include <cfenv>
#include <cmath>

using DrySVF = TPTSVFBandPassFilterGeneric<float, 1>;

namespace
{
constexpr double x_u = 0x1p-24;
constexpr double x_outerPad = 1e-12;
constexpr double x_innerPad = 1e-18;

struct DryTube
{
    DrySVF m_center;
    double m_radius;

    double Advance(float input, double uncertainty)
    {
        const double x = std::abs(static_cast<double>(m_center.m_state.m_ic1[0]));
        const double y = std::abs(static_cast<double>(m_center.m_state.m_ic2[0]));
        const double local = x+y+.02*std::abs(input);

        // The tap uses the pre-update integrators. The next radius encloses
        // the integrators after the update. dry_power_lower.py checks both.
        //
        const double tapRadius = (1.17*m_radius+.005*uncertainty+10*x_u*local+x_innerPad)*
            (1+x_outerPad);
        m_radius = (.99601*m_radius+.02*uncertainty+17*x_u*local+x_innerPad)*
            (1+x_outerPad);
        m_center.Process(input);
        return tapRadius;
    }
};
}

// Return the lower dry power BEFORE processing each corresponding sample.
// The incoming power is nonnegative and the incoming integrators can be
// any reset-reachable states from prior inputs of magnitude at most 241.
//
extern "C" int DryPowerLower(double drive, double resonance, double cutoffHz, int count,
    const float* input, float* lower)
{
    if (count < 0 || (count != 0 && (!input || !lower)) || std::fegetround() != FE_TONEAREST)
    {
        return 1;
    }

    auto filter = Configure(drive,resonance,cutoffHz);
    const auto& detector = filter.m_inputEnvelope;
    const auto check = [](const DrySVF& svf)
    {
        return svf.m_state.m_a1[0] == 0x1.fde64cp-1f &&
            svf.m_state.m_a2[0] == 0x1.0bc136p-8f &&
            svf.m_state.m_a3[0] == 0x1.1933eap-16f;
    };
    const double beta = detector.m_powerAlpha[0];
    if (!check(detector.m_stage1) || !check(detector.m_stage2) || beta != 0x1.289674p-12)
    {
        return 2;
    }

    DryTube first{detector.m_stage1,500};
    DryTube second{detector.m_stage2,1200};
    const double leak = 1-beta-x_u-5*x_u*beta;
    const double inject = beta*(1-5*x_u);
    double power = 0;
    for (int i = 0; i < count; ++i)
    {
        const float sample = input[i];
        if (!(std::abs(sample) <= 241))
        {
            return 3;
        }

        lower[i] = std::nextafter(static_cast<float>(power), 0.f);
        const double firstRadius = first.Advance(sample,0);
        const double secondRadius = second.Advance(first.m_center.GetBandPass()[0],firstRadius);
        const double center = std::abs(static_cast<double>(second.m_center.GetBandPass()[0]));
        const double tap = std::max(0., (center-secondRadius)*(1-x_outerPad));
        power = std::max(0., (leak*power+inject*tap*tap)*(1-x_outerPad)-x_innerPad);
    }

    return 0;
}

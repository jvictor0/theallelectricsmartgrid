#include "stock.cpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <string_view>

int main(int argc, char** argv)
{
    const bool wholeSurvey = argc == 2 && std::string_view(argv[1]) == "--whole-survey";
    if (argc != 1 && !wholeSurvey)
    {
        return 3;
    }

    using Vector = Filter::Vector;
    const auto bits = [](float x) { return std::bit_cast<uint32_t>(x); };
    Filter filter;
    const auto first = bits((wholeSurvey ? 20.0f : 92.0f) / 192000.0f);
    const auto last = bits((wholeSurvey ? 20000.0f : 101.0f) / 192000.0f);
    const auto gainFirst = bits(0.25f);
    const auto gainLast = bits(wholeSurvey ? 5.0f : 0.261f);
    const double radius = wholeSurvey ? 0.9999 : 0.999;
    for (auto i = gainFirst; i <= gainLast; ++i)
    {
        float g = std::bit_cast<float>(i);
        filter.SetSaturationGain(Vector::Splat(g));
        double t = filter.m_saturationTanhGain[0];
        double slope = static_cast<double>(g) / t;
        if (!(t > 0.24 && t <= 1 && slope > 0 && slope < 5.01) ||
            (!wholeSurvey && !(slope > 1.01701 && slope < 1.02199)))
        {
            return 1;
        }
    }

    for (auto i = first; i <= last; ++i)
    {
        filter.SetCutoffAndResonance(Vector::Splat(std::bit_cast<float>(i)), Vector());
        const double alpha = filter.m_stage4.m_alpha[0];
        const auto& detector = filter.m_inputEnvelope;
        const auto& svf = detector.m_stage1.m_state;
        double a1 = svf.m_a1[0];
        double a2 = svf.m_a2[0];
        double a3 = svf.m_a3[0];
        double u = 2 * a1 - 1;
        double v = 2 * a2;
        double w = 1 - 2 * a3;

        // Test positive definiteness of radius^2 P - A^T P A with P = [[1,.5],[.5,1]].
        // The margins exceed binary64 dot-product and determinant roundoff.
        //
        const double q = radius * radius;
        double xx = q - (u*u + u*v + v*v);
        double xy = q*0.5 - (-u*v + 0.5*(u*w - v*v) + v*w);
        double yy = q - (v*v - v*w + w*w);
        const double beta = detector.m_powerAlpha[0];
        const double critical = filter.m_criticalFeedback[0];
        if (!(alpha > 0.0006 && alpha < 0.5 && a1 > 0 && a1 < 1 &&
              a2 > 0 && a2 < 1 && a3 > 0 && a3 < 1 &&
              beta > 0.00002 && beta < 0.1 && critical >= 4 && critical < 9 &&
              xx > 1e-8 && yy > 1e-8 && xx*yy - xy*xy > 1e-10))
        {
            return 2;
        }

        if (!wholeSurvey && !(alpha > 0.00290001 && alpha < 0.00339999 &&
            a2 < 0.002 && a3 < 0.00001 && beta < 0.01))
        {
            return 2;
        }
    }

    std::printf("{\"binary32_cutoffs\":%u,\"binary32_gains\":%u}\n",
                last - first + 1, gainLast - gainFirst + 1);
}

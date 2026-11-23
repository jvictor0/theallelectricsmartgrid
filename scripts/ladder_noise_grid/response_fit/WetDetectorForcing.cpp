#include "DetectorMemory.cpp"

namespace
{
constexpr double x_round = 0x1p-24;
constexpr float x_powerCap = 2.0780001f;

double Up(double n)
{
    return std::nextafter(n, INFINITY);
}

float FloatUp(double n)
{
    const float f = static_cast<float>(n);
    return static_cast<double>(f) < n ? std::nextafter(f, INFINITY) : f;
}

struct SVFComparison
{
    double m_x;
    double m_y;
    double m_input;
    double m_radius;
    double m_inputRadius;

    double TapRadius() const
    {
        return Up(1.17*m_radius + .0041*m_inputRadius +
            10*x_round*(m_x+m_y+.02*m_input) + 1e-18);
    }

    double NextRadius() const
    {
        return Up(.99601*m_radius + .009*m_inputRadius +
            10.2*x_round*(m_x+m_y+.02*m_input) + 1e-18);
    }
};

SVFComparison Compare(const DetectorSVF& filter, float input, double radius, double inputRadius)
{
    return {std::abs(static_cast<double>(filter.m_state.m_ic1[0])),
        std::abs(static_cast<double>(filter.m_state.m_ic2[0])),
        std::abs(static_cast<double>(input)), radius, inputRadius};
}

struct ReferenceSVF
{
    double m_a1;
    double m_a2;
    double m_a3;
    double m_x = 0;
    double m_y = 0;
    double m_radius;

    void Process(float input)
    {
        // Use a binary64 reference for enumeration so the enclosure includes
        // only one binary32 trajectory's roundoff.
        //
        m_radius = Up(.99601*m_radius + 5.2*x_round*(
            std::abs(m_x)+std::abs(m_y)+.02*std::abs(static_cast<double>(input))) + 1e-18);
        const double diff = input-m_y;
        const double nextX = (2*m_a1-1)*m_x + 2*m_a2*diff;
        const double nextY = m_y + 2*m_a2*m_x + 2*m_a3*diff;
        m_x = nextX;
        m_y = nextY;
    }
};

// Select a small box in the first 12,000 samples after the previous cut.
// Deduplicate between eight-sample blocks: all retained words are exact.
//
int Coalesce(DetectorSVF svf, const std::vector<float>& input, int begin, double radius,
    std::vector<float>& band, int* stats)
{
    const int count = static_cast<int>(input.size());
    int cut = -1;
    WordInterval first{}, second{};
    uint64_t best = 64000001;
    ReferenceSVF center{svf.m_state.m_a1[0],svf.m_state.m_a2[0],svf.m_state.m_a3[0],0,0,radius};
    ReferenceSVF nominal = center;
    for (int i = begin; i < count; ++i)
    {
        center.Process(input[i]);
        const auto a = WordInterval::Around(center.m_x,1.16*center.m_radius);
        const auto b = WordInterval::Around(center.m_y,1.16*center.m_radius);
        if (a.Count() < best && b.Count() <= (best-1)/a.Count())
        {
            best = a.Count()*b.Count();
            first = a;
            second = b;
            cut = i+1;
            nominal = center;
        }

        if (cut >= 0 && (best <= 4096 || i+1 >= begin+12000))
        {
            break;
        }
    }

    if (cut < 0)
    {
        return -1;
    }

    if (stats)
    {
        stats[0] = cut;
    }

    auto compatible = [&nominal](uint64_t word)
    {
        const double a = std::bit_cast<float>(static_cast<uint32_t>(word>>32));
        const double b = std::bit_cast<float>(static_cast<uint32_t>(word));
        const double x = a-nominal.m_x;
        const double y = b-nominal.m_y;
        return x*x+x*y+y*y <= Up(nominal.m_radius*nominal.m_radius*1.000000000001);
    };

    std::vector<uint64_t> states;
    states.reserve(best);
    for (uint64_t aa = first.m_first; aa <= first.m_last; ++aa)
    {
        for (uint64_t bb = second.m_first; bb <= second.m_last; ++bb)
        {
            const uint64_t word = (aa<<32)|bb;
            if (compatible(word))
            {
                states.push_back(word);
            }
        }
    }

    if (stats)
    {
        stats[1] = static_cast<int>(states.size());
    }

    int n = cut;
    while (states.size() > 1 && n < count)
    {
        const int end = std::min(count,n+8);
        for (auto& state : states)
        {
            Unpack(svf,state);
            for (int i = n; i < end; ++i)
            {
                svf.Process(input[i]);
            }

            state = Pack(svf);
        }

        for (int i = n; i < end; ++i)
        {
            nominal.Process(input[i]);
        }

        std::erase_if(states,[&compatible](uint64_t word)
        {
            return !compatible(word);
        });
        Deduplicate(states);
        n = end;
    }

    if (stats)
    {
        stats[2] = static_cast<int>(states.size());
    }

    if (states.size() != 1)
    {
        return -1;
    }

    Unpack(svf,states.front());
    for (int i = n; i < count; ++i)
    {
        svf.Process(input[i]);
        band[i] = svf.GetBandPass()[0];
    }

    return n;
}

struct DetectorTube
{
    double m_firstRadius = 3;
    double m_secondRadius = 4;

    std::pair<double,double> SquareBounds(BandPassedEnvelopeFollower& detector, float input, double radius)
    {
        const auto cmp1 = Compare(detector.m_stage1,input,m_firstRadius,radius);
        detector.m_stage1.Process(input);
        const float band1 = detector.m_stage1.GetBandPass()[0];
        const auto cmp2 = Compare(detector.m_stage2,band1,m_secondRadius,cmp1.TapRadius());
        detector.m_stage2.Process(band1);
        const double band2 = std::abs(static_cast<double>(detector.m_stage2.GetBandPass()[0]));
        m_firstRadius = std::min(6.,cmp1.NextRadius());
        m_secondRadius = std::min(8.,cmp2.NextRadius());
        const double tapRadius = cmp2.TapRadius();
        const double tapLow = std::max(0.,std::nextafter(band2-tapRadius,-INFINITY));
        const double tapHigh = Up(band2+tapRadius);
        return {std::nextafter(tapLow*tapLow*(1-2*x_round)-2e-24,-INFINITY),
            Up(tapHigh*tapHigh*(1+2*x_round)+2e-24)};
    }
};

void StepPowerBox(float beta, double vlo, double vhi, float& low, float& high)
{
    // Bound the rounded EMA about the exact affine update for all powers
    // in the box, including evaluation of these endpoints in binary64.
    // The global power invariant can only narrow the result.
    //
    const double error = x_round*(1.01*high+3.01*beta*vhi) + 1e-18;
    const double upperReal = (1.-beta)*high+beta*vhi+error;
    const double lowerReal = (1.-beta)*low+beta*vlo-error;
    high = std::min(x_powerCap,FloatUp(Up(upperReal)));
    low = std::max(0.f,-FloatUp(Up(-lowerReal)));
}

bool CorrectCoefficients(const BandPassedEnvelopeFollower& detector)
{
    const auto& coeff = detector.m_stage1.m_state;
    // Exact words pinned by wet_detector_forcing.py.
    //
    return std::bit_cast<uint32_t>(coeff.m_a1[0]) == 0x3f7ef326 &&
        std::bit_cast<uint32_t>(coeff.m_a2[0]) == 0x3b85e09b &&
        std::bit_cast<uint32_t>(coeff.m_a3[0]) == 0x378c99f5 &&
        std::bit_cast<uint32_t>(detector.m_powerAlpha[0]) == 0x39944b3a;
}

}

// Return zero for a completed enclosure and nonzero for invalid input.
// upper[i] bounds the old wet EMA immediately before processing pre[i].
//
static int EncloseWetDetector(double drive, double resonance, double cutoff, int count,
    const float* pre, float* upper, int* cuts, int* stats)
{
    std::fill_n(cuts,3,-1);
    if (stats)
    {
        std::fill_n(stats,9,-1);
    }

    int& erasure = cuts[2];
    if (count < 1)
    {
        return 1;
    }

    auto filter = Configure(drive,resonance,cutoff);
    auto& detector = filter.m_feedbackEnvelope;
    const float beta = detector.m_powerAlpha[0];
    if (!CorrectCoefficients(detector))
    {
        return 2;
    }

    std::vector<float> input(pre,pre+count), first(count), second(count);
    for (float x : input)
    {
        if (!(std::abs(static_cast<double>(x)) <= 1.073))
        {
            return 3;
        }
    }

    const int firstCut = Coalesce(detector.m_stage1,input,0,3,first,stats);
    const int secondCut = firstCut < 0 ? -1 : Coalesce(detector.m_stage2,first,firstCut,4,second,stats ? stats+3 : nullptr);
    cuts[0] = firstCut;
    cuts[1] = secondCut;
    DetectorTube tube;
    float low = 0;
    float high = x_powerCap;
    std::vector<uint32_t> powers;
    for (int i = 0; i < count; ++i)
    {
        upper[i] = high;
        double vlo;
        double vhi;
        if (secondCut >= 0 && i >= secondCut)
        {
            vlo = vhi = second[i]*second[i];
        }
        else
        {
            const auto bounds = tube.SquareBounds(detector,input[i],0);
            vlo = bounds.first;
            vhi = bounds.second;
        }

        if (powers.empty() && secondCut >= 0 && i >= secondCut)
        {
            const auto loWord = std::bit_cast<uint32_t>(low);
            const auto hiWord = std::bit_cast<uint32_t>(high);
            if (hiWord-loWord <= 65536)
            {
                if (stats)
                {
                    stats[6] = i;
                    stats[7] = static_cast<int>(hiWord-loWord+1);
                }

                for (uint32_t word = loWord; word <= hiWord; ++word)
                {
                    powers.push_back(word);
                }
            }
        }

        if (!powers.empty())
        {
            if (powers.size() == 1 && erasure < 0)
            {
                erasure = i;
            }

            const float v = static_cast<float>(vhi);
            for (auto& word : powers)
            {
                // v is a square and p starts at +0 or above. At v=0 the
                // subtraction is exact and RN(beta*p) <= p. For any v>=0
                // monotonicity in v preserves nonnegativity. Exact opposite
                // sign cancellation produces +0 under nearest rounding.
                //
                const float p = std::bit_cast<float>(word);
                word = std::bit_cast<uint32_t>(p + beta*(v-p));
            }

            Deduplicate(powers);
            low = std::bit_cast<float>(powers.front());
            high = std::bit_cast<float>(powers.back());
            if (powers.size() == 1 && erasure < 0)
            {
                erasure = i+1;
            }
        }
        else
        {
            StepPowerBox(beta,vlo,vhi,low,high);
        }
    }

    if (stats)
    {
        stats[8] = static_cast<int>(powers.size());
    }

    return 0;
}

extern "C" int WetDetectorForcing(double drive, double resonance, double cutoff, int count,
    const float* pre, float* upper, int* erasure)
{
    int cuts[3];
    const int code = EncloseWetDetector(drive,resonance,cutoff,count,pre,upper,cuts,nullptr);
    *erasure = cuts[2];
    return code;
}

extern "C" int WetDetectorBoxUpper(double drive, double resonance, double cutoff, int count,
    const float* lo, const float* hi, float* upper)
{
    if (count < 1)
    {
        return 1;
    }

    auto filter = Configure(drive,resonance,cutoff);
    auto& detector = filter.m_feedbackEnvelope;
    if (!CorrectCoefficients(detector))
    {
        return 2;
    }

    const float beta = detector.m_powerAlpha[0];
    const float maxPre = -FloatUp(-1.073);
    float low = 0;
    float high = x_powerCap;
    DetectorTube tube;
    for (int i = 0; i < count; ++i)
    {
        const float left = std::max(lo[i],-maxPre);
        const float right = std::min(hi[i],maxPre);
        if (!(left <= right))
        {
            return 3;
        }

        upper[i] = high;
        const float center = static_cast<float>((static_cast<double>(left)+right)/2);
        const double radius = Up(std::max(static_cast<double>(center)-left,static_cast<double>(right)-center));
        const auto bounds = tube.SquareBounds(detector,center,radius);
        StepPowerBox(beta,bounds.first,bounds.second,low,high);
    }

    return 0;
}

// Optional diagnostics store three values per section in stats[9].
// Each section reports the enumeration start, initial count and final count.
//
extern "C" int WetDetectorForcingStats(double drive, double resonance, double cutoff, int count,
    const float* pre, float* upper, int* cuts, int* stats)
{
    return EncloseWetDetector(drive,resonance,cutoff,count,pre,upper,cuts,stats);
}

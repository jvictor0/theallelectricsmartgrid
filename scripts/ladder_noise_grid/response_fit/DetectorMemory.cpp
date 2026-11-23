#include "stock.cpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using DetectorSVF = TPTSVFBandPassFilterGeneric<float, 1>;

struct WordInterval
{
    uint32_t m_first;
    uint32_t m_last;

    static WordInterval Around(double center, double radius)
    {
        const float lo = std::nextafter(static_cast<float>(center-radius), -INFINITY);
        const float hi = std::nextafter(static_cast<float>(center+radius), INFINITY);
        if (!(lo > 0 || hi < 0))
        {
            return {0, UINT32_MAX};
        }

        const auto a = std::bit_cast<uint32_t>(lo);
        const auto b = std::bit_cast<uint32_t>(hi);
        return {std::min(a,b),std::max(a,b)};
    }

    uint64_t Count() const
    {
        return static_cast<uint64_t>(m_last)-m_first+1;
    }
};

static uint64_t Pack(const DetectorSVF& svf)
{
    return (static_cast<uint64_t>(std::bit_cast<uint32_t>(svf.m_state.m_ic1[0]))<<32) |
        std::bit_cast<uint32_t>(svf.m_state.m_ic2[0]);
}

static void Unpack(DetectorSVF& svf, uint64_t packed)
{
    svf.m_state.m_ic1 = Filter::Vector::Splat(std::bit_cast<float>(static_cast<uint32_t>(packed>>32)));
    svf.m_state.m_ic2 = Filter::Vector::Splat(std::bit_cast<float>(static_cast<uint32_t>(packed)));
}

template<typename Word>
static void Deduplicate(std::vector<Word>& words)
{
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
}

// Find a small rectangle containing every possible pair of integrator words,
// then propagate its complete finite image through the actual generated step.
//
static int ForgetSVF(DetectorSVF svf, const std::vector<float>& input, int begin, double radius,
    std::vector<float>& band, uint64_t& candidates, uint64_t& terminal)
{
    int cut = -1;
    WordInterval first{}, second{};
    uint64_t best = UINT64_MAX;
    for (int i = begin; i < std::min(static_cast<int>(input.size()), begin+100000); ++i)
    {
        const double x = std::abs(static_cast<double>(svf.m_state.m_ic1[0]));
        const double y = std::abs(static_cast<double>(svf.m_state.m_ic2[0]));
        radius = .99601*radius + 17*0x1p-24*(x+y+.02*std::abs(input[i])) + 1e-18;
        svf.Process(input[i]);
        const auto a = WordInterval::Around(svf.m_state.m_ic1[0], 1.16*radius);
        const auto b = WordInterval::Around(svf.m_state.m_ic2[0], 1.16*radius);
        if (a.Count() < best && b.Count() <= (best-1)/a.Count())
        {
            best = a.Count()*b.Count();
            first = a;
            second = b;
            cut = i+1;
        }
    }
    candidates = best;
    if (cut < 0 || best > 32000000)
    {
        return -1;
    }

    std::vector<uint64_t> words;
    words.reserve(best);
    for (uint64_t a = first.m_first; a <= first.m_last; ++a)
    {
        for (uint64_t b = second.m_first; b <= second.m_last; ++b)
        {
            words.push_back((a<<32)|b);
        }
    }

    int n = cut;
    while (words.size() > 1 && n+16 <= static_cast<int>(input.size()))
    {
        for (auto& state : words)
        {
            Unpack(svf,state);
            for (int j = 0; j < 16; ++j)
            {
                svf.Process(input[n+j]);
            }

            state = Pack(svf);
        }

        Deduplicate(words);
        n += 16;
    }
    if (words.size() != 1)
    {
        return -1;
    }

    Unpack(svf,words.front());
    for (int i = n; i < static_cast<int>(input.size()); ++i)
    {
        svf.Process(input[i]);
        band[i] = svf.GetBandPass()[0];
    }

    terminal = Pack(svf);
    return n;
}

// Filter coefficients and initial enclosing radii are checked by detector_memory.py.
// Every reported success includes equality of both SVFs and the power word.
//
extern "C" int DetectorMemory(double drive, double resonance, double cutoff, int count,
    const float* input, int* certified, uint64_t* candidates, uint32_t* terminal)
{
    auto filter = Configure(drive,resonance,cutoff);
    auto& detector = filter.m_inputEnvelope;
    std::vector<float> first(input,input+count), second(count), powerInput(count);
    uint64_t firstTerminal = 0;
    uint64_t secondTerminal = 0;
    certified[0] = ForgetSVF(detector.m_stage1, first, 0, 2.1e7, second, candidates[0], firstTerminal);
    if (certified[0] < 0)
    {
        return 1;
    }

    certified[1] = ForgetSVF(detector.m_stage2, second, certified[0], 3.1e12, powerInput, candidates[1], secondTerminal);
    if (certified[1] < 0)
    {
        return 2;
    }

    const float beta = detector.m_powerAlpha[0];
    auto step = [beta](float p, float u)
    {
        return p + beta*(u*u-p);
    };
    double radius = 2.1e26;
    float center = 0;
    int n = certified[1];
    WordInterval box{};
    for (; n < count; ++n)
    {
        const float v = powerInput[n]*powerInput[n];
        radius = (1.-beta+6*0x1p-24)*radius +
            10*0x1p-24*(std::abs(center)+beta*static_cast<double>(v)) + 1e-18;
        center = step(center,powerInput[n]);
        box = WordInterval::Around(center,radius);
        if (box.Count() <= 50000)
        {
            ++n;
            break;
        }
    }
    candidates[2] = box.Count();
    if (n >= count)
    {
        return 3;
    }

    std::vector<uint32_t> words;
    for (uint64_t a = box.m_first; a <= box.m_last; ++a)
    {
        words.push_back(static_cast<uint32_t>(a));
    }
    while (words.size() > 1 && n+32 <= count)
    {
        for (auto& word : words)
        {
            float p = std::bit_cast<float>(word);
            for (int j = 0; j < 32; ++j)
            {
                p = step(p,powerInput[n+j]);
            }

            word = std::bit_cast<uint32_t>(p);
        }

        Deduplicate(words);
        n += 32;
    }
    if (words.size() != 1)
    {
        return 3;
    }

    certified[2] = n;
    float p = std::bit_cast<float>(words[0]);
    for (; n < count; ++n)
    {
        p = step(p,powerInput[n]);
    }
    *terminal = std::bit_cast<uint32_t>(p);
    for (int i = 0; i < count; ++i)
    {
        detector.Process(input[i]);
    }
    const bool matches = Pack(detector.m_stage1) == firstTerminal &&
        Pack(detector.m_stage2) == secondTerminal &&
        std::bit_cast<uint32_t>(detector.GetPower()[0]) == *terminal;
    return matches ? 0 : 4;
}

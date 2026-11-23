#include "stock.cpp"
#include <algorithm>
#include <array>
#include <cmath>

using Stages = std::array<float, 4>;

static Stages ReadStages(const Filter& f)
{
    return {f.m_stage1.m_output[0], f.m_stage2.m_output[0], f.m_stage3.m_output[0], f.m_stage4.m_output[0]};
}

struct StageBox
{
    Stages m_low;
    Stages m_high;

    void Include(const Filter& filter)
    {
        const auto state = ReadStages(filter);
        for (size_t i = 0; i < state.size(); ++i)
        {
            m_low[i] = std::min(m_low[i], state[i]);
            m_high[i] = std::max(m_high[i], state[i]);
        }
    }
};

// Each corner is a member of the coordinate box; it need not be a reachable
// joint state. Its stock result supplies a necessary width for any method
// that replaces the state set with coordinate intervals after every sample.
//
extern "C" int WetHistoryBox(double drive, double resonance, double cutoff, int prefix, int count,
    const float* first, const float* second, const float* common,
    float* wet, float* otherWet, float* low, float* high)
{
    auto reference = Configure(drive, resonance, cutoff);
    auto other = reference;
    for (int i = 0; i < prefix; ++i)
    {
        reference.Process(first[i]);
        other.Process(second[i]);
    }

    StageBox box{ReadStages(reference), ReadStages(reference)};
    box.Include(other);
    for (int i = 0; i < count; ++i)
    {
        const auto before = reference;
        wet[i] = reference.Process(common[i])[0];
        otherWet[i] = other.Process(common[i])[0];
        low[i] = std::min(wet[i], otherWet[i]);
        high[i] = std::max(wet[i], otherWet[i]);
        StageBox next{ReadStages(reference), ReadStages(reference)};
        next.Include(other);
        for (const auto& stages : {box.m_low, box.m_high})
        {
            for (const float feedbackStage : {box.m_low[3], box.m_high[3]})
            {
                auto corner = before;
                corner.m_stage1.m_output = Filter::Vector::Splat(stages[0]);
                corner.m_stage2.m_output = Filter::Vector::Splat(stages[1]);
                corner.m_stage3.m_output = Filter::Vector::Splat(stages[2]);
                corner.m_stage4.m_output = Filter::Vector::Splat(feedbackStage);
                const float y = corner.Process(common[i])[0];
                const auto updated = ReadStages(corner);
                if (!std::isfinite(y) || !std::all_of(updated.begin(), updated.end(),
                    [](float value)
                    {
                        return std::isfinite(value);
                    }))
                {
                    return 1;
                }

                next.Include(corner);
                low[i] = std::min(low[i], y);
                high[i] = std::max(high[i], y);
            }
        }

        box = next;
    }

    return 0;
}

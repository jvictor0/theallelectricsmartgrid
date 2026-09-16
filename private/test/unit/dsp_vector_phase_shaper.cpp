#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "VectorPhaseShaper.hpp"

DOCTEST_TEST_CASE("VectorPhaseShaper wraps a negative elbow value that rounds to one")
{
    GlobalEnv::ResetPerTest();
    VectorPhaseShaperInternal shaper;
    VectorPhaseShaperInternal::Input input;
    AdaptiveWaveTable table;
    for (size_t sample = 0; sample < BasicWaveTable::x_tableSize; ++sample)
    {
        table.m_waveTable.m_table[sample] = std::sin(2.0 * M_PI * sample / BasicWaveTable::x_tableSize);
    }

    table.Generate();
    shaper.m_morphingWaveTable.SetLeft(&table);
    shaper.m_morphingWaveTable.SetRight(&table);
    shaper.m_dScale = 1.0f;
    shaper.m_d.m_output = 0.01f;
    shaper.m_v.m_output = 1.01f;
    shaper.m_freq = 0.001f;
    shaper.m_phase = 0.0f;
    shaper.Evaluate(input);
    float atBoundary = shaper.m_out;

    shaper.m_phase = 1e-10f;
    shaper.Evaluate(input);
    DOCTEST_CHECK(std::isfinite(shaper.m_out));
    DOCTEST_CHECK(std::abs(shaper.m_out - atBoundary) < 1e-5f);
}

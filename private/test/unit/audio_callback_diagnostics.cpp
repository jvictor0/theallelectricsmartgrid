#include "doctest.h"
#include "AudioCallbackDiagnostics.hpp"

DOCTEST_TEST_CASE("audio diagnostics distinguish callback arrival gaps from variable block budgets")
{
    AudioCallbackDiagnostics diagnostics;
    const auto first = diagnostics.Observe(1000000, 256, 48000.0);
    DOCTEST_CHECK(first.m_sequence == 1);
    DOCTEST_CHECK(first.m_gapUs == 0);
    DOCTEST_CHECK(first.m_previousBudgetUs == 0);

    const auto second = diagnostics.Observe(1005333, 128, 48000.0);
    DOCTEST_CHECK(second.m_sequence == 2);
    DOCTEST_CHECK(second.m_gapUs == 5333);
    DOCTEST_CHECK(second.m_previousBudgetUs == 5333);

    const auto late = diagnostics.Observe(1025333, 256, 48000.0);
    DOCTEST_CHECK(late.m_gapUs == 20000);
    DOCTEST_CHECK(late.m_previousBudgetUs == 2667);

    diagnostics.Reset();
    const auto restarted = diagnostics.Observe(9000000, 256, 48000.0);
    DOCTEST_CHECK(restarted.m_sequence == 1);
    DOCTEST_CHECK(restarted.m_gapUs == 0);
    DOCTEST_CHECK(restarted.m_previousBudgetUs == 0);
}

DOCTEST_TEST_CASE("audio diagnostics refuse to run fixed rate DSP at an incompatible device rate")
{
    DOCTEST_CHECK(AudioCallbackDiagnostics::CanRender(48000.0));
    DOCTEST_CHECK_FALSE(AudioCallbackDiagnostics::CanRender(44100.0));
    DOCTEST_CHECK_FALSE(AudioCallbackDiagnostics::CanRender(96000.0));
    DOCTEST_CHECK_FALSE(AudioCallbackDiagnostics::CanRender(0.0));
}

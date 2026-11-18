#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "ConservativeReuppingLadderCore.hpp"
#include "ConservativeReuppingLadderCoreGeneric.hpp"
#include <cstring>

namespace
{
using MonoDouble = MultiChannelNumber<double, 1>;

template<typename Number>
auto Bits(Number x)
{
    std::conditional_t<sizeof(Number) == 4, uint32_t, uint64_t> bits;
    std::memcpy(&bits, &x, sizeof(x));
    return bits;
}

}

DOCTEST_TEST_CASE("Conservative ladder: scalar and vector processing agree and reset preserves tuning")
{
    GlobalEnv::ResetPerTest();
    ConservativeReuppingLadderCore scalarLadder{};
    ConservativeReuppingLadderCoreGeneric<double, 1> vectorLadder{};
    scalarLadder.ComputeCoefficients({0.125, 0.7, 2});
    vectorLadder.ComputeCoefficients({MonoDouble::Splat(0.125), MonoDouble::Splat(0.7), MonoDouble::Splat(2)});
    for (size_t i = 0; i < 128; ++i)
    {
        const double x = (i % 7 == 0) ? 1 : -0.1;
        ConservativeReuppingLadderCore::Input a{x};
        ConservativeReuppingLadderCoreGeneric<double, 1>::Input b{MonoDouble::Splat(x)};
        scalarLadder.Process(a);
        vectorLadder.Process(b);
        DOCTEST_CHECK(Bits(scalarLadder.m_state.m_output) == Bits(vectorLadder.m_state.m_output[0]));
    }

    scalarLadder.Reset();
    DOCTEST_CHECK(scalarLadder.m_state.m_output == 0);
    DOCTEST_CHECK(scalarLadder.m_state.m_saturation.m_gain == 2);
}

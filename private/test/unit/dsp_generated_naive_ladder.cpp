#include "doctest.h"
#include "../support/GlobalEnv.hpp"
#include "NaiveReuppingLadder.hpp"
#include "NaiveReuppingLadderCore.hpp"
#include "NaiveReuppingLadderCoreGeneric.hpp"
#include "BandPassedEnvelopeFollowerCore.hpp"
#include "BandPassedEnvelopeFollowerCoreGeneric.hpp"
#include "NormalizedTanh.hpp"
#include "NormalizedTanhGeneric.hpp"
#include <cstring>
#include <limits>
#include <tuple>

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

template<typename Vector>
void CheckBits(const Vector& actual, const Vector& expected, size_t size)
{
    for (size_t lane = 0; lane < size; ++lane)
    {
        DOCTEST_CAPTURE(lane);
        DOCTEST_REQUIRE(Bits(actual[lane]) == Bits(expected[lane]));
    }
}

template<typename State>
auto SVFFields(State& s)
{
    return std::tie(s.m_k, s.m_a1, s.m_a2, s.m_a3, s.m_ic1, s.m_ic2,
        s.m_lowPass, s.m_bandPass, s.m_highPass, s.m_notch, s.m_allPass);
}

template<typename TupleA, typename TupleB, size_t... I>
void CheckFields(TupleA a, TupleB b, size_t size, std::index_sequence<I...>)
{
    (CheckBits(std::get<I>(a), std::get<I>(b), size), ...);
}

template<typename A, typename B>
void CheckSVF(A& a, B& b, size_t size)
{
    CheckFields(SVFFields(a), SVFFields(b), size, std::make_index_sequence<11>{});
}

template<typename A, typename B>
void CheckEnvelope(A& a, B& b, size_t size)
{
    CheckSVF(a.m_stage1, b.m_stage1.m_state, size);
    CheckSVF(a.m_stage2, b.m_stage2.m_state, size);
    CheckBits(a.m_power, b.m_power, size);
    CheckBits(a.m_powerAlpha, b.m_powerAlpha, size);
}

template<typename Number, size_t Size>
void CheckLadder(NaiveReuppingLadderCoreGeneric<Number, Size>& generated,
    NaiveReuppingLadderGeneric<Number, Size>& handwritten)
{
    auto& s = generated.m_state;
    auto& h = handwritten;
    const auto a = std::tie(s.m_alpha, s.m_saturation.m_gain, s.m_saturation.m_tanhGain,
        s.m_cutoff, s.m_feedback, s.m_stage1, s.m_stage2, s.m_stage3, s.m_stage4,
        s.m_input, s.m_output, s.m_kEff, s.m_criticalFeedback, s.m_kSafe,
        s.m_baseFeedback, s.m_referencePowerGain, s.m_dampingScale);
    const auto b = std::tie(h.m_stage1.m_alpha, h.m_saturationGain, h.m_saturationTanhGain,
        h.m_cutoff, h.m_feedback, h.m_stage1.m_output, h.m_stage2.m_output,
        h.m_stage3.m_output, h.m_stage4.m_output, h.m_input, h.m_output, h.m_kEff,
        h.m_criticalFeedback, h.m_kSafe, h.m_baseFeedback, h.m_referencePowerGain, h.m_dampingScale);
    CheckFields(a, b, Size, std::make_index_sequence<17>{});
    CheckEnvelope(s.m_inputEnvelope, h.m_inputEnvelope, Size);
    CheckEnvelope(s.m_feedbackEnvelope, h.m_feedbackEnvelope, Size);
}

template<typename Number, size_t Size>
void CheckTrajectory()
{
    using Core = NaiveReuppingLadderCoreGeneric<Number, Size>;
    using Vector = typename Core::Vector;
    Core generated{};
    NaiveReuppingLadderGeneric<Number, Size> handwritten;
    typename Core::LogicalSpec spec{};
    constexpr double x_cutoffs[] = {1e-7, 0.001, 0.03, 0.125, 0.1959, 0.196, 0.49, 0.5, 0.7};
    constexpr double x_gains[] = {0.001, 0.5, 1, 2, 5, 10};
    for (size_t sample = 0; sample < 1024; ++sample)
    {
        DOCTEST_CAPTURE(sizeof(Number));
        DOCTEST_CAPTURE(Size);
        DOCTEST_CAPTURE(sample);
        if (sample % 13 == 0)
        {
            for (size_t lane = 0; lane < Size; ++lane)
            {
                spec.m_cutoff[lane] = static_cast<Number>(x_cutoffs[(sample / 13 + lane) % 9]);
                spec.m_resonance[lane] = static_cast<Number>((sample / 13 + lane) % 5) / 4;
                spec.m_gain[lane] = static_cast<Number>(x_gains[(sample / 13 + lane * 2) % 6]);
            }

            generated.ComputeCoefficients(spec);
            handwritten.SetCutoffAndResonance(spec.m_cutoff, spec.m_resonance);
            handwritten.SetSaturationGain(spec.m_gain);
            CheckLadder(generated, handwritten);
            generated.ComputeCoefficients(spec);
            CheckLadder(generated, handwritten);
        }

        Vector x;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            x[lane] = static_cast<Number>(std::sin(static_cast<double>(sample * 17 + lane * 31)))
                * static_cast<Number>((sample % 7 == 0) ? 10 : 0.05);
        }

        if constexpr (Size == 3)
        {
            x.m_values[3] = std::numeric_limits<Number>::quiet_NaN();
        }

        typename Core::Input input{x};
        generated.Process(input);
        handwritten.Process(x);
        CheckLadder(generated, handwritten);
        if (sample % 101 == 100)
        {
            generated.Reset();
            handwritten.Reset();
            CheckLadder(generated, handwritten);
        }
    }
}
}

DOCTEST_TEST_CASE("Generated naive ladder: tuning, histories and trajectories match handwritten bits")
{
    GlobalEnv::Init();
    CheckTrajectory<float, 1>();
    CheckTrajectory<float, 2>();
    CheckTrajectory<float, 3>();
    CheckTrajectory<float, 4>();
    CheckTrajectory<double, 1>();
    CheckTrajectory<double, 2>();
    CheckTrajectory<double, 3>();
    CheckTrajectory<double, 4>();
}

DOCTEST_TEST_CASE("Generated nonlinear components: scalar tuning, clamp boundaries and reset")
{
    GlobalEnv::Init();
    for (double gain : {0.001, 0.5, 1.0, 2.0, 5.0})
    {
        NormalizedTanh scalar{};
        NormalizedTanhGeneric<double, 1> vector{};
        scalar.ComputeCoefficients({gain});
        vector.ComputeCoefficients({MonoDouble::Splat(gain)});
        for (double x : {-100.0, -3.0 / gain, -1.0, -0.0, 0.0, 1e-100, 1.0, 3.0 / gain, 100.0})
        {
            NormalizedTanh::Input a{x};
            NormalizedTanhGeneric<double, 1>::Input b{MonoDouble::Splat(x)};
            scalar.Process(a);
            vector.Process(b);
            const auto expected = TanhSaturatorGeneric<double, 1, true>::Tanh(b.m_x * gain)
                / TanhSaturatorGeneric<double, 1, true>::Tanh(MonoDouble::Splat(gain));
            DOCTEST_CHECK(Bits(scalar.m_state.m_output) == Bits(expected[0]));
            CheckBits(vector.m_state.m_output, expected, 1);
        }

        scalar.Reset();
        vector.Reset();
        DOCTEST_CHECK(scalar.m_state.m_output == 0);
        DOCTEST_CHECK(scalar.m_state.m_coefficients.m_gain == gain);
        CheckBits(vector.m_state.m_output, MonoDouble(), 1);
    }

    BandPassedEnvelopeFollowerCore scalarEnvelope{};
    BandPassedEnvelopeFollowerCoreGeneric<double, 1> vectorEnvelope{};
    scalarEnvelope.ComputeCoefficients({0.12, 0.03});
    vectorEnvelope.ComputeCoefficients({MonoDouble::Splat(0.12), MonoDouble::Splat(0.03)});
    for (size_t i = 0; i < 64; ++i)
    {
        const double x = (i % 7 == 0) ? 1 : -0.1;
        BandPassedEnvelopeFollowerCore::Input a{x};
        BandPassedEnvelopeFollowerCoreGeneric<double, 1>::Input b{MonoDouble::Splat(x)};
        scalarEnvelope.Process(a);
        vectorEnvelope.Process(b);
        DOCTEST_CHECK(Bits(scalarEnvelope.m_state.m_power) == Bits(vectorEnvelope.m_state.m_power[0]));
    }

    scalarEnvelope.Reset();
    DOCTEST_CHECK(scalarEnvelope.m_state.m_power == 0);
    DOCTEST_CHECK(scalarEnvelope.m_state.m_powerAlpha == 0.03);

    NaiveReuppingLadderCore scalarLadder{};
    NaiveReuppingLadderCoreGeneric<double, 1> vectorLadder{};
    scalarLadder.ComputeCoefficients({0.125, 0.7, 2});
    vectorLadder.ComputeCoefficients({MonoDouble::Splat(0.125), MonoDouble::Splat(0.7), MonoDouble::Splat(2)});
    for (size_t i = 0; i < 128; ++i)
    {
        const double x = (i % 7 == 0) ? 1 : -0.1;
        NaiveReuppingLadderCore::Input a{x};
        NaiveReuppingLadderCoreGeneric<double, 1>::Input b{MonoDouble::Splat(x)};
        scalarLadder.Process(a);
        vectorLadder.Process(b);
        DOCTEST_CHECK(Bits(scalarLadder.m_state.m_output) == Bits(vectorLadder.m_state.m_output[0]));
    }

    scalarLadder.Reset();
    DOCTEST_CHECK(scalarLadder.m_state.m_output == 0);
    DOCTEST_CHECK(scalarLadder.m_state.m_cutoff == 0.125);
}

#include "OnePoleTPTLowPass.hpp"
#include "OnePoleTPTLowPassGeneric.hpp"
#include "OnePoleTPTHighPass.hpp"
#include "OnePoleTPTHighPassGeneric.hpp"
#include "TPTBaseWidth.hpp"
#include "TPTBaseWidthGeneric.hpp"
#include "TPTSVF.hpp"
#include "TPTSVFGeneric.hpp"
#include "MorphingTPTSVF.hpp"
#include "MorphingTPTSVFGeneric.hpp"
#include "TPTSVF4PoleLowPass.hpp"
#include "TPTSVF4PoleLowPassGeneric.hpp"
#include "TPTSVF4PoleBandPass.hpp"
#include "TPTSVF4PoleBandPassGeneric.hpp"
#include "TPTSVF4PoleHighPass.hpp"
#include "TPTSVF4PoleHighPassGeneric.hpp"
#include "TPTLadder.hpp"
#include "TPTLadderGeneric.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>

template<typename To, typename From>
To Bits(From value)
{
    static_assert(sizeof(To) == sizeof(From));
    To result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::array<double, 13> ScalarResponses(const std::array<double, 10>& v,
    const ResponseFrequencyGeneric<double>& frequency)
{
    return {
        OnePoleTPTLowPass::MagnitudeSquared({v[0]}, frequency),
        OnePoleTPTHighPass::MagnitudeSquared({v[0]}, frequency),
        TPTBaseWidth::MagnitudeSquared({{v[0]}, {v[1]}}, frequency),
        TPTSVF::LowPassMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVF::BandPassMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVF::HighPassMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVF::NotchMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVF::AllPassMagnitudeSquared({v[0], v[4]}, frequency),
        MorphingTPTSVF::MagnitudeSquared({{v[0], v[4]}, v[7], v[8], v[9]}, frequency),
        TPTSVF4PoleLowPass::MagnitudeSquared({{v[0], v[4]}, {v[1], v[5]}}, frequency),
        TPTSVF4PoleBandPass::MagnitudeSquared({{v[0], v[4]}, {v[1], v[5]}}, frequency),
        TPTSVF4PoleHighPass::MagnitudeSquared({{v[0], v[4]}, {v[1], v[5]}}, frequency),
        TPTLadder::MagnitudeSquared({v[0], v[1], v[2], v[3], v[4]}, frequency),
    };
}

template<typename Number, size_t Size>
std::array<MultiChannelNumber<Number, Size>, 13> VectorResponses(const std::array<MultiChannelNumber<Number, Size>, 10>& v,
    const ResponseFrequencyGeneric<MultiChannelNumber<Number, Size>>& frequency)
{
    return {
        OnePoleTPTLowPassGeneric<Number, Size>::MagnitudeSquared({v[0]}, frequency),
        OnePoleTPTHighPassGeneric<Number, Size>::MagnitudeSquared({v[0]}, frequency),
        TPTBaseWidthGeneric<Number, Size>::MagnitudeSquared({{v[0]}, {v[1]}}, frequency),
        TPTSVFGeneric<Number, Size>::LowPassMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVFGeneric<Number, Size>::BandPassMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVFGeneric<Number, Size>::HighPassMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVFGeneric<Number, Size>::NotchMagnitudeSquared({v[0], v[4]}, frequency),
        TPTSVFGeneric<Number, Size>::AllPassMagnitudeSquared({v[0], v[4]}, frequency),
        MorphingTPTSVFGeneric<Number, Size>::MagnitudeSquared({{v[0], v[4]}, v[7], v[8], v[9]}, frequency),
        TPTSVF4PoleLowPassGeneric<Number, Size>::MagnitudeSquared({{v[0], v[4]}, {v[1], v[5]}}, frequency),
        TPTSVF4PoleBandPassGeneric<Number, Size>::MagnitudeSquared({{v[0], v[4]}, {v[1], v[5]}}, frequency),
        TPTSVF4PoleHighPassGeneric<Number, Size>::MagnitudeSquared({{v[0], v[4]}, {v[1], v[5]}}, frequency),
        TPTLadderGeneric<Number, Size>::MagnitudeSquared({v[0], v[1], v[2], v[3], v[4]}, frequency),
    };
}

// An independent complex transfer calculation uses the digital delay q.
// It exercises the half-angle bridge, output selection, mixing and cascades.
//
std::array<double, 13> ComplexReference(const std::array<double, 10>& v, double cycles)
{
    using C = std::complex<double>;
    const C q = cycles == 0 ? C(1, 0) : cycles == 0.5 ? C(-1, 0) : std::polar(1.0, -2 * std::acos(-1.0) * cycles);
    const C a = 1.0 - q;
    const C b = 1.0 + q;
    const auto lp = [&](double g)
    {
        return g * b / (a + g * b);
    };

    const auto hp = [&](double g)
    {
        return a / (a + g * b);
    };

    const auto svf = [&](double g, double k)
    {
        const C denominator = a * a + k * g * a * b + g * g * b * b;
        return std::array<C, 3>{g * g * b * b / denominator, g * a * b / denominator, a * a / denominator};
    };
    const auto first = svf(v[0], v[4]);
    const auto second = svf(v[1], v[5]);
    const C cascade = lp(v[0]) * lp(v[1]) * lp(v[2]) * lp(v[3]);
    return {std::norm(lp(v[0])), std::norm(hp(v[0])), std::norm(hp(v[0]) * lp(v[1])),
        std::norm(first[0]), std::norm(first[1]), std::norm(first[2]), std::norm(first[0] + first[2]),
        std::norm(first[0] - v[4] * first[1] + first[2]),
        std::norm(v[7] * first[0] + v[8] * first[1] + v[9] * first[2]),
        std::norm(first[0] * second[0]), std::norm(first[1] * second[1]), std::norm(first[2] * second[2]),
        std::norm(cascade / (1.0 + v[4] * cascade))};
}

void CheckComplexReference()
{
    for (double gain : {0.001, 0.1, 1.0, 10.0})
    {
        const std::array<double, 10> values{gain, 0.7, 0.4, 1.3, 0.8, 1.4, 1.8, 0.6, -0.3, 0.9};
        for (double cycles : {0.0, 1e-6, 0.01, 0.1, 0.25, 0.499, 0.5})
        {
            const double sine = cycles == 0.5 ? 1 : std::sin(std::acos(-1.0) * cycles);
            const double cosine = cycles == 0.5 ? 0 : std::cos(std::acos(-1.0) * cycles);
            const ResponseFrequencyGeneric<double> frequency{cycles, sine, cosine, {1 - 2 * sine * sine, -2 * sine * cosine}};
            const auto actual = ScalarResponses(values, frequency);
            const auto expected = ComplexReference(values, cycles);
            for (size_t i = 0; i < actual.size(); ++i)
            {
                assert(std::isfinite(actual[i]));
                assert(std::abs(actual[i] - expected[i]) <= 1e-11 * (1 + expected[i]));
            }
        }
    }
}

template<typename Number, size_t Size>
void Run()
{
    using Vector = MultiChannelNumber<Number, Size>;
    using Integer = std::conditional_t<std::is_same_v<Number, float>, uint32_t, uint64_t>;
    std::array<Vector, 10> values;
    ResponseFrequencyGeneric<Vector> frequency;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        Integer bits;
        for (auto& value : values)
        {
            std::cin >> bits;
            value[lane] = Bits<Number>(bits);
        }

        for (auto* value : {&frequency.m_cyclesPerSample, &frequency.m_sinHalf, &frequency.m_cosHalf,
                 &frequency.m_zInverse.m_real, &frequency.m_zInverse.m_imag})
        {
            std::cin >> bits;
            (*value)[lane] = Bits<Number>(bits);
        }
    }

    const auto results = VectorResponses<Number, Size>(values, frequency);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        if constexpr (std::is_same_v<Number, double>)
        {
            std::array<double, 10> scalarValues;
            for (size_t i = 0; i < values.size(); ++i)
            {
                scalarValues[i] = values[i][lane];
            }

            const ResponseFrequencyGeneric<double> scalarFrequency{frequency.m_cyclesPerSample[lane],
                frequency.m_sinHalf[lane], frequency.m_cosHalf[lane], frequency.m_zInverse[lane]};
            const auto scalar = ScalarResponses(scalarValues, scalarFrequency);
            for (size_t i = 0; i < results.size(); ++i)
            {
                assert(Bits<uint64_t>(scalar[i]) == Bits<uint64_t>(results[i][lane]));
            }
        }

        for (size_t i = 0; i < results.size(); ++i)
        {
            if (lane != 0 || i != 0)
            {
                std::cout << ' ';
            }

            std::cout << Bits<Integer>(results[i][lane]);
        }
    }

    std::cout << '\n';
}

template<typename Number>
void Dispatch(size_t width)
{
    switch (width)
    {
        case 1: Run<Number, 1>(); break;
        case 2: Run<Number, 2>(); break;
        case 3: Run<Number, 3>(); break;
        case 4: Run<Number, 4>(); break;
        default: assert(false);
    }
}

int main()
{
    CheckComplexReference();
    int precision;
    size_t width;
    while (std::cin >> precision >> width)
    {
        if (precision == 32)
        {
            Dispatch<float>(width);
        }
        else
        {
            assert(precision == 64);
            Dispatch<double>(width);
        }
    }
}

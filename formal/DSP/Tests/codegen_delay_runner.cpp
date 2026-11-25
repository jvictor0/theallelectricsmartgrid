#include "CircularBuffer.hpp"
#include "SchroederAllPassCoreGeneric.hpp"
#include "SquiggleBoyQuadReverb.hpp"
#include "TunedDecayCombCoreGeneric.hpp"
#include "TunedDecayCombTuningGeneric.hpp"
#include "TunedDecayCombResponseGeneric.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

template<typename Number>
Number Read()
{
    uint64_t raw;
    std::cin >> raw;
    using Bits = std::conditional_t<sizeof(Number) == 4, uint32_t, uint64_t>;
    Bits bits = static_cast<Bits>(raw);
    Number result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

template<typename Number>
void Print(Number value)
{
    using Bits = std::conditional_t<sizeof(Number) == 4, uint32_t, uint64_t>;
    Bits bits;
    std::memcpy(&bits, &value, sizeof(bits));
    std::cout << static_cast<uint64_t>(bits) << ' ';
}

template<typename Number, typename Coefficients>
void ReadCoefficients(Coefficients& c, size_t lane)
{
    c.m_delay[lane] = Read<Number>();
    c.m_a[lane] = Read<Number>();
    c.m_b[lane] = Read<Number>();
    c.m_r[lane] = Read<Number>();
    c.m_delayGain[lane] = Read<Number>();
    c.m_g[lane] = Read<Number>();
    c.m_k[lane] = Read<Number>();
    c.m_h[lane] = Read<Number>();
    c.m_gain[lane] = Read<Number>();
    c.m_sign[lane] = Read<Number>();
    c.m_bypass[lane] = Read<Number>();
}

template<typename Coefficients>
void PrintCoefficients(const Coefficients& c, size_t lane)
{
    for (auto value : {c.m_delay[lane], c.m_a[lane], c.m_b[lane], c.m_r[lane], c.m_delayGain[lane],
        c.m_g[lane], c.m_k[lane], c.m_h[lane], c.m_gain[lane], c.m_sign[lane], c.m_bypass[lane]})
    {
        Print(value);
    }
}

template<typename Number, size_t Size>
void RunAllPass()
{
    using Core = SchroederAllPassCoreGeneric<Number, Size>;
    static_assert(sizeof(typename Core::State) == sizeof(typename Core::Vector));
    Core core{};
    typename Core::Input input{};
    for (size_t lane = 0; lane < Size; ++lane)
    {
        core.m_state.m_gain[lane] = Read<Number>();
        // Old observations remain in the oracle input but are absent from runtime state.
        //
        static_cast<void>(Read<Number>());
        static_cast<void>(Read<Number>());
        input.m_x[lane] = Read<Number>();
        input.m_delayed[lane] = Read<Number>();
    }

    core.Process(input);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        Print(core.m_state.m_gain[lane]);
        Print(core.m_output.m_y[lane]);
        Print(core.m_output.m_written[lane]);
    }
}

template<size_t Size>
void RunComb(const std::string& kind)
{
    using Number = double;
    if (kind == "comb")
    {
        using Core = TunedDecayCombCoreGeneric<Number, Size>;
        static_assert(sizeof(typename Core::State) == 14 * sizeof(typename Core::Vector));
        Core core{};
        typename Core::Input input{};
        for (size_t lane = 0; lane < Size; ++lane)
        {
            ReadCoefficients<Number>(core.m_state.m_coefficients, lane);
            auto& s = core.m_state.m_memory;
            s.m_allpass[lane] = Read<Number>();
            s.m_ic1[lane] = Read<Number>();
            s.m_ic2[lane] = Read<Number>();
            // The rich model includes a previous output; the recurrence never reads it.
            //
            static_cast<void>(Read<Number>());
            input.m_x[lane] = Read<Number>();
            input.m_delayed[lane] = Read<Number>();
        }

        core.Process(input);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            const auto& s = core.m_state.m_memory;
            for (auto value : {s.m_allpass[lane], s.m_ic1[lane], s.m_ic2[lane], core.m_output.m_y[lane]})
            {
                Print(value);
            }
        }
    }
    else if (kind == "tuning")
    {
        using Core = TunedDecayCombTuningGeneric<Number, Size>;
        static_assert(std::is_empty_v<Core>);
        typename Core::Input input{};
        for (size_t lane = 0; lane < Size; ++lane)
        {
            input.m_frequency[lane] = Read<Number>();
            input.m_feedbackSign[lane] = Read<Number>();
            input.m_log2DecaySeconds[lane] = Read<Number>();
            input.m_dampOctaves[lane] = Read<Number>();
            input.m_sampleRate[lane] = Read<Number>();
        }

        const auto prepared = Core::Prepare(input);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            PrintCoefficients(prepared, lane);
        }
    }
    else if (kind == "response")
    {
        using Core = TunedDecayCombResponseGeneric<Number, Size>;
        Core core{};
        typename Core::Input input{};
        for (size_t lane = 0; lane < Size; ++lane)
        {
            ReadCoefficients<Number>(core.m_state.m_first, lane);
            ReadCoefficients<Number>(core.m_state.m_second, lane);
            core.m_state.m_mix[lane] = Read<Number>();
            input.m_cyclesPerSample[lane] = Read<Number>();
            input.m_sinHalf[lane] = Read<Number>();
            input.m_cosHalf[lane] = Read<Number>();
            input.m_zInverse.m_real[lane] = Read<Number>();
            input.m_zInverse.m_imag[lane] = Read<Number>();
        }

        core.Process(input);
        for (size_t lane = 0; lane < Size; ++lane)
        {
            Print(core.m_output.m_numerator.m_real[lane]);
            Print(core.m_output.m_numerator.m_imag[lane]);
            Print(core.m_output.m_denominator.m_real[lane]);
            Print(core.m_output.m_denominator.m_imag[lane]);
        }
    }
    else
    {
        throw std::runtime_error("Unknown delay kernel");
    }
}

template<typename Number, size_t Size>
void Run(const std::string& kind)
{
    if (kind == "allpass")
    {
        RunAllPass<Number, Size>();
    }
    else if constexpr (std::is_same_v<Number, double>)
    {
        RunComb<Size>(kind);
    }
    else
    {
        throw std::runtime_error("Comb tests require double precision");
    }

    std::cout << '\n';
}

template<bool Framed>
void CheckReverbClocks()
{
    using Core = SquiggleBoyQuadReverb<float, Framed>;
    using Vector = typename Core::Vector;
    using Clock = WanderingRandomLFOGeneric<float, Framed>;
    const auto silence = Vector::Splat(0);
    const typename Core::LogicalSpec spec{
        Vector::Splat(32), silence, Vector::Splat(0.125f),
        Vector::Splat(2), Vector::Splat(1)};
    const auto core = std::make_unique<Core>(spec);
    std::array<Clock, 4> first;
    std::array<Clock, 4> second;
    for (size_t lane = 0; lane < first.size(); ++lane)
    {
        first[lane].Initialize(1920000, 9 + 16 * lane);
        second[lane].Initialize(1920000, 10 + 16 * lane);
    }

    // Read copies so only the reverb kernel advances the clocks under test.
    //
    const auto checkClocks = [](auto clock, auto& expected)
    {
        const std::array actual{
            clock.m_lane0.Process(), clock.m_lane1.Process(),
            clock.m_lane2.Process(), clock.m_lane3.Process()};
        for (size_t lane = 0; lane < actual.size(); ++lane)
        {
            assert(actual[lane] == expected[lane].Process());
        }
    };

    for (size_t sample = 0; sample < 96; ++sample)
    {
        checkClocks(core->m_state.m_first.m_clock, first);
        checkClocks(core->m_state.m_second.m_clock, second);
        core->Process(silence);
    }
}

template<typename Number>
void RunReverb()
{
    using Core = SquiggleBoyQuadReverb<Number>;
    static std::unique_ptr<Core> state;
    const auto readVector = []()
    {
        typename Core::Vector values;
        for (int ch = 0; ch < 4; ++ch)
        {
            values[ch] = Read<Number>();
        }

        return values;
    };
    const typename Core::LogicalSpec spec{readVector(), readVector(), readVector(), readVector(), readVector()};
    const auto audio = readVector();
    if (!state)
    {
        state = std::make_unique<Core>(spec);
    }
    else
    {
        state->ComputeCoefficients(spec);
    }

    const auto output = state->Process(audio);
    for (int ch = 0; ch < 4; ++ch)
    {
        Print(output[ch]);
    }

    std::cout << '\n';
}

template<size_t Capacity, size_t Size>
void Ring(size_t age, double value)
{
    static CircularBufferGeneric<double, Size, Capacity> ring;
    using Vector = typename CircularBufferGeneric<double, Size, Capacity>::Vector;
    MultiChannelNumber<size_t, Size> taps;
    Vector input;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        taps[lane] = age + 1;
        input[lane] = value + static_cast<double>(lane);
    }

    auto output = ring.ReadDelays(taps);
    ring.Push(input);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        Print(output[lane]);
    }

    std::cout << ring.m_head << '\n';
}

template<size_t Size>
void RingCapacity(size_t capacity, size_t age, double value)
{
    switch (capacity)
    {
    case 1: Ring<1, Size>(age, value); break;
    case 3: Ring<3, Size>(age, value); break;
    case 8: Ring<8, Size>(age, value); break;
    case 4096: Ring<4096, Size>(age, value); break;
    case 16384: Ring<16384, Size>(age, value); break;
    default: throw std::runtime_error("Unknown ring capacity");
    }
}

template<size_t Capacity, size_t Size>
void Address(size_t head, size_t age)
{
    CircularBufferGeneric<double, Size, Capacity> ring;
    for (size_t slot = 0; slot < Capacity; ++slot)
    {
        for (size_t lane = 0; lane < Size; ++lane)
        {
            ring.m_data[slot][lane] = static_cast<double>(slot + lane);
        }
    }

    ring.m_head = head;
    auto output = ring.ReadAge(age);
    MultiChannelNumber<size_t, Size> ages;
    MultiChannelNumber<size_t, Size> indices;
    for (size_t lane = 0; lane < Size; ++lane)
    {
        ages[lane] = (age + lane) % Capacity;
        indices[lane] = ring.Address(ages[lane]);
    }

    const auto diagonal = ring.m_data[indices];
    const std::array<std::uint64_t, 4> offsets{0, 1, 3, Capacity + 2};
    const auto gathered = ring.ReadAgeOffsets(ages, offsets);
    for (size_t lane = 0; lane < Size; ++lane)
    {
        assert(diagonal[lane] == ring.ReadAge(ages[lane])[lane]);
        for (size_t tap = 0; tap < offsets.size(); ++tap)
        {
            const auto expectedAge = (ages[lane] + Capacity - offsets[tap] % Capacity) % Capacity;
            assert(gathered[tap][lane] == ring.ReadAge(expectedAge)[lane]);
        }
    }

    ring.Push(MultiChannelNumber<double, Size>::Splat(0));
    for (size_t lane = 0; lane < Size; ++lane)
    {
        Print(output[lane]);
    }

    std::cout << ring.m_head << '\n';
}

template<size_t Size>
void AddressCapacity(size_t capacity, size_t head, size_t age)
{
    switch (capacity)
    {
    case 1: Address<1, Size>(head, age); break;
    case 2: Address<2, Size>(head, age); break;
    case 3: Address<3, Size>(head, age); break;
    case 4: Address<4, Size>(head, age); break;
    case 7: Address<7, Size>(head, age); break;
    case 8: Address<8, Size>(head, age); break;
    case 16: Address<16, Size>(head, age); break;
    default: throw std::runtime_error("Unknown address capacity");
    }
}

template<typename Stream>
void RunSpline()
{
    static Stream stream(6, 11);
    Print(stream.Process());
    std::cout << '\n';
}

int main()
{
    CheckReverbClocks<false>();
    CheckReverbClocks<true>();
    std::string kind;
    size_t precision, width;
    while (std::cin >> kind >> precision >> width)
    {
        if (kind == "reverb" && precision == 32 && width == 4)
        {
            RunReverb<float>();
        }
        else if (kind == "reverb" && precision == 64 && width == 4)
        {
            RunReverb<double>();
        }
        else if (kind == "spline" && precision == 32 && width == 1)
        {
            RunSpline<WanderingRandomLFOGeneric<float, false>>();
        }
        else if (kind == "spline" && precision == 64 && width == 1)
        {
            RunSpline<WanderingRandomLFOGeneric<double, false>>();
        }
        else if (kind == "spline_framed" && precision == 32 && width == 1)
        {
            RunSpline<WanderingRandomLFOGeneric<float, true>>();
        }
        else if (kind == "spline_framed" && precision == 64 && width == 1)
        {
            RunSpline<WanderingRandomLFOGeneric<double, true>>();
        }
        else if (kind == "address")
        {
            size_t capacity, head, age;
            std::cin >> capacity >> head >> age;
            if (width == 2)
            {
                AddressCapacity<2>(capacity, head, age);
            }
            else if (width == 4)
            {
                AddressCapacity<4>(capacity, head, age);
            }
            else
            {
                throw std::runtime_error("Unsupported address width");
            }
        }
        else if (kind == "ring")
        {
            size_t capacity, age;
            std::cin >> capacity >> age;
            double value = Read<double>();
            if (width == 2)
            {
                RingCapacity<2>(capacity, age, value);
            }
            else if (width == 4)
            {
                RingCapacity<4>(capacity, age, value);
            }
            else
            {
                throw std::runtime_error("Unsupported ring width");
            }
        }
        else if (precision == 32 && width == 2)
        {
            Run<float, 2>(kind);
        }
        else if (precision == 32 && width == 4)
        {
            Run<float, 4>(kind);
        }
        else if (precision == 64 && width == 2)
        {
            Run<double, 2>(kind);
        }
        else if (precision == 64 && width == 4)
        {
            Run<double, 4>(kind);
        }
        else
        {
            throw std::runtime_error("Unsupported numeric format");
        }
    }
}

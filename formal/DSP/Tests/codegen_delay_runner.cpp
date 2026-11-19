#include "CircularBufferGeneric.hpp"
#include "SchroederAllPassCoreGeneric.hpp"
#include "TunedDecayCombCoreGeneric.hpp"
#include "TunedDecayCombTuningGeneric.hpp"
#include "TunedDecayCombResponseGeneric.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
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
void Run(const std::string& kind)
{
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
    else if (kind == "allpass")
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

int main()
{
    std::string kind;
    size_t precision, width;
    while (std::cin >> kind >> precision >> width)
    {
        if (kind == "address")
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

#include "OnePoleZOHLowPass.hpp"
#include "OnePoleTPTLowPass.hpp"
#include "OnePoleTPTHighPass.hpp"
#include "TPTBaseWidth.hpp"
#include "TPTSVF.hpp"
#include "MorphingTPTSVF.hpp"
#include "TPTSVF4PoleHighPass.hpp"
#include "TPTSVF4PoleLowPass.hpp"
#include "TPTSVF4PoleBandPass.hpp"
#include "TPTLadder.hpp"
#include "SwapProcessor.hpp"
#include "GainProcessor.hpp"
#include "FMAProcessor.hpp"
#include "FMAProcessor32.hpp"
#include "CoupledProcessor.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

template<typename Value, typename Bits>
Value FromBits(Bits bits)
{
    static_assert(sizeof(Value) == sizeof(Bits));
    Value value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

template<typename Bits, typename Value>
Bits ToBits(Value value)
{
    static_assert(sizeof(Value) == sizeof(Bits));
    Bits bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// State-only processors whose states are Count doubles in declaration order:
// read the state and input, process once, and print the next state.
//
template<typename Processor, size_t Count>
bool RunFlat()
{
    static_assert(sizeof(typename Processor::State) == Count * sizeof(double));
    std::array<double, Count> values;
    for (double& value : values)
    {
        uint64_t bits;
        if (!(std::cin >> bits))
        {
            return false;
        }

        value = FromBits<double>(bits);
    }

    uint64_t x;
    if (!(std::cin >> x))
    {
        return false;
    }

    Processor processor{};
    std::memcpy(&processor.m_state, values.data(), sizeof(values));
    typename Processor::Input input{FromBits<double>(x)};
    processor.Process(input);
    std::memcpy(values.data(), &processor.m_state, sizeof(values));
    for (size_t i = 0; i < Count; ++i)
    {
        std::cout << (i == 0 ? "" : " ") << ToBits<uint64_t>(values[i]);
    }

    std::cout << '\n';
    return true;
}

bool RunFlatKind(const std::string& kind, bool& known)
{
    known = true;
    if (kind == "zoh")
    {
        return RunFlat<OnePoleZOHLowPass, 2>();
    }

    if (kind == "tpt")
    {
        return RunFlat<OnePoleTPTLowPass, 3>();
    }

    if (kind == "tptHigh")
    {
        return RunFlat<OnePoleTPTHighPass, 4>();
    }

    if (kind == "baseWidth")
    {
        return RunFlat<TPTBaseWidth, 7>();
    }

    if (kind == "svf")
    {
        return RunFlat<TPTSVF, 11>();
    }

    if (kind == "morph")
    {
        return RunFlat<MorphingTPTSVF, 15>();
    }

    if (kind == "svf4High")
    {
        return RunFlat<TPTSVF4PoleHighPass, 22>();
    }

    if (kind == "svf4Low")
    {
        return RunFlat<TPTSVF4PoleLowPass, 22>();
    }

    if (kind == "svf4Band")
    {
        return RunFlat<TPTSVF4PoleBandPass, 22>();
    }

    if (kind == "ladder")
    {
        return RunFlat<TPTLadder, 14>();
    }

    known = false;
    return true;
}

template<typename Processor, typename Number, typename Bits>
bool RunFMA()
{
    Bits a, c, b;
    if (!(std::cin >> a >> c >> b))
    {
        return false;
    }

    Processor processor{};
    processor.m_state = {FromBits<Number>(a), FromBits<Number>(c)};
    typename Processor::Input input{FromBits<Number>(b)};
    processor.Process(input);
    std::cout << ToBits<Bits>(processor.m_state.m_left) << ' '
              << ToBits<Bits>(processor.m_state.m_right) << ' '
              << ToBits<Bits>(processor.m_output.m_x) << '\n';
    return true;
}

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        bool known;
        if (!RunFlatKind(kind, known))
        {
            return 1;
        }

        if (known)
        {
            continue;
        }

        if (kind == "fma" || kind == "fma32")
        {
            const bool success = kind == "fma" ? RunFMA<FMAProcessor, double, uint64_t>()
                : RunFMA<FMAProcessor32, float, uint32_t>();
            if (!success)
            {
                return 1;
            }
        }
        else if (kind == "swap")
        {
            uint64_t left, right, x;
            unsigned reset;
            if (!(std::cin >> left >> right >> x >> reset))
            {
                return 1;
            }

            SwapProcessor processor{{FromBits<double>(left), FromBits<double>(right)}, {0.0, 0.0}};
            SwapProcessor::Input input{FromBits<double>(x), reset != 0};
            processor.Process(input);
            std::cout << ToBits<uint64_t>(processor.m_state.m_left) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_right) << ' '
                      << ToBits<uint64_t>(processor.m_output.m_oldLeft) << ' '
                      << ToBits<uint64_t>(processor.m_output.m_sum) << '\n';
        }
        else if (kind == "gain")
        {
            uint32_t gain, x;
            if (!(std::cin >> gain >> x))
            {
                return 1;
            }

            GainProcessor processor{{FromBits<float>(gain)}, {0.0f, false}};
            GainProcessor::Input input{FromBits<float>(x)};
            processor.Process(input);
            std::cout << ToBits<uint32_t>(processor.m_state.m_gain) << ' '
                      << ToBits<uint32_t>(processor.m_output.m_y) << ' '
                      << static_cast<unsigned>(processor.m_output.m_same) << '\n';
        }
        else if (kind == "coupled")
        {
            uint64_t first, second, x;
            if (!(std::cin >> first >> second >> x))
            {
                return 1;
            }

            CoupledProcessor processor{};
            processor.m_state.m_model.m_A = {{{0.75, -0.125}, {0.25, 0.5}}};
            processor.m_state.m_model.m_B = {0.5, -0.25};
            processor.m_state.m_model.m_C = {0.75, -0.5};
            processor.m_state.m_model.m_D = 0.125;
            processor.m_state.m_q = {FromBits<double>(first), FromBits<double>(second)};
            CoupledProcessor::Input input{FromBits<double>(x)};
            processor.Process(input);
            std::cout << ToBits<uint64_t>(processor.m_state.m_model.m_A[0][0]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_A[0][1]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_A[1][0]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_A[1][1]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_B[0]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_B[1]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_C[0]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_C[1]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_model.m_D) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_q[0]) << ' '
                      << ToBits<uint64_t>(processor.m_state.m_q[1]) << ' '
                      << ToBits<uint64_t>(processor.m_output.m_y) << '\n';
        }
        else
        {
            std::cerr << "Unknown processor: " << kind << '\n';
            return 1;
        }
    }
}

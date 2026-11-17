#include "OnePoleZOHLowPassGeneric.hpp"
#include "OnePoleTPTLowPassGeneric.hpp"
#include "OnePoleTPTHighPassGeneric.hpp"
#include "TPTBaseWidthGeneric.hpp"
#include "TPTSVFGeneric.hpp"
#include "MorphingTPTSVFGeneric.hpp"
#include "TPTSVF4PoleHighPassGeneric.hpp"
#include "TPTSVF4PoleLowPassGeneric.hpp"
#include "TPTSVF4PoleBandPassGeneric.hpp"
#include "TPTLadderGeneric.hpp"
#include "LinkwitzRileyCrossoverGeneric.hpp"
#include "CoupledProcessorGeneric.hpp"
#include "ArithmeticProcessorGeneric.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>

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

template<typename State>
auto LowPassFields(State& state)
{
    return std::tie(state.m_G, state.m_s, state.m_y);
}

template<typename State>
auto SVFFields(State& state)
{
    return std::tie(state.m_k, state.m_a1, state.m_a2, state.m_a3,
        state.m_ic1, state.m_ic2, state.m_lowPass, state.m_bandPass,
        state.m_highPass, state.m_notch);
}

template<typename Number, size_t Size>
auto StateFields(OnePoleZOHLowPassGeneric<Number, Size>& processor)
{
    return std::tie(processor.m_state.m_alpha, processor.m_state.m_y);
}

template<typename Number, size_t Size>
auto StateFields(OnePoleTPTLowPassGeneric<Number, Size>& processor)
{
    return LowPassFields(processor.m_state);
}

template<typename Number, size_t Size>
auto StateFields(OnePoleTPTHighPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(LowPassFields(processor.m_state.m_lowPass), std::tie(processor.m_state.m_y));
}

template<typename Number, size_t Size>
auto StateFields(TPTBaseWidthGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    return std::tuple_cat(LowPassFields(state.m_highPass.m_lowPass),
        std::tie(state.m_highPass.m_y), LowPassFields(state.m_lowPass));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVFGeneric<Number, Size>& processor)
{
    return SVFFields(processor.m_state);
}

template<typename Number, size_t Size>
auto StateFields(MorphingTPTSVFGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    return std::tuple_cat(SVFFields(state.m_svf),
        std::tie(state.m_cosine, state.m_sine, state.m_blend, state.m_output));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVF4PoleHighPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state.m_stage1), SVFFields(processor.m_state.m_stage2));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVF4PoleLowPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state.m_stage1), SVFFields(processor.m_state.m_stage2));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVF4PoleBandPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state.m_stage1), SVFFields(processor.m_state.m_stage2));
}

template<typename Number, size_t Size>
auto StateFields(TPTLadderGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    return std::tuple_cat(std::tie(state.m_k, state.m_drive), LowPassFields(state.m_stage1),
        LowPassFields(state.m_stage2), LowPassFields(state.m_stage3), LowPassFields(state.m_stage4));
}

template<typename Number, size_t Size>
auto StateFields(LinkwitzRileyCrossoverGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    return std::tuple_cat(SVFFields(state.m_first), SVFFields(state.m_lowSecond), SVFFields(state.m_highSecond));
}

template<typename Number, size_t Size>
auto StateFields(CoupledProcessorGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    auto& model = state.m_model;
    return std::tie(model.m_A[0][0], model.m_A[0][1], model.m_A[1][0], model.m_A[1][1],
        model.m_B[0], model.m_B[1], model.m_C[0], model.m_C[1], model.m_D, state.m_q[0], state.m_q[1]);
}

template<typename Number, size_t Size>
auto StateFields(ArithmeticProcessorGeneric<Number, Size>& processor)
{
    return std::tie(processor.m_state.m_left, processor.m_state.m_right);
}

template<typename Processor>
auto ResultFields(Processor& processor)
{
    return StateFields(processor);
}

template<typename Number, size_t Size>
auto ResultFields(CoupledProcessorGeneric<Number, Size>& processor)
{
    return std::tuple_cat(StateFields(processor), std::tie(processor.m_output.m_y));
}

template<typename Number, size_t Size>
auto ResultFields(ArithmeticProcessorGeneric<Number, Size>& processor)
{
    return std::tuple_cat(StateFields(processor), std::tie(processor.m_output.m_x));
}

// Load each logical field/channel explicitly. Keep the generated processor's
// state across the whole trajectory; the three-channel padding is not a field.
//
template<typename Processor, typename Number, size_t Size>
bool Run(size_t steps)
{
    using Bits = std::conditional_t<std::is_same_v<Number, float>, uint32_t, uint64_t>;
    Processor processor{};
    auto fields = StateFields(processor);
    for (size_t channel = 0; channel < Size; ++channel)
    {
        auto read = [&](auto& value)
        {
            Bits bits = 0;
            std::cin >> bits;
            value[channel] = FromBits<Number>(bits);
            if constexpr (Size == 3)
            {
                value.m_values[3] = std::numeric_limits<Number>::quiet_NaN();
            }
        };

        std::apply([&](auto&... values)
        {
            (read(values), ...);
        }, fields);
    }

    for (size_t step = 0; step < steps; ++step)
    {
        typename Processor::Input input{};
        for (size_t channel = 0; channel < Size; ++channel)
        {
            Bits bits = 0;
            std::cin >> bits;
            input.m_x[channel] = FromBits<Number>(bits);
        }

        if (!std::cin)
        {
            return false;
        }

        processor.Process(input);
        bool first = true;
        for (size_t channel = 0; channel < Size; ++channel)
        {
            auto print = [&](auto& value)
            {
                std::cout << (first ? "" : " ") << ToBits<Bits>(value[channel]);
                first = false;
            };

            std::apply([&](auto&... values)
            {
                (print(values), ...);
            }, ResultFields(processor));
        }

        std::cout << '\n';
    }

    return true;
}

template<typename Number, size_t Size>
bool RunKind(const std::string& kind, size_t steps)
{
    if (kind == "arithmetic")
    {
        return Run<ArithmeticProcessorGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "zoh")
    {
        return Run<OnePoleZOHLowPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "tpt")
    {
        return Run<OnePoleTPTLowPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "tptHigh")
    {
        return Run<OnePoleTPTHighPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "baseWidth")
    {
        return Run<TPTBaseWidthGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svf")
    {
        return Run<TPTSVFGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "morph")
    {
        return Run<MorphingTPTSVFGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svf4High")
    {
        return Run<TPTSVF4PoleHighPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svf4Low")
    {
        return Run<TPTSVF4PoleLowPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svf4Band")
    {
        return Run<TPTSVF4PoleBandPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "ladder")
    {
        return Run<TPTLadderGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "crossover")
    {
        return Run<LinkwitzRileyCrossoverGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "coupled")
    {
        return Run<CoupledProcessorGeneric<Number, Size>, Number, Size>(steps);
    }

    return false;
}

template<typename Number>
bool RunWidth(const std::string& kind, size_t width, size_t steps)
{
    switch (width)
    {
        case 1: return RunKind<Number, 1>(kind, steps);
        case 2: return RunKind<Number, 2>(kind, steps);
        case 3: return RunKind<Number, 3>(kind, steps);
        case 4: return RunKind<Number, 4>(kind, steps);
        default: return false;
    }
}

int main()
{
    std::string kind;
    unsigned precision;
    size_t width;
    size_t steps;
    while (std::cin >> kind >> precision >> width >> steps)
    {
        bool success = precision == 32 ? RunWidth<float>(kind, width, steps)
            : precision == 64 && RunWidth<double>(kind, width, steps);
        if (!success)
        {
            std::cerr << "Invalid channel test: " << kind << '\n';
            return 1;
        }
    }
}

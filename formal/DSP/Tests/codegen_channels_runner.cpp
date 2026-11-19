#include "TPTSVFLowPassGeneric.hpp"
#include "TPTSVFHighPassGeneric.hpp"
#include "TPTSVFBandPassGeneric.hpp"
#include "TPTSVFNotchGeneric.hpp"
#include "TPTSVFAllPassGeneric.hpp"
#include "OnePoleZOHLowPassGeneric.hpp"
#include "OnePoleZOHLowPass.hpp"
#include "OnePoleZOHLowPass32.hpp"
#include "Filter.hpp"
#include "FrequencyProbeGeneric.hpp"
#include "FrequencyProbe.hpp"
#include "FrequencyProbe32.hpp"
#include "ConstantFunctionGeneric.hpp"
#include "SnapshotFunctionGeneric.hpp"
#include "OnePoleTPTLowPassGeneric.hpp"
#include "OnePoleTPTHighPassGeneric.hpp"
#include "TPTBaseWidthGeneric.hpp"
#include "TPTSVFGeneric.hpp"
#include "LinkwitzRileySplitter4BandGeneric.hpp"
#include "MorphingTPTSVFGeneric.hpp"
#include "TPTSVF4PoleHighPassGeneric.hpp"
#include "TPTSVF4PoleLowPassGeneric.hpp"
#include "TPTSVF4PoleBandPassGeneric.hpp"
#include "TPTLadderGeneric.hpp"
#include "ConservativeReuppingLadderCoreGeneric.hpp"
#include "CoupledProcessorGeneric.hpp"
#include "ArithmeticProcessorGeneric.hpp"
#include "FMAProcessorGeneric.hpp"

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
    return std::tie(state.m_G, state.m_s);
}

template<typename State>
auto SVFFields(State& state)
{
    return std::tie(state.m_k, state.m_a1, state.m_a2, state.m_a3,
        state.m_ic1, state.m_ic2);
}

template<typename Number, size_t Size>
auto StateFields(OnePoleZOHLowPassGeneric<Number, Size>& processor)
{
    return std::tie(processor.m_state.m_alpha, processor.m_state.m_y);
}

template<typename Number, size_t Size>
auto StateFields(OnePoleTPTLowPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(LowPassFields(processor.m_state), std::tie(processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(OnePoleTPTHighPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(LowPassFields(processor.m_state.m_lowPass), std::tie(processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(TPTBaseWidthGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    return std::tuple_cat(LowPassFields(state.m_highPass.m_lowPass),
        LowPassFields(state.m_lowPass), std::tie(processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVFGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state), std::tie(processor.m_output.m_lowPass,
        processor.m_output.m_bandPass, processor.m_output.m_highPass, processor.m_output.m_notch, processor.m_output.m_allPass));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVFLowPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state), std::tie(processor.m_output.m_lowPass));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVFHighPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state), std::tie(processor.m_output.m_highPass));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVFBandPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state), std::tie(processor.m_output.m_bandPass));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVFNotchGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state), std::tie(processor.m_output.m_notch));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVFAllPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state), std::tie(processor.m_output.m_allPass));
}

template<typename Number, size_t Size>
auto StateFields(MorphingTPTSVFGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    return std::tuple_cat(SVFFields(state.m_svf),
        std::tie(state.m_cosine, state.m_sine, state.m_blend, processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVF4PoleHighPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state.m_stage1), SVFFields(processor.m_state.m_stage2), std::tie(processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVF4PoleLowPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state.m_stage1), SVFFields(processor.m_state.m_stage2), std::tie(processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(TPTSVF4PoleBandPassGeneric<Number, Size>& processor)
{
    return std::tuple_cat(SVFFields(processor.m_state.m_stage1), SVFFields(processor.m_state.m_stage2), std::tie(processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(TPTLadderGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    return std::tuple_cat(std::tie(state.m_k), LowPassFields(state.m_stage1),
        LowPassFields(state.m_stage2), LowPassFields(state.m_stage3), LowPassFields(state.m_stage4), std::tie(processor.m_output.m_x));
}

template<typename Number, size_t Size>
auto StateFields(ConservativeReuppingLadderCoreGeneric<Number, Size>& processor)
{
    auto& s = processor.m_state;
    return std::tie(s.m_alpha, s.m_saturation.m_gain, s.m_saturation.m_tanhGain,
        s.m_k, s.m_stage1, s.m_stage2, s.m_stage3, s.m_stage4, processor.m_output.m_x);
}

template<typename State>
auto SVFMemory(State& state)
{
    return std::tie(state.m_k, state.m_a1, state.m_a2, state.m_a3, state.m_ic1, state.m_ic2);
}

template<typename State>
auto PhaseFields(State& state)
{
    return SVFMemory(state.m_svf);
}

template<typename State>
auto TwoBandFields(State& state)
{
    return std::tuple_cat(SVFMemory(state.m_first), SVFMemory(state.m_second));
}

template<typename Number, size_t Size>
auto StateFields(LinkwitzRileySplitter4BandGeneric<Number, Size>& processor)
{
    auto& state = processor.m_state;
    auto& tree = state.m_tree;
    return std::tuple_cat(SVFMemory(tree.m_first), SVFMemory(tree.m_second),
        PhaseFields(tree.m_lowPhase), PhaseFields(tree.m_highPhase),
        TwoBandFields(tree.m_low), TwoBandFields(tree.m_high),
        std::tie(processor.m_output.m_bands[0], processor.m_output.m_bands[1], processor.m_output.m_bands[2], processor.m_output.m_bands[3]));
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

template<typename Number, size_t Size>
auto StateFields(FMAProcessorGeneric<Number, Size>& processor)
{
    return std::tie(processor.m_state.m_left, processor.m_state.m_right);
}

template<typename Number, size_t Size>
auto ResultFields(FMAProcessorGeneric<Number, Size>& processor)
{
    return std::tuple_cat(StateFields(processor), std::tie(processor.m_output.m_x));
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

// The pure-function path loads a fresh snapshot and prepared frequency for
// each call. Compare scalar C++ too, and poison the unused SIMD padding.
//
template<typename Number, size_t Size>
bool RunResponse(size_t calls)
{
    using Response = OnePoleZOHLowPassGeneric<Number, Size>;
    using ScalarResponse = std::conditional_t<std::is_same_v<Number, float>,
        OnePoleZOHLowPass32, OnePoleZOHLowPass>;
    using Bits = std::conditional_t<std::is_same_v<Number, float>, uint32_t, uint64_t>;
    using Constant = ConstantFunctionGeneric<Number, Size>;
    using SnapshotOnly = SnapshotFunctionGeneric<Number, Size>;
    using Probe = FrequencyProbeGeneric<Number, Size>;
    using ScalarProbe = std::conditional_t<std::is_same_v<Number, float>, FrequencyProbe32, FrequencyProbe>;
    static_assert(std::is_same_v<typename Response::Frequency, typename Probe::Input>);
    static_assert(std::is_same_v<typename ScalarResponse::Frequency, typename ScalarProbe::Input>);
    if constexpr (std::is_same_v<Number, double>)
    {
        static_assert(std::is_same_v<typename Response::Frequency,
            typename OPLowPassFilterGeneric<float, Size>::Snapshot::Frequency>);
    }
    for (size_t call = 0; call < calls; ++call)
    {
        typename Response::Snapshot snapshot{};
        typename Response::Frequency frequency{};
        auto frequencyFields = std::tie(frequency.m_cyclesPerSample, frequency.m_sinHalf,
            frequency.m_cosHalf, frequency.m_zInverse.m_real, frequency.m_zInverse.m_imag);
        for (size_t channel = 0; channel < Size; ++channel)
        {
            Bits alphaBits = 0;
            std::cin >> alphaBits;
            snapshot.m_alpha[channel] = FromBits<Number>(alphaBits);
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
            }, frequencyFields);
        }

        if (!std::cin)
        {
            return false;
        }

        if constexpr (Size == 3)
        {
            snapshot.m_alpha.m_values[3] = std::numeric_limits<Number>::quiet_NaN();
        }

        const auto result = Response::MagnitudeSquared(snapshot, frequency);
        const auto probe = Probe::Evaluate({snapshot.m_alpha, -snapshot.m_alpha}, frequency);
        const auto constant = Constant::Evaluate({}, frequency.m_sinHalf);
        const auto snapshotOnly = SnapshotOnly::Evaluate({snapshot.m_alpha}, frequency.m_sinHalf);
        for (size_t channel = 0; channel < Size; ++channel)
        {
            const typename ScalarResponse::Frequency scalarFrequency{frequency.m_cyclesPerSample[channel],
                frequency.m_sinHalf[channel], frequency.m_cosHalf[channel], {frequency.m_zInverse.m_real[channel],
                frequency.m_zInverse.m_imag[channel]}};
            const auto scalar = ScalarResponse::MagnitudeSquared({snapshot.m_alpha[channel]}, scalarFrequency);
            const auto scalarProbe = ScalarProbe::Evaluate(
                {snapshot.m_alpha[channel], -snapshot.m_alpha[channel]}, scalarFrequency);
            if (ToBits<Bits>(result[channel]) != ToBits<Bits>(scalar) ||
                ToBits<Bits>(probe[channel]) != ToBits<Bits>(scalarProbe) ||
                constant[channel] != static_cast<Number>(1) ||
                ToBits<Bits>(snapshotOnly[channel]) != ToBits<Bits>(snapshot.m_alpha[channel]))
            {
                return false;
            }

            std::cout << (channel == 0 ? "" : " ") << ToBits<Bits>(result[channel])
                << ' ' << ToBits<Bits>(probe[channel]);
        }

        std::cout << '\n';
    }

    return true;
}

template<typename Number, size_t Size>
bool RunKind(const std::string& kind, size_t steps)
{
    if (kind == "zohResponse")
    {
        return RunResponse<Number, Size>(steps);
    }

    if (kind == "fma")
    {
        return Run<FMAProcessorGeneric<Number, Size>, Number, Size>(steps);
    }

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

    if (kind == "svfLowPass")
    {
        return Run<TPTSVFLowPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svfHighPass")
    {
        return Run<TPTSVFHighPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svfBandPass")
    {
        return Run<TPTSVFBandPassGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svfNotch")
    {
        return Run<TPTSVFNotchGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "svfAllPass")
    {
        return Run<TPTSVFAllPassGeneric<Number, Size>, Number, Size>(steps);
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

    if (kind == "splitter")
    {
        return Run<LinkwitzRileySplitter4BandGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "ladder")
    {
        return Run<TPTLadderGeneric<Number, Size>, Number, Size>(steps);
    }

    if (kind == "conservative")
    {
        return Run<ConservativeReuppingLadderCoreGeneric<Number, Size>, Number, Size>(steps);
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

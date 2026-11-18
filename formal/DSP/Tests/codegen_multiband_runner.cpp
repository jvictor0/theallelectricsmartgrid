#include "MultibandSaturator4BandCoreGeneric.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
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
auto SVFFields(State& state)
{
    return std::tie(state.m_k, state.m_a1, state.m_a2, state.m_a3,
        state.m_ic1, state.m_ic2, state.m_lowPass, state.m_bandPass,
        state.m_highPass, state.m_notch, state.m_allPass);
}

template<typename State>
auto PhaseFields(State& state)
{
    return std::tuple_cat(SVFFields(state.m_svf), std::tie(state.m_low.m_x, state.m_high.m_x));
}

template<typename State>
auto TwoBandFields(State& state)
{
    return std::tuple_cat(SVFFields(state.m_first), SVFFields(state.m_second),
        std::tie(state.m_lowPhase.m_x, state.m_highPhase.m_x, state.m_low.m_x, state.m_high.m_x));
}

template<typename Core>
auto StateFields(Core& core)
{
    auto& state = core.m_state;
    auto& split = state.m_splitter;
    auto& tree = split.m_tree;
    return std::tuple_cat(SVFFields(tree.m_first), SVFFields(tree.m_second),
        PhaseFields(tree.m_lowPhase), PhaseFields(tree.m_highPhase),
        TwoBandFields(tree.m_low), TwoBandFields(tree.m_high),
        std::tie(split.m_bands[0], split.m_bands[1], split.m_bands[2], split.m_bands[3],
            state.m_bandGain[0], state.m_bandGain[1], state.m_bandGain[2], state.m_bandGain[3], state.m_masterGain));
}

template<typename Core>
auto OutputFields(Core& core)
{
    auto& o = core.m_output;
    return std::tie(o.m_bands[0].m_before, o.m_bands[0].m_after,
        o.m_bands[1].m_before, o.m_bands[1].m_after,
        o.m_bands[2].m_before, o.m_bands[2].m_after,
        o.m_bands[3].m_before, o.m_bands[3].m_after,
        o.m_master.m_before, o.m_master.m_after, o.m_sub);
}

template<typename Core>
auto SnapshotFields(Core& core)
{
    auto& s = core.m_responseSnapshot;
    return std::tie(s.m_splitter.m_tree.m_value.m_g, s.m_splitter.m_tree.m_value.m_k,
        s.m_splitter.m_tree.m_low.m_value.m_g, s.m_splitter.m_tree.m_low.m_value.m_k,
        s.m_splitter.m_tree.m_high.m_value.m_g, s.m_splitter.m_tree.m_high.m_value.m_k,
        s.m_bandGain[0], s.m_bandGain[1], s.m_bandGain[2], s.m_bandGain[3], s.m_masterGain);
}

template<typename Number, size_t Width>
void Run(size_t steps)
{
    using Core = MultibandSaturator4BandCoreGeneric<Number, Width>;
    using Bits = std::conditional_t<sizeof(Number) == 4, uint32_t, uint64_t>;
    Core core{};
    auto state = StateFields(core);
    for (size_t lane = 0; lane < Width; ++lane)
    {
        std::apply([&](auto&... fields)
        {
            auto read = [&](auto& value)
            {
                uint64_t bits;
                std::cin >> bits;
                value[lane] = FromBits<Number>(static_cast<Bits>(bits));
            };
            (read(fields), ...);
        }, state);
    }

    for (size_t lane = 0; lane < Width; ++lane)
    {
        std::apply([&](auto&... fields)
        {
            auto read = [&](auto& value)
            {
                uint64_t bits;
                std::cin >> bits;
                value[lane] = FromBits<Number>(static_cast<Bits>(bits));
            };
            (read(fields), ...);
        }, SnapshotFields(core));
    }

    for (size_t k = 0; k < steps; ++k)
    {
        typename Core::Input input;
        int mono;
        std::cin >> mono;
        input.m_monoBass = mono != 0;
        for (size_t lane = 0; lane < Width; ++lane)
        {
            uint64_t bits;
            std::cin >> bits;
            input.m_x[lane] = FromBits<Number>(static_cast<Bits>(bits));
        }

        typename Core::Frequency frequency{};
        for (size_t lane = 0; lane < Width; ++lane)
        {
            uint64_t f, sine, cosine;
            std::cin >> f >> sine >> cosine;
            frequency.m_cyclesPerSample[lane] = FromBits<Number>(static_cast<Bits>(f));
            frequency.m_sinHalf[lane] = FromBits<Number>(static_cast<Bits>(sine));
            frequency.m_cosHalf[lane] = FromBits<Number>(static_cast<Bits>(cosine));
        }

        auto power = Core::MagnitudeSquared(core.m_responseSnapshot, frequency);
        core.Process(input);
        bool first = true;
        for (size_t lane = 0; lane < Width; ++lane)
        {
            std::apply([&](auto&... fields)
            {
                auto print = [&](auto& value)
                {
                    if (!first)
                    {
                        std::cout << ' ';
                    }

                    first = false;
                    std::cout << static_cast<uint64_t>(ToBits<Bits>(value[lane]));
                };
                (print(fields), ...);
            }, std::tuple_cat(state, OutputFields(core), std::tie(power)));
        }

        std::cout << '\n';
    }
}

template<typename Number>
void Dispatch(size_t width, size_t steps)
{
    switch (width)
    {
        case 1: Run<Number, 1>(steps); break;
        case 2: Run<Number, 2>(steps); break;
        case 3: Run<Number, 3>(steps); break;
        case 4: Run<Number, 4>(steps); break;
        default: std::abort();
    }
}

int main()
{
    std::string tag;
    int precision;
    size_t width, steps;
    while (std::cin >> tag >> precision >> width >> steps)
    {
        if (tag != "case")
        {
            return 1;
        }

        if (precision == 32)
        {
            Dispatch<float>(width, steps);
        }
        else if (precision == 64)
        {
            Dispatch<double>(width, steps);
        }
        else
        {
            return 1;
        }
    }
}

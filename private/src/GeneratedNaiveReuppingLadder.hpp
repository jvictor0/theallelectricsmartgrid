#pragma once

#include "NaiveReuppingLadderCoreGeneric.hpp"
#include "SampledTransferFunction.hpp"
#include "SurveyResponseTable.hpp"
#include <optional>

// One app resource, installed on the UI thread before the first paint.
//
struct NaiveLadderResponse
{
    static constexpr size_t x_bands = 144;
    static constexpr size_t x_bytes = 9999872;
    static constexpr auto x_sha256 = "c178ba51a4d38e4574c088ae1cac8be8b3934ec26448b280e81ced94bc5d473f";
    static constexpr double x_engineRate = 192000;
    static inline std::optional<dsp_survey::ResponseTable> s_table;

    static std::array<double, 3> Coordinates(double cutoff, double resonance, double gain)
    {
        // Invert the knob mapping in SurveyExamples.NaiveLadder.prepare.
        //
        return { std::log2(4 * gain) / 4.321928094887363,
            std::log1p(48 * resonance) / std::log(49.0), cutoff * x_engineRate };
    }

    static float Power(double db)
    {
        return static_cast<float>(std::exp2(db * 0.33219280948873625));
    }
};

template<typename Number, size_t Size>
struct GeneratedNaiveReuppingLadderGeneric : NaiveReuppingLadderCoreGeneric<Number, Size>
{
    using Core = NaiveReuppingLadderCoreGeneric<Number, Size>;
    using Vector = typename Core::Vector;
    static constexpr Number x_maxCutoff = static_cast<Number>(0.5);
    static constexpr Number x_halfPowerRatio = static_cast<Number>(0.43497944204608224);
    typename Core::LogicalSpec m_spec{};

    struct Snapshot : PowerResponseSnapshot<Size>
    {
        using typename PowerResponseSnapshot<Size>::Frequency;
        using typename PowerResponseSnapshot<Size>::ResponseVector;
        typename Core::Snapshot m_controls{};

        using PreparedResponse = std::array<std::array<double, NaiveLadderResponse::x_bands>, Size>;

        auto Coordinates(size_t lane) const
        {
            return NaiveLadderResponse::Coordinates(m_controls.m_cutoff[lane],
                m_controls.m_resonance[lane], m_controls.m_gain[lane]);
        }

        void PrepareResponse(PreparedResponse& response) const
        {
            if (const auto& table = NaiveLadderResponse::s_table)
            {
                std::array<double, NaiveLadderResponse::x_bands> scratch;
                for (size_t lane = 0; lane < Size; ++lane)
                {
                    const auto controls = Coordinates(lane);
                    table->ReadSpectralKnots(controls.data(), 0, table->m_bands,
                        response[lane].data(), scratch.data());
                }
            }
        }

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency,
            const PreparedResponse& prepared) const
        {
            ResponseVector power{};
            if (const auto& table = NaiveLadderResponse::s_table)
            {
                const auto firstHz = frequency.m_cyclesPerSample[0] * NaiveLadderResponse::x_engineRate;
                const auto first = table->LocateFrequency(firstHz);
                for (size_t lane = 0; lane < Size; ++lane)
                {
                    const auto hz = frequency.m_cyclesPerSample[lane] * NaiveLadderResponse::x_engineRate;
                    const auto position = hz == firstHz ? first : table->LocateFrequency(hz);
                    power[lane] = NaiveLadderResponse::Power(table->DisplayDbAt(prepared[lane].data(), position));
                }
            }

            return power;
        }

        ResponseVector MagnitudeSquaredResponses(const Frequency& frequency) const override
        {
            PreparedResponse prepared;
            PrepareResponse(prepared);
            return MagnitudeSquaredResponses(frequency, prepared);
        }
    };

    using UIState = TransferFunctionGeneric<Snapshot>;

    GeneratedNaiveReuppingLadderGeneric()
    {
        SetCutoffResonanceAndGain(Vector::Splat(static_cast<Number>(0.1)), Vector(),
            Vector::Splat(static_cast<Number>(0.5)));
    }

    void SetCutoffResonanceAndGain(const Vector& cutoff, const Vector& resonance, const Vector& gain)
    {
        const auto clamped = Vector::Min(Vector::Splat(x_maxCutoff), cutoff);
        if (clamped != m_spec.m_cutoff || resonance != m_spec.m_resonance || gain != m_spec.m_gain)
        {
            m_spec = {clamped, resonance, gain};
            this->ComputeCoefficients(m_spec);
        }
    }

    void SetCutoffAndResonance(const Vector& cutoff, const Vector& resonance)
    {
        SetCutoffResonanceAndGain(cutoff, resonance, m_spec.m_gain);
    }

    void SetSaturationGain(const Vector& gain)
    {
        SetCutoffResonanceAndGain(m_spec.m_cutoff, m_spec.m_resonance, gain);
    }

    Vector Process(const Vector& sample)
    {
        typename Core::Input input{sample};
        Core::Process(input);
        return this->m_output.m_x;
    }

    Snapshot MakeSnapshot() const
    {
        Snapshot snapshot;
        snapshot.m_controls = Core::MakeSnapshot();
        return snapshot;
    }

    void PopulateUIStateSample(SampledSnapshotWriter<UIState>& writer) const
    {
        writer.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }
};

using TrioGeneratedNaiveReuppingLadder = GeneratedNaiveReuppingLadderGeneric<float, 3>;

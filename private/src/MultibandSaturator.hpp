#pragma once

#include "MultibandSaturator4BandCoreGeneric.hpp"
#include "PhaseUtils.hpp"
#include "Metering.hpp"
#include "SampleTimer.hpp"
#include <algorithm>

template<size_t NumBands, size_t NumChannels>
struct MultibandSaturator
{
    static_assert(NumBands == 4, "the generated mastering core has four bands");
    using Core = MultibandSaturator4BandCoreGeneric<float, NumChannels>;
    using Vector = typename Core::Vector;
    using CutoffVector = MultiChannelNumber<float, 3>;
    using ResponseCore = MultibandSaturator4BandCoreGeneric<float, 1>;
    using Snapshot = GeneratedResponseSnapshot<ResponseCore, &ResponseCore::MagnitudeSquared>;
    static constexpr float x_minCutoff = 0.0001f;
    static constexpr float x_maxCutoff = 0.499f;

    Core m_core{};
    typename Core::PreparedSpec m_prepared{};
    CutoffVector m_cutoffs{};
    Vector m_output{};
    float m_sub = 0;
    MeterGeneric<float, NumChannels> m_meter[NumBands];
    MeterGeneric<float, NumChannels> m_masterMeter;

    struct UIState : TransferFunctionGeneric<Snapshot>
    {
        using TransferFunctionGeneric<Snapshot>::TransferFunctionGeneric;

        MeterReaderGeneric<float, NumChannels> m_meterReader[NumBands];
        MeterReaderGeneric<float, NumChannels> m_masterMeterReader;

        void Process()
        {
            for (auto& meter : m_meterReader)
            {
                meter.Process();
            }

            m_masterMeterReader.Process();
        }
    };

    SampledSnapshotWriter<UIState> m_responseSampler;

    void SetupUIState(UIState* uiState)
    {
        m_responseSampler.Bind(uiState);
        for (size_t i = 0; i < NumBands; ++i)
        {
            uiState->m_meterReader[i].SetMeter(&m_meter[i]);
        }

        uiState->m_masterMeterReader.SetMeter(&m_masterMeter);
    }

    struct Input
    {
        PhaseUtils::ExpParam m_gain[NumBands];
        PhaseUtils::ExpParam m_masterGain;
        PhaseUtils::ExpParam m_bassFreq;
        PhaseUtils::ExpParam m_crossoverFreqFactor[NumBands - 2];

        Input()
        {
            for (size_t i = 0; i < NumBands; ++i)
            {
                m_gain[i] = PhaseUtils::ExpParam(1.0 / 16.0f, 2.0f);
                m_gain[i].m_expParam = 1.0;
            }

            m_masterGain = PhaseUtils::ExpParam(1.0 / 4.0f, 2.0f);
            m_masterGain.m_expParam = 1.0;

            // The crossover range is defined in cycles per host sample, so it keeps
            // its frequencies in Hz at any engine rate.
            //
            m_bassFreq = PhaseUtils::ExpParam(
                SampleTimer::HzToCycPerSamp(SampleTimer::CycPerHostSampToHz(0.5 / 1024.0f)),
                SampleTimer::HzToCycPerSamp(SampleTimer::CycPerHostSampToHz(0.5 / (std::pow(1024, static_cast<float>(NumBands - 2) / (NumBands - 1))))));
            m_bassFreq.Update(0.5);

            for (size_t i = 0; i < NumBands - 2; ++i)
            {
                m_crossoverFreqFactor[i] = PhaseUtils::ExpParam(1.0, 32.0f);
                m_crossoverFreqFactor[i].Update(0.5);
            }
        }

        void PopulateUIState(UIState* uiState)
        {
            uiState->Process();
        }

    };

    // Packed lanes represent cutoffs here, independently of the audio width.
    // Section preparation is generated from the same Lean SVF tuner as audio.
    //
    template<typename Section, typename Bank>
    static void BroadcastSection(Section& section, const Bank& bank, size_t index)
    {
        section.m_snapshot.m_g = Vector::Splat(bank.m_snapshot.m_g[index]);
        section.m_snapshot.m_k = Vector::Splat(bank.m_snapshot.m_k[index]);
        section.m_a1 = Vector::Splat(bank.m_a1[index]);
        section.m_a2 = Vector::Splat(bank.m_a2[index]);
        section.m_a3 = Vector::Splat(bank.m_a3[index]);
    }

    void SetCoefficients(const Input& input)
    {
        const float low = input.m_bassFreq.m_expParam;
        const float middle = low * input.m_crossoverFreqFactor[0].m_expParam;
        const float high = middle * input.m_crossoverFreqFactor[1].m_expParam;
        const auto cutoffs = CutoffVector::Max(CutoffVector::Splat(x_minCutoff),
            CutoffVector::Min(CutoffVector::Splat(x_maxCutoff), CutoffVector(low, middle, high)));
        if (cutoffs != m_cutoffs)
        {
            const auto bank = Core::template Preparation<3>::Prepare(cutoffs);
            BroadcastSection(m_prepared.m_splitter.m_tree.m_value, bank, 1);
            BroadcastSection(m_prepared.m_splitter.m_tree.m_low.m_value, bank, 0);
            BroadcastSection(m_prepared.m_splitter.m_tree.m_high.m_value, bank, 2);
            m_cutoffs = cutoffs;
        }
        else
        {
            bool gainsChanged = input.m_masterGain.m_expParam != m_prepared.m_masterGain[0];
            for (size_t band = 0; band < NumBands; ++band)
            {
                gainsChanged |= input.m_gain[band].m_expParam != m_prepared.m_bandGain[band][0];
            }

            if (!gainsChanged)
            {
                return;
            }
        }

        for (size_t band = 0; band < NumBands; ++band)
        {
            m_prepared.m_bandGain[band] = Vector::Splat(input.m_gain[band].m_expParam);
        }

        m_prepared.m_masterGain = Vector::Splat(input.m_masterGain.m_expParam);
        m_core.InstallPrepared(m_prepared);
    }

    template<typename Target, typename Source>
    static void CaptureSection(Target& target, const Source& source)
    {
        target.m_g = ResponseCore::Vector::Splat(source.m_g[0]);
        target.m_k = ResponseCore::Vector::Splat(source.m_k[0]);
    }

    // App coefficients are shared between audio channels. The single UI curve
    // describes common-channel input, including when mono bass is enabled.
    //
    Snapshot MakeSnapshot() const
    {
        const auto& source = m_core.m_responseSnapshot;
        Snapshot snapshot;
        CaptureSection(snapshot.m_splitter.m_tree.m_value, source.m_splitter.m_tree.m_value);
        CaptureSection(snapshot.m_splitter.m_tree.m_low.m_value, source.m_splitter.m_tree.m_low.m_value);
        CaptureSection(snapshot.m_splitter.m_tree.m_high.m_value, source.m_splitter.m_tree.m_high.m_value);
        for (size_t band = 0; band < NumBands; ++band)
        {
            snapshot.m_bandGain[band] = ResponseCore::Vector::Splat(source.m_bandGain[band][0]);
        }

        snapshot.m_masterGain = ResponseCore::Vector::Splat(source.m_masterGain[0]);
        return snapshot;
    }

    void PopulateUIStateSample()
    {
        m_responseSampler.PopulateUIStateSample([&]
        {
            return MakeSnapshot();
        });
    }

    Vector Process(const Input& input, const Vector& in, bool monoTheBass)
    {
        SetCoefficients(input);
        PopulateUIStateSample();
        typename Core::Input sample{in, monoTheBass};
        m_core.Process(sample);
        const auto& result = m_core.m_output;
        for (size_t band = 0; band < NumBands; ++band)
        {
            m_meter[band].RecordSaturated(result.m_bands[band].m_before, result.m_bands[band].m_after);
        }

        m_masterMeter.RecordSaturated(result.m_master.m_before, result.m_master.m_after);
        m_output = result.m_master.m_after;
        m_sub = result.m_sub[0];
        return m_output;
    }
};

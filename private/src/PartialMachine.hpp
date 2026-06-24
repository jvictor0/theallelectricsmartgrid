#pragma once

#include "GangedRandomLFO.hpp"
#include "OLA.hpp"
#include "PhaseUtils.hpp"
#include "QuadUtils.hpp"
#include "SampleTimer.hpp"
#include "ScopeWriter.hpp"
#include "SnapshotUIState.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "SpectralModel.hpp"
#include "TransferFunction.hpp"

#include <algorithm>
#include <cmath>

struct PartialMachine
{
    using SpectralModel = SpectralModelGeneric<12, FrequencyDependentParameter>;
    using Parameter = FrequencyDependentParameter::Parameter;
    using AtomicParameter = FrequencyDependentParameter::AtomicParameter;
    using Index = FrequencyDependentParameter::Index;

    struct SynthesisContext
    {
        struct Input
        {
            Parameter m_bwBaseFrequency;
            Parameter m_bwWidth;
            Parameter m_volume;
            Parameter m_bassCutoff;
            Parameter m_azimuthFactor;
            Parameter m_syntheticGain;
            Parameter m_organicGain;
            Parameter m_reductionFeedback;
            Parameter m_unison;
            Parameter m_pitchShiftDepth;
            Parameter m_pitchShift;
            double m_azimuthOffset;

            Input()
                : m_syntheticGain(1.0f)
                , m_organicGain(0.0f)
                , m_pitchShiftDepth(1.0f)
                , m_pitchShift(0.0f)
                , m_azimuthOffset(0.0f)
            {
            }
        };

        static float GetReduction(float frequency, Index index, Input& input)
        {
            float bwBaseFrequency = input.m_bwBaseFrequency.Process(index);
            float bwWidth = input.m_bwWidth.Process(index);
            float volume = input.m_volume.Process(index);

            float belowReduction = 1.0f;
            if (frequency < bwBaseFrequency)
            {
                float octaves = std::log2f(bwBaseFrequency / frequency);
                belowReduction = std::powf(10.0f, -12.0f * octaves / 20.0f);
            }

            float highThreshold = bwBaseFrequency * bwWidth;
            float aboveReduction = 1.0f;
            if (highThreshold < frequency)
            {
                float octaves = std::log2f(frequency / highThreshold);
                aboveReduction = std::powf(10.0f, -12.0f * octaves / 20.0f);
            }

            return belowReduction * aboveReduction * volume;
        }

        static float GetReduction(const SpectralModel::Atom& atom, Input& input)
        {
            return GetReduction(atom.m_synthesisOmega, atom.m_index, input);
        }

        static float GetRadius(const SpectralModel::Atom& atom, Input& input)
        {
            return GetRadius(atom.m_synthesisOmega, atom.m_index, input);
        }

        static float GetRadius(float frequency, Index index, Input& input)
        {
            float bassCutoff = input.m_bassCutoff.Process(index);

            if (frequency < bassCutoff)
            {
                return 0.0f;
            }

            if (bassCutoff * 2.0f < frequency)
            {
                return 1.0f;
            }

            return std::log2f(frequency / bassCutoff);
        }

        static float GetAzimuth(const SpectralModel::Atom& atom, Input& input)
        {
            return GetAzimuth(atom.m_synthesisOmega, atom.m_index, input);
        }

        static float GetAzimuth(float frequency, Index index, Input& input)
        {
            double azimuthFactor = input.m_azimuthFactor.Process(index);
            double linearSynthesisOmega = FrequencyDependentParameter::FrequencyToLinear(frequency);
            double azimuth = azimuthFactor * linearSynthesisOmega + input.m_azimuthOffset;
            return azimuth - std::floor(azimuth);
        }

        static float GetPitchShiftFactor(Index index, Input& input)
        {
            float pitchShiftDepth = input.m_pitchShiftDepth.Process(index);
            float pitchShift = input.m_pitchShift.ProcessLinear(index);
            return std::powf(pitchShiftDepth, pitchShift);
        }

        static float GetPitchShiftedOmega(float omega, Index index, Input& input)
        {
            return omega * GetPitchShiftFactor(index, input);
        }

        static float GetPitchShiftedOmega(const SpectralModel::Atom& atom, Input& input)
        {
            return GetPitchShiftedOmega(atom.m_synthesisOmega, atom.m_index, input);
        }

        static QuadFloat Pan(float azimuth, float radius)
        {
            float x = 0.5f + 0.5f * radius * Math::Cos2pi(azimuth);
            float y = 0.5f + 0.5f * radius * Math::Sin2pi(azimuth);
            return QuadFloat::Pan(x, y, 1.0f);
        }

        struct UnisonContext
        {
            // Voices are added in symmetric pairs; pair 3 (voices 5 and 6) always
            // resolves to interp = 0 -> gain ZeroedExpParam::Compute(4, 0) = 0, so it
            // is always silent. Cap at 5 so the synthesis loop skips the dead pair.
            //
            static constexpr size_t x_numVoices = 5;

            float m_gain[x_numVoices];
            float m_detune[x_numVoices];
            float m_azimuthOffset[x_numVoices];
        };

        static UnisonContext GetUnisonContext(Index index, Input& input)
        {
            float unison = input.m_unison.ProcessLinear(index);

            UnisonContext result{};
            result.m_gain[0] = 1.0f;
            result.m_detune[0] = 1.0f;
            result.m_azimuthOffset[0] = 0.0f;
            for (size_t v = 1; v < UnisonContext::x_numVoices; ++v)
            {
                int pair = (v + 1) / 2;
                int sign = 2 * ((v + 1) % 2) - 1;
                float interp = std::min<float>(1.0f, (3 - pair) * unison);
                result.m_gain[v] = PhaseUtils::ZeroedExpParam::Compute(4.0f, interp);
                result.m_detune[v] = std::powf(1.06, sign * interp);
                result.m_azimuthOffset[v] = sign * interp / std::powf(2, 4 - pair);
            }

            float rms = 0;
            for (size_t v = 0; v < UnisonContext::x_numVoices; ++v)
            {
                rms += result.m_gain[v] * result.m_gain[v];
            }

            rms = std::sqrt(rms);
            for (size_t v = 0; v < UnisonContext::x_numVoices; ++v)
            {
                result.m_gain[v] /= rms;
            }

            return result;
        }

        void ProcessAtom(SpectralModel::Atom& atom, Input& input)
        {
            float shiftedSynthesisOmega = GetPitchShiftedOmega(atom, input);
            float reduction = GetReduction(atom, input);
            float reducedMagnitude = atom.m_synthesisMagnitude * reduction;
            float newMagnitude = PhaseUtils::ExpParam::Compute(
                atom.m_synthesisMagnitude, 
                std::max(SpectralModel::x_deathMag, reducedMagnitude), 
                input.m_reductionFeedback.ProcessLinear(atom.m_index));
                
            if (atom.m_isSynthetic)
            {
                reduction *= input.m_syntheticGain.Process(atom.m_index);
            }
            else
            {
                reduction *= input.m_organicGain.Process(atom.m_index);
            }

            float radius = GetRadius(atom, input);
            float azimuth = GetAzimuth(atom, input);
            UnisonContext unisonContext = GetUnisonContext(atom.m_index, input);
            for (size_t v = 0; v < UnisonContext::x_numVoices; ++v)
            {
                float voiceAzimuth = azimuth + unisonContext.m_azimuthOffset[v];
                voiceAzimuth = voiceAzimuth - std::floor(voiceAzimuth);
                QuadFloat distribution = Pan(voiceAzimuth, radius);
                float voiceMagnitude = reducedMagnitude * unisonContext.m_gain[v];
                float voicePhase = static_cast<float>(atom.m_synthesisPhase * unisonContext.m_detune[v]);
                float voiceOmega = shiftedSynthesisOmega * unisonContext.m_detune[v];
                m_dft.WriteWindowedPartial(voiceMagnitude, voicePhase, voiceOmega, distribution);
            }

            atom.UpdatePhase();
            atom.m_synthesisMagnitude = newMagnitude;
        }

        QuadDFT m_dft;
    };

    struct Input
    {
        SpectralModel::Input m_spectralModelInput;
        SynthesisContext::Input m_synthesisContextInput;

        Input()
        {
        }
    };

    struct ResidualMachine
    {
        RGen m_gen;

        void Process(QuadDFT& dft, SpectralModel& spectralModel, Input& input)
        {
            for (size_t k = 1; k < SpectralModel::ResidualModel::x_numBuckets; ++k)
            {
                float frequency = spectralModel.m_residualModel.m_frequencies[k];
                float logFrequency = spectralModel.m_residualModel.m_logFrequencies[k];
                Index index = FrequencyDependentParameter::GetIndexForLogFrequency(logFrequency, input.m_spectralModelInput.m_parameterInput);
                float envelope = spectralModel.m_residualModel.GetEnvelope(k);
                float reduction = SynthesisContext::GetReduction(frequency, index, input.m_synthesisContextInput);
                float reducedMagnitude = envelope * reduction;
                float radius = SynthesisContext::GetRadius(frequency, index, input.m_synthesisContextInput);
                float azimuth = SynthesisContext::GetAzimuth(frequency, index, input.m_synthesisContextInput);
                QuadFloat distribution = SynthesisContext::Pan(azimuth, radius);
                float phase = m_gen.UniGen();
                std::complex<float> value(
                    reducedMagnitude * Math::Cos2pi(phase),
                    reducedMagnitude * Math::Sin2pi(phase));
                dft.AddComponent(k, value, distribution);
                spectralModel.m_residualModel.m_magnitudes[k] = PhaseUtils::ExpParam::Compute(
                    std::max(SpectralModel::x_deathMag, envelope),
                    std::max(SpectralModel::x_deathMag, reducedMagnitude),
                    input.m_synthesisContextInput.m_reductionFeedback.ProcessLinear(index));
            }
        }
    };

    struct InputSetter
    {
        static constexpr float x_hopSeconds = static_cast<float>(SpectralModel::x_H) / static_cast<float>(SampleTimer::x_sampleRate);
        static constexpr size_t x_numAtoms = 1024;

        struct Input
        {
            QuadFloat m_parameterLinearFrequency;
            QuadFloat m_attack;
            QuadFloat m_decay;
            QuadFloat m_portamento;
            QuadFloat m_density;
            QuadFloat m_bwBaseFrequency;
            QuadFloat m_bwWidth;
            QuadFloat m_volume;
            QuadFloat m_bassCutoff;
            QuadFloat m_azimuthFactor;
            QuadFloat m_reductionFeedback;
            QuadFloat m_unison;
            QuadFloat m_pitchShiftDepth;
            QuadFloat m_pitchShift;
            QuadFloat m_syntheticMixKnob;

            Input()
            {
            }
        };

        static float TimeToAlpha(float seconds)
        {
            return 1.0f - std::exp(-x_hopSeconds / seconds);
        }

        PhaseUtils::ExpParam m_attack[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_decay[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_portamento[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_parameterLinearFrequency[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_density[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_bwBaseFrequency[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_bwWidth[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ZeroedExpParam m_volume[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_bassCutoff[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_azimuthFactor[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_pitchShiftDepth[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpHalfRangeCrossfade m_syntheticMix[FrequencyDependentParameter::x_numParameters];
        PhaseUtils::ExpParam m_syntheticHarmonicMagnitude[SpectralModel::x_numSyntheticHarmonics][FrequencyDependentParameter::x_numParameters];
        ManyGangedRandomLFO m_syntheticHarmonicLFO[SpectralModel::x_numSyntheticHarmonics];
        ManyGangedRandomLFO::Input m_syntheticHarmonicLFOInput[SpectralModel::x_numSyntheticHarmonics];

        InputSetter()
        {
            for (size_t h = 0; h < SpectralModel::x_numSyntheticHarmonics; ++h)
            {
                m_syntheticHarmonicLFOInput[h].m_gangSize = FrequencyDependentParameter::x_numParameters;
                m_syntheticHarmonicLFOInput[h].m_time = 6.0f;
                m_syntheticHarmonicLFOInput[h].m_sigma = 0.2f;
                m_syntheticHarmonicLFOInput[h].m_numGangs = 1;
            }

            for (int i = 0; i < FrequencyDependentParameter::x_numParameters; ++i)
            {
                m_attack[i] = PhaseUtils::ExpParam(0.01, 2.0);
                m_decay[i] = PhaseUtils::ExpParam(0.01, 10.0);
                m_portamento[i] = PhaseUtils::ExpParam(0.01, 2.0);
                m_parameterLinearFrequency[i] = PhaseUtils::ExpParam(1.0f / 10.0f, 100.0f);
                m_density[i] = PhaseUtils::ExpParam(1.0f / 1024.0f, 1.0f / 64.0f);
                m_bwBaseFrequency[i] = PhaseUtils::ExpParam(1.0f / 2048.0f, 0.5f);
                m_bwWidth[i] = PhaseUtils::ExpParam(1.0f, 2048.0f);
                m_bassCutoff[i] = PhaseUtils::ExpParam(1.0f / 2048.0f, 0.5f);
                m_azimuthFactor[i] = PhaseUtils::ExpParam(1.0f / 32.0f, 1.0f);
                m_pitchShiftDepth[i] = PhaseUtils::ExpParam(std::powf(2.0f, 5.0f / 1200.0f), 2.0f);
                m_syntheticMix[i].SetBaseByCenter(0.25f);

                for (size_t h = 0; h < SpectralModel::x_numSyntheticHarmonics; ++h)
                {
                    m_syntheticHarmonicMagnitude[h][i] = PhaseUtils::ExpParam(1.0f / 16.0f, 1.0f);
                }
            }
        }

        void SetInput(const Input& knobInput, PartialMachine::Input& input)
        {
            input.m_spectralModelInput.m_numAtoms = x_numAtoms;
            input.m_spectralModelInput.m_useSyntheticHarmonics = false;

            for (size_t h = 0; h < SpectralModel::x_numSyntheticHarmonics; ++h)
            {
                m_syntheticHarmonicLFO[h].Process(1.0f / static_cast<float>(SampleTimer::x_sampleRate), m_syntheticHarmonicLFOInput[h]);
            }

            for (int i = 0; i < FrequencyDependentParameter::x_numParameters; ++i)
            {
                input.m_spectralModelInput.m_parameterInput.m_linearFreqs.m_parameters[i] = m_parameterLinearFrequency[i].Update(knobInput.m_parameterLinearFrequency[i]);
                input.m_spectralModelInput.m_slewUpAlpha.m_parameters[i] = TimeToAlpha(m_attack[i].Update(knobInput.m_attack[i]));
                input.m_spectralModelInput.m_slewDownAlpha.m_parameters[i] = TimeToAlpha(m_decay[i].Update(knobInput.m_decay[i]));
                input.m_spectralModelInput.m_omegaPortamentoAlpha.m_parameters[i] = TimeToAlpha(m_portamento[i].Update(knobInput.m_portamento[i]));
                input.m_spectralModelInput.m_omegaDensity.m_parameters[i] = m_density[i].Update(1.0f - knobInput.m_density[i]);
                input.m_synthesisContextInput.m_bwBaseFrequency.m_parameters[i] = m_bwBaseFrequency[i].Update(knobInput.m_bwBaseFrequency[i]);
                input.m_synthesisContextInput.m_bwWidth.m_parameters[i] = m_bwWidth[i].Update(knobInput.m_bwWidth[i]);
                input.m_synthesisContextInput.m_volume.m_parameters[i] = m_volume[i].Update(knobInput.m_volume[i]);
                input.m_synthesisContextInput.m_bassCutoff.m_parameters[i] = m_bassCutoff[i].Update(knobInput.m_bassCutoff[i]);
                input.m_synthesisContextInput.m_azimuthFactor.m_parameters[i] = m_azimuthFactor[i].Update(knobInput.m_azimuthFactor[i]);
                input.m_synthesisContextInput.m_reductionFeedback.m_parameters[i] = knobInput.m_reductionFeedback[i];
                input.m_synthesisContextInput.m_unison.m_parameters[i] = knobInput.m_unison[i];
                input.m_synthesisContextInput.m_pitchShiftDepth.m_parameters[i] = m_pitchShiftDepth[i].Update(knobInput.m_pitchShiftDepth[i]);
                input.m_synthesisContextInput.m_pitchShift.m_parameters[i] = 2.0f * knobInput.m_pitchShift[i] - 1.0f;

                // For the time being, the synthetic harmonics are disabled.
                //
                if (input.m_spectralModelInput.m_useSyntheticHarmonics)
                {                
                    m_syntheticMix[i].Process(
                        knobInput.m_syntheticMixKnob[i],
                        input.m_synthesisContextInput.m_organicGain.m_parameters[i],
                        input.m_synthesisContextInput.m_syntheticGain.m_parameters[i]);

                    for (size_t h = 0; h < SpectralModel::x_numSyntheticHarmonics; ++h)
                    {
                        float harmonicMagnitude = std::min(1.0f, std::max(0.0f, m_syntheticHarmonicLFO[h].m_lfos[0].m_pos[i]));
                        input.m_spectralModelInput.m_syntheticHarmonics[h].m_parameters[i] = m_syntheticHarmonicMagnitude[h][i].Update(harmonicMagnitude) / (h + 2);
                    }
                }
                else
                {
                    input.m_synthesisContextInput.m_organicGain.m_parameters[i] = 1.0f;
                    input.m_synthesisContextInput.m_syntheticGain.m_parameters[i] = 0.0f;
                }
            }
        }
    };

    PartialMachine()
        : m_index(0)
    {
    }

    void ProcessSynthesisFrame(Input& input)
    {
        SynthesisContext synthesisContext;
        for (size_t i = 0; i < m_spectralModel.m_atoms.Size(); ++i)
        {
            synthesisContext.ProcessAtom(*m_spectralModel.m_atoms[i], input.m_synthesisContextInput);
        }

        m_residualMachine.Process(synthesisContext.m_dft, m_spectralModel, input);
        m_ola.Write(synthesisContext.m_dft);
    }

    QuadFloat Process(QuadFloat inputSample, Input& input)
    {
        float summedInput = inputSample.Sum();
        m_scopeWriter.Write(summedInput);
        m_buffer.m_table[m_index % SpectralModel::x_tableSize] = summedInput;
        ++m_index;

        if (m_index % SpectralModel::x_H == 0)
        {
            SpectralModel::Buffer buffer;
            for (size_t i = 0; i < SpectralModel::x_tableSize; ++i)
            {
                buffer.m_table[i] = m_buffer.m_table[(m_index + i) % SpectralModel::x_tableSize] * Math4096::Hann(i);
            }

            m_spectralModel.ExtractAtomsAndResidual(buffer, input.m_spectralModelInput);
            ProcessSynthesisFrame(input);
        }

        return m_ola.Process();
    }

    void SetupAudioScopeWriter(ScopeWriter* scopeWriter)
    {
        m_scopeWriter = ScopeWriterHolder(scopeWriter, 0, static_cast<size_t>(SmartGridOne::MonoAudioScopes::PartialMachine));
    }

    struct Snapshot
    {
        size_t m_numAtoms;
        SpectralModel::Atom m_atoms[SpectralModel::x_maxAtoms];

        Snapshot()
            : m_numAtoms(0)
            , m_atoms{}
        {
        }
    };

    struct UIState : SnapshotUIState<Snapshot>, TransferFunction
    {
        struct QuadComponentTransferFunction : TransferFunction
        {
            UIState* m_owner;
            size_t m_speakerIx;

            QuadComponentTransferFunction()
                : m_owner(nullptr)
                , m_speakerIx(0)
            {
            }

            QuadComponentTransferFunction(UIState* owner, size_t speakerIx)
                : m_owner(owner)
                , m_speakerIx(speakerIx)
            {
            }

            std::complex<float> TransferFunctionValue(float frequency) const override
            {
                float reduction = m_owner->QuadComponentFrequencyResponse(frequency, m_speakerIx);
                return std::complex<float>(reduction, 0.0f);                
            }

            float FrequencyResponse(float frequency) const override
            {
                return m_owner->QuadComponentFrequencyResponse(frequency, m_speakerIx);
            }
        };

        FrequencyDependentParameter::UIState m_parameterUIState;
        AtomicParameter m_bwBaseFrequency;
        AtomicParameter m_bwWidth;
        AtomicParameter m_volume;
        AtomicParameter m_bassCutoff;
        AtomicParameter m_azimuthFactor;
        AtomicParameter m_pitchShiftDepth;
        AtomicParameter m_pitchShift;
        std::atomic<float> m_azimuthOffset;

        QuadComponentTransferFunction m_quadComponentTransferFunction[4];

        UIState()
        {
            for (size_t i = 0; i < 4; ++i)
            {
                m_quadComponentTransferFunction[i] = QuadComponentTransferFunction(this, i);
                m_pitchShiftDepth.m_parameters[i].store(1.0f);
            }
        }

        Input ToInput() const
        {
            Input result;
            result.m_spectralModelInput.m_parameterInput = m_parameterUIState.ToInput();
            result.m_synthesisContextInput.m_bwBaseFrequency = m_bwBaseFrequency.ToParameter();
            result.m_synthesisContextInput.m_bwWidth = m_bwWidth.ToParameter();
            result.m_synthesisContextInput.m_volume = m_volume.ToParameter();
            result.m_synthesisContextInput.m_bassCutoff = m_bassCutoff.ToParameter();
            result.m_synthesisContextInput.m_azimuthFactor = m_azimuthFactor.ToParameter();
            result.m_synthesisContextInput.m_pitchShiftDepth = m_pitchShiftDepth.ToParameter();
            result.m_synthesisContextInput.m_pitchShift = m_pitchShift.ToParameter();
            result.m_synthesisContextInput.m_azimuthOffset = m_azimuthOffset.load();
            return result;
        }

        void FromInput(const Input& input)
        {
            m_parameterUIState.FromInput(input.m_spectralModelInput.m_parameterInput);
            m_bwBaseFrequency.FromParameter(input.m_synthesisContextInput.m_bwBaseFrequency);
            m_bwWidth.FromParameter(input.m_synthesisContextInput.m_bwWidth);
            m_volume.FromParameter(input.m_synthesisContextInput.m_volume);
            m_bassCutoff.FromParameter(input.m_synthesisContextInput.m_bassCutoff);
            m_azimuthFactor.FromParameter(input.m_synthesisContextInput.m_azimuthFactor);
            m_pitchShiftDepth.FromParameter(input.m_synthesisContextInput.m_pitchShiftDepth);
            m_pitchShift.FromParameter(input.m_synthesisContextInput.m_pitchShift);
            m_azimuthOffset.store(input.m_synthesisContextInput.m_azimuthOffset);
        }
        
        float FrequencyResponse(float frequency) const override
        {
            Input dspInput = ToInput();
            Index index = FrequencyDependentParameter::GetIndexForFrequency(frequency, dspInput.m_spectralModelInput.m_parameterInput);
            return SynthesisContext::GetReduction(frequency, index, dspInput.m_synthesisContextInput);
        }

        float QuadComponentFrequencyResponse(float frequency, size_t speakerIx) const
        {
            Input dspInput = ToInput();
            Index index = FrequencyDependentParameter::GetIndexForFrequency(frequency, dspInput.m_spectralModelInput.m_parameterInput);
            float reduction = SynthesisContext::GetReduction(frequency, index, dspInput.m_synthesisContextInput);
            float radius = SynthesisContext::GetRadius(frequency, index, dspInput.m_synthesisContextInput);
            float azimuth = SynthesisContext::GetAzimuth(frequency, index, dspInput.m_synthesisContextInput);
            QuadFloat distribution = SynthesisContext::Pan(azimuth, radius);
            return reduction * distribution[speakerIx];
        }

        std::complex<float> TransferFunctionValue(float frequency) const override
        {
            return std::complex<float>(FrequencyResponse(frequency), 0.0f);
        }
    };

    void PopulateUIState(UIState& uiState, Input& input)
    {
        uiState.FromInput(input);

        Snapshot& snapshot = uiState.BeginSnapshot();
        snapshot.m_numAtoms = m_spectralModel.m_atoms.Size();
        for (size_t i = 0; i < snapshot.m_numAtoms; ++i)
        {
            snapshot.m_atoms[i] = *m_spectralModel.m_atoms[i];
        }

        uiState.CommitSnapshot();
    }        

    size_t m_index;
    SpectralModel::Buffer m_buffer;
    SpectralModel m_spectralModel;
    ResidualMachine m_residualMachine;
    QuadOLA m_ola;
    ScopeWriterHolder m_scopeWriter;
};

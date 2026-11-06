#pragma once

#include "SquiggleBoyShape.hpp"
#include "SquiggleBoySource.hpp"
#include "VoiceMachineEnums.hpp"
#include "QuadUtils.hpp"
#include "PhaseUtils.hpp"
#include "Filter.hpp"
#include "Slew.hpp"
#include "AHD.hpp"
#include "GangedRandomLFO.hpp"
#include "Lissajous.hpp"
#include "MultiChannelNumber.hpp"
#include "PolyXFader.hpp"
#include "ScopeWriter.hpp"
#include "SmartGridOneEncoders.hpp"
#include "SmartGridOneScopeEnums.hpp"
#include "SampleTimer.hpp"
#include "Oversample.hpp"
#include "RecordingBuffer.hpp"
#include "SquiggleBoyVoiceConfig.hpp"
#include "RecordingManager.hpp"
#include "Saturator.hpp"
#include "VCO.hpp"

#include <algorithm>
#include <cstring>

struct TheoryOfTime;
struct AudioBufferBank;

template <size_t Size>
struct SquiggleLFOGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using PolyXFader = PolyXFaderGeneric<Size>;

    static constexpr size_t x_numPhasors = 6;

    PolyXFader m_polyXFader;
    Vector m_output;

    ScopeWriterHolderGeneric<Size> m_scopeWriter;

    struct Input
    {
        typename PolyXFader::Input m_polyXFaderInput;
        PhaseUtils::ExpParamGeneric<float, Size> m_mult;

        Input()
            : m_mult(1, 16)
        {
            m_polyXFaderInput.m_size = x_numPhasors;
        }
    };

    Vector Process(Input& input)
    {
        input.m_polyXFaderInput.m_mult = input.m_mult.m_expParam;
        m_polyXFader.Process(input.m_polyXFaderInput);
        m_output = m_polyXFader.m_rawOutput;

        m_scopeWriter.Write(m_polyXFader.m_output);
        for (size_t i = 0; i < Size; ++i)
        {
            if (m_polyXFader.m_top[i])
            {
                m_scopeWriter.RecordStartAtVoice(i, m_polyXFader.m_top[i].m_offset);
            }
        }

        return m_output;
    }

    SquiggleLFOGeneric()
        : m_output(Vector::Splat(0))
    {
    }
};

// One track of Size voices, one sample per call. Channel i is voice
// track * Size + i.
//
template <size_t Size>
struct SquiggleBoyTrackGeneric
{
    using Vector = MultiChannelNumber<float, Size>;
    using VoiceConfig = SquiggleBoyVoiceConfig;
    using SourceMachine = VoiceMachine::SourceMachine;
    using Param = SmartGridOneEncoders::Param;
    using Source = SquiggleBoySourceGeneric<Size>;
    using Shape = SquiggleBoyShapeGeneric<Size>;
    using SquiggleLFO = SquiggleLFOGeneric<Size>;

    static constexpr size_t x_recordingBufferWriterSize = RecordingManager::x_numVoices;
    static constexpr size_t x_numGangedRandomLFOs = 4;

    struct AmpSection
    {
        using Envelope = AHDGeneric<Size>;
        using Saturator = TanhSaturatorGeneric<float, Size, true>;

        static constexpr float x_subSlewMinSeconds = 0.001f;
        static constexpr float x_subRiseMaxSeconds = 1.0f;
        static constexpr float x_subFallMaxSeconds = 5.0f;

        Envelope m_ahd;
        Envelope m_modulationAHD;

        Vector m_output;
        Vector m_subOut;

        OPLowPassFilterGeneric<float, Size> m_freqDependentGainSlew;
        Vector m_freqDependentGainTarget;

        BiDirectionalSlewGeneric<Size> m_subSlew;

        ScopeWriterHolderGeneric<Size> m_scopeWriter;
        ScopeWriterHolderGeneric<Size> m_subScopeWriter;
        ScopeWriterHolderGeneric<Size> m_modulationEnvelopeScopeWriter;
        ScopeWriterHolderGeneric<Size> m_ampEnvelopeScopeWriter;
        bool m_subRunning[Size];

        struct Input
        {
            SourceMachine m_sourceMachine;
            Vector m_vcoTargetFreq;
            typename Envelope::InputSetter m_ahdInputSetter;
            typename Envelope::Input m_ahdInput;
            typename Envelope::InputSetter m_modulationAHDInputSetter;
            typename Envelope::Input m_modulationAHDInput;

            PhaseUtils::ZeroedExpParamGeneric<float, Size> m_gain;
            PhaseUtils::ZeroedExpParamGeneric<float, Size> m_subGain;
            PhaseUtils::ExpParamGeneric<float, Size> m_subTanhGain;
            PhaseUtils::ExpParamGeneric<float, Size> m_subRiseSeconds;
            PhaseUtils::ExpParamGeneric<float, Size> m_subFallSeconds;

            bool m_subTrig[Size];

            void SetAHD(const Vector& attack, const Vector& hold, const Vector& decay, const Vector& amplitude)
            {
                m_ahdInputSetter.Set(attack, hold, decay, amplitude, false, m_ahdInput);
            }

            void SetModulationAHD(const Vector& attack, const Vector& hold, const Vector& decay, const Vector& amplitude)
            {
                m_modulationAHDInputSetter.Set(attack, hold, decay, amplitude, true, m_modulationAHDInput);
            }

            Input()
                : m_sourceMachine(SourceMachine::DualWaveShapingVCO)
                , m_vcoTargetFreq(Vector::Splat(0))
                , m_subTanhGain(0.125f, 32.0f)
                , m_subRiseSeconds(x_subSlewMinSeconds, x_subRiseMaxSeconds)
                , m_subFallSeconds(x_subSlewMinSeconds, x_subFallMaxSeconds)
            {
                for (size_t i = 0; i < Size; ++i)
                {
                    m_subTrig[i] = false;
                }
            }
        };

        AmpSection()
            : m_output(Vector::Splat(0))
            , m_subOut(Vector::Splat(0))
            , m_freqDependentGainTarget(Vector::Splat(1))
        {
            m_freqDependentGainSlew.SetAlphaFromNatFreq(SampleTimer::HzToCycPerSamp(250.0));
            for (size_t i = 0; i < Size; ++i)
            {
                m_subRunning[i] = false;
            }
        }

        void WriteEnvelopeScopes()
        {
            m_modulationEnvelopeScopeWriter.Write(m_modulationAHD.m_output);
            m_ampEnvelopeScopeWriter.Write(m_ahd.m_output);
            for (size_t i = 0; i < Size; ++i)
            {
                const AHD& modulationAHD = m_modulationAHD.m_channels[i];
                const AHD& ahd = m_ahd.m_channels[i];
                if (modulationAHD.m_triggered)
                {
                    m_modulationEnvelopeScopeWriter.RecordStartAtVoice(i, 0.0);
                }

                if (modulationAHD.m_changed && modulationAHD.m_state == AHD::State::Idle)
                {
                    m_modulationEnvelopeScopeWriter.RecordEndAtVoice(i);
                }

                if (ahd.m_triggered)
                {
                    m_ampEnvelopeScopeWriter.RecordStartAtVoice(i, 0.0);
                }

                if (ahd.m_changed && ahd.m_state == AHD::State::Idle)
                {
                    m_ampEnvelopeScopeWriter.RecordEndAtVoice(i);
                }
            }
        }

        Vector Process(Input& input, const Vector& filterOutput, const Vector& sub)
        {
            for (size_t i = 0; i < Size; ++i)
            {
                if (!input.m_ahdInput.m_channels[i].m_trig)
                {
                    continue;
                }

                float vcoTargetHz = static_cast<float>(SampleTimer::CycPerSampToHz(input.m_vcoTargetFreq[i]));
                if (vcoTargetHz < 100 || !VoiceMachine::IsSynthLike(input.m_sourceMachine))
                {
                    m_freqDependentGainTarget[i] = 1.0f;
                }
                else
                {
                    m_freqDependentGainTarget[i] = 1.0f / std::sqrt(vcoTargetHz / 100);
                }

                if (m_ahd.m_channels[i].m_state != AHD::State::Idle)
                {
                    m_scopeWriter.RecordEndAtVoice(i);
                }

                m_subRunning[i] = input.m_subTrig[i];
            }

            Vector freqDependentGain = m_freqDependentGainSlew.Process(m_freqDependentGainTarget);

            // AHD::Process now applies amplitude and polarity internally
            //
            m_modulationAHD.Process(input.m_modulationAHDInput);
            Vector ahdEnv = PhaseUtils::ZeroedExpParamGeneric<float, Size>::Compute(10.0f, m_ahd.Process(input.m_ahdInput));

            constexpr float x_slewTimeToCycles = static_cast<float>(SampleTimer::HzToCycPerSamp(1.0 / (2.0 * M_PI)));
            m_subSlew.SetSlewUp(Vector::Splat(x_slewTimeToCycles) / input.m_subRiseSeconds.m_expParam);
            m_subSlew.SetSlewDown(Vector::Splat(x_slewTimeToCycles) / input.m_subFallSeconds.m_expParam);
            Vector subTarget = ahdEnv;
            for (size_t i = 0; i < Size; ++i)
            {
                if (!m_subRunning[i])
                {
                    subTarget[i] = 0.0f;
                }
            }

            Vector subEnvelope = m_subSlew.Process(subTarget);
            Vector subDrive = input.m_subTanhGain.m_expParam;
            Vector saturatedSub = Saturator::Tanh(sub * subEnvelope * subDrive) / Saturator::Tanh(subDrive);
            Vector mainOut = freqDependentGain * ahdEnv * filterOutput;
            Vector subOut = freqDependentGain * saturatedSub * input.m_subGain.m_expParam;
            m_scopeWriter.Write(mainOut);
            m_subScopeWriter.Write(subOut);

            for (size_t i = 0; i < Size; ++i)
            {
                const AHD& ahd = m_ahd.m_channels[i];
                if (ahd.m_triggered)
                {
                    m_scopeWriter.RecordStartAtVoice(i, 0.0);
                }

                if (ahd.m_changed && ahd.m_state == AHD::State::Idle)
                {
                    m_scopeWriter.RecordEndAtVoice(i);
                }
            }

            WriteEnvelopeScopes();

            m_subOut = subOut * input.m_gain.m_expParam;
            m_output = mainOut * input.m_gain.m_expParam;

            return m_output;
        }
    };

    struct PanSection
    {
        using LissajousLFO = LissajousLFOGeneric<Size>;

        LissajousLFO m_lissajousLFO;
        Vector m_outputX;
        Vector m_outputY;

        PanSection()
            : m_outputX(Vector::Splat(0))
            , m_outputY(Vector::Splat(0))
        {
        }

        struct Input : LissajousLFO::Input
        {
            float m_input;

            Input()
                : m_input(0)
            {
            }
        };

        void Process(Input& input)
        {
            m_lissajousLFO.Process(Vector::Splat(input.m_input), input);
            m_outputX = m_lissajousLFO.m_outputX * 0.5f + 0.5f;
            m_outputY = m_lissajousLFO.m_outputY * 0.5f + 0.5f;
        }
    };

    void SetupAudioScopeWriters(ScopeWriterInput<Size>* scopeWriter, size_t track)
    {
        m_source.SetScopeWriters(scopeWriter, track);
        m_shape.SetScopeWriters(scopeWriter, track);
        m_amp.m_scopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::PostAmp), track));
        m_amp.m_subScopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::Sub), track));
    }

    void SetupControlScopeWriters(ScopeWriterInput<Size>* scopeWriter, size_t track)
    {
        m_squiggleLFO[0].m_scopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1), track));
        m_squiggleLFO[1].m_scopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::LFO2), track));
        m_amp.m_modulationEnvelopeScopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::ModulationEnvelope), track));
        m_amp.m_ampEnvelopeScopeWriter = ScopeWriterHolderGeneric<Size>(scopeWriter,
            SmartGridOne::VoiceScopeIndex(static_cast<size_t>(SmartGridOne::VoiceScopes::AmpEnvelope), track));
    }

    // Double state: the slowest portamento stalls short of the target pitch in
    // float at the engine rate.
    //
    OPLowPassFilterGeneric<double, Size> m_baseFreqSlew;
    PhaseUtils::ExpParamGeneric<float, Size> m_baseFreqSlewAmount;
    GangedRandomLFO<Size> m_gangedRandomLFO[x_numGangedRandomLFOs];
    Source m_source;
    VCOGeneric<Size> m_sub;
    Shape m_shape;
    AmpSection m_amp;
    PanSection m_pan;
    SquiggleLFO m_squiggleLFO[2];

    Vector m_output;

    // Recording buffers hold host-rate samples: the track output is decimated
    // and written once per host frame.
    //
    using RecordingDecimator = DownsamplerGeneric<float, Size, 8, SampleTimer::x_oversample>;
    RecordingBuffer m_recordingBuffer[Size];
    RecordingBufferWriter<x_recordingBufferWriterSize> m_recordingBufferWriter[Size];
    RecordingDecimator m_recordingDecimator;
    size_t m_recordingSubSample;

    struct Input
    {
        struct SubInput
        {
            Vector m_baseFreq;

            SubInput()
                : m_baseFreq(Vector::Splat(0))
            {
            }
        };

        VoiceConfig* m_voiceConfig[Size];
        AHD::AHDControl m_ahdControl[Size];
        const GangedRandomLFOInput* m_gangedRandomLFOInput;

        SubInput m_subInput;
        typename Source::Input m_sourceInput;
        typename Shape::Input m_shapeInput;
        typename AmpSection::Input m_ampInput;
        typename PanSection::Input m_panInput;
        typename SquiggleLFO::Input m_squiggleLFOInput[2];

        Input()
            : m_voiceConfig{}
            , m_gangedRandomLFOInput(nullptr)
        {
        }

        // Machines are chosen per track; the UI keeps them equal across the
        // track's voices, so the first voice's config stands for the track.
        //
        void SetMachines()
        {
            m_sourceInput.m_sourceMachine = m_voiceConfig[0]->m_sourceMachine;
            m_shapeInput.m_sourceMachine = m_voiceConfig[0]->m_sourceMachine;
            m_shapeInput.m_filterMachine = m_voiceConfig[0]->m_filterMachine;
            m_ampInput.m_sourceMachine = m_voiceConfig[0]->m_sourceMachine;
            for (size_t i = 0; i < Size; ++i)
            {
                m_sourceInput.m_sourceAssignment[i] = m_voiceConfig[i]->m_sourceAssignment;
                m_sourceInput.m_audioBufferBank[i] = m_voiceConfig[i]->m_audioBufferBank;
            }
        }

        void SetGates()
        {
            for (size_t i = 0; i < Size; ++i)
            {
                m_ampInput.m_ahdInput.Set(i, m_ahdControl[i]);
                m_ampInput.m_modulationAHDInput.Set(i, m_ahdControl[i]);
                m_sourceInput.m_physicalModelingInput.m_ahdInput.Set(i, m_ahdControl[i]);

                m_squiggleLFOInput[0].m_polyXFaderInput.m_trig[i] = m_ahdControl[i].m_trig;
                m_squiggleLFOInput[1].m_polyXFaderInput.m_trig[i] = m_ahdControl[i].m_trig;
            }
        }

        void SetTheoryOfTimeInputs(TheoryOfTime* theoryOfTime)
        {
            m_sourceInput.m_theoryOfTime = theoryOfTime;
        }
    };

    SquiggleBoyTrackGeneric()
        : m_baseFreqSlewAmount(SampleTimer::HzToCycPerSamp(0.125), SampleTimer::HzToCycPerSamp(500.0))
        , m_output(Vector::Splat(0))
        , m_recordingSubSample(0)
    {
    }

    void DebugPrint()
    {
        m_shape.DebugPrint();
    }

    // targetFreq is the note frequency per voice in cycles per sample before
    // portamento; gainFader is the track fader.
    //
    void SetEncoderParams(
        SmartGridOneEncoders& encoders,
        Input& input,
        size_t track,
        const Vector& targetFreq,
        const Vector& gainFader)
    {
        Vector slewFrequency = m_baseFreqSlewAmount.Update(
            Vector::Splat(1) - encoders.GetValue<Size>(Param::Portamento, track));
        m_baseFreqSlew.m_alpha = OPLowPassFilterGeneric<float, Size>::AlphaFromNatFreqApprox(slewFrequency).template Convert<double>();
        Vector baseFreq = m_baseFreqSlew.Process(targetFreq.template Convert<double>()).template Convert<float>();

        input.SetMachines();
        m_source.SetEncoderParams(
            encoders,
            input.m_sourceInput,
            track,
            baseFreq,
            m_gangedRandomLFO[2].GetOutput(),
            m_gangedRandomLFO[3].GetOutput());

        for (size_t i = 0; i < Size; ++i)
        {
            AudioBufferBank* audioBufferBank = input.m_voiceConfig[i]->m_audioBufferBank;
            if (audioBufferBank)
            {
                audioBufferBank->m_bankPosition = std::clamp(
                    encoders.GetValue(Param::SampleBankPosition, static_cast<int>(track * Size + i)), 0.0f, 1.0f);
            }
        }

        input.m_subInput.m_baseFreq = baseFreq;

        input.m_ampInput.m_subGain.Update(encoders.GetValue<Size>(Param::SubOscillator, track));
        input.m_ampInput.m_subTanhGain.Update(encoders.GetValue<Size>(Param::SubTanhGain, track));
        input.m_ampInput.m_subRiseSeconds.Update(encoders.GetValue<Size>(Param::SubSlewRise, track));
        input.m_ampInput.m_subFallSeconds.Update(encoders.GetValue<Size>(Param::SubSlewFall, track));

        m_shape.SetEncoderParams(encoders, input.m_shapeInput, track, baseFreq);

        input.m_ampInput.SetModulationAHD(
            encoders.GetValue<Size>(Param::ModAHDAttack, track),
            encoders.GetValue<Size>(Param::ModAHDHold, track),
            encoders.GetValue<Size>(Param::ModAHDDecay, track),
            encoders.GetValue<Size>(Param::ModAmplitude, track));

        input.m_ampInput.m_vcoTargetFreq = targetFreq;
        input.m_ampInput.SetAHD(
            encoders.GetValue<Size>(Param::AmpAHDAttack, track),
            encoders.GetValue<Size>(Param::AmpAHDHold, track),
            encoders.GetValue<Size>(Param::AmpAHDDecay, track),
            encoders.GetValue<Size>(Param::AmpAmplitude, track));

        input.m_ampInput.m_gain.Update(gainFader);

        // Voices spread around the pan circle by their place in the track,
        // and tracks offset within that spacing.
        //
        Vector staticOffset;
        Vector lfoPhaseSpread;
        for (size_t i = 0; i < Size; ++i)
        {
            lfoPhaseSpread[i] = static_cast<float>(i) / Size;
            staticOffset[i] = lfoPhaseSpread[i] + static_cast<float>(track) / (Size * Size);
        }

        input.m_panInput.m_radius.Update(encoders.GetValue<Size>(Param::PanRadius, track));
        input.m_panInput.m_phaseShift = staticOffset + encoders.GetValue<Size>(Param::PanPhaseShift, track);
        input.m_panInput.m_multX = encoders.GetValue<Size>(Param::PanMultX, track) * 4 + 1;
        input.m_panInput.m_multY = encoders.GetValue<Size>(Param::PanMultY, track) * 4 + 1;
        input.m_panInput.m_centerX = encoders.GetValue<Size>(Param::PanCenterX, track) * 2 - 1;
        input.m_panInput.m_centerY = encoders.GetValue<Size>(Param::PanCenterY, track) * 2 - 1;

        static constexpr Param x_lfoParams[2][8] =
        {
            {
                Param::LFO1Skew,
                Param::LFO1Mult,
                Param::LFO1Shape,
                Param::LFO1Amplitude,
                Param::LFO1Center,
                Param::LFO1Slope,
                Param::LFO1PhaseShift,
                Param::LFO1SampleAndHold
            },
            {
                Param::LFO2Skew,
                Param::LFO2Mult,
                Param::LFO2Shape,
                Param::LFO2Amplitude,
                Param::LFO2Center,
                Param::LFO2Slope,
                Param::LFO2PhaseShift,
                Param::LFO2SampleAndHold
            }
        };

        for (size_t lfo = 0; lfo < 2; ++lfo)
        {
            const Param* params = x_lfoParams[lfo];
            typename SquiggleLFO::Input& lfoInput = input.m_squiggleLFOInput[lfo];
            lfoInput.m_polyXFaderInput.m_attackFrac = encoders.GetValue<Size>(params[0], track);
            lfoInput.m_mult.Update(encoders.GetValue<Size>(params[1], track));
            lfoInput.m_polyXFaderInput.m_shape = encoders.GetValue<Size>(params[2], track);
            lfoInput.m_polyXFaderInput.m_amplitude = encoders.GetValue<Size>(params[3], track);
            lfoInput.m_polyXFaderInput.m_center = Vector::Splat(1) - encoders.GetValue<Size>(params[4], track);
            lfoInput.m_polyXFaderInput.m_slope = encoders.GetValue<Size>(params[5], track);
            lfoInput.m_polyXFaderInput.m_phaseShift = encoders.GetValue<Size>(params[6], track) * lfoPhaseSpread;
            lfoInput.m_polyXFaderInput.m_phaseDomain = PhaseDomain::Modulated;
            lfoInput.m_polyXFaderInput.m_shFade = encoders.GetValue<Size>(params[7], track);
        }

        input.SetGates();
    }

    Vector Processs(Input& input)
    {
        for (size_t i = 0; i < x_numGangedRandomLFOs; ++i)
        {
            m_gangedRandomLFO[i].Process(SampleTimer::x_samplePeriod, input.m_gangedRandomLFOInput[i]);
        }

        input.SetMachines();

        Vector source = m_source.Process(input.m_sourceInput);
        Vector shaped = m_shape.Process(input.m_shapeInput, source);
        m_shape.WriteScopes(m_source.m_top);

        Vector sub = m_sub.Process(input.m_subInput.m_baseFreq * 0.5f);
        m_output = m_amp.Process(input.m_ampInput, shaped, sub);
        m_pan.Process(input.m_panInput);
        m_squiggleLFO[0].Process(input.m_squiggleLFOInput[0]);
        m_squiggleLFO[1].Process(input.m_squiggleLFOInput[1]);

        Vector recorded = m_recordingDecimator.Process(m_output);
        ++m_recordingSubSample;
        if (m_recordingSubSample == SampleTimer::x_oversample)
        {
            m_recordingSubSample = 0;
            for (size_t i = 0; i < Size; ++i)
            {
                m_recordingBufferWriter[i].Process(recorded[i]);
            }
        }

        return m_output;
    }
};

using SquiggleBoyTrack = SquiggleBoyTrackGeneric<3>;

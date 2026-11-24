#pragma once

#include "QuadUtils.hpp"
#include "AdaptiveWaveTable.hpp"
#include "Q.hpp"
#include "Slew.hpp"
#include "AnalyzePVDR32.hpp"
#include "AnalyzePVDR64.hpp"
#include "SynthesizeSpectrum32.hpp"
#include "SynthesizeSpectrum64.hpp"
#include "ResynthesisFrame32.hpp"
#include "ResynthesisFrame64.hpp"
#include "GrainControl32.hpp"
#include "UnisonBase32.hpp"
#include "UnisonBase64.hpp"
#include "Math.hpp"
#include "Array.hpp"
#include "MultiChannelComplex.hpp"
#include "NormGen.hpp"
#include "PriorityQueue.hpp"
#include "SampleTimer.hpp"
#include "SpectralModel.hpp"

// Controls for one channel. Every channel count shares this type, and a
// multichannel resynthesizer takes one per channel.
//
struct ResynthesizerInput
{
    static constexpr float x_immediateSlewUp = 0.5f;
    static constexpr size_t x_numShifts = 2;

    Q m_shift[x_numShifts - 1];
    float m_fade[x_numShifts - 1];
    float m_unisonGain;
    float m_unisonDetune;
    float m_slewUp;
    float m_slewDown;
    float m_rmsThreshold;
    float m_rmsQuiet;
    float m_rmsLoud;
    float m_loudShift;

    struct OscillatorControl
    {
        bool m_gain;
        float m_value;
        size_t m_oscillator;
    };

    float GetUnisonGainForOscillator(int index) const
    {
        return GrainControl32::Evaluate(OscillatorControl{true, m_unisonGain, static_cast<size_t>(index)});
    }

    float GetUnisonDetune(int index) const
    {
        return GrainControl32::Evaluate(OscillatorControl{false, m_unisonDetune, static_cast<size_t>(index)});
    }

    ResynthesizerInput()
        : m_unisonGain(0.0)
        , m_unisonDetune(1.0)
        , m_slewUp(x_immediateSlewUp)
        , m_slewDown(0.5)
        , m_rmsThreshold(1.0)
        , m_rmsQuiet(0)
        , m_rmsLoud(1)
        , m_loudShift(0.0)
    {
        for (size_t i = 0; i < x_numShifts - 1; ++i)
        {
            m_shift[i] = Q(1, 1);
            m_fade[i] = 0.0f;
        }
    }
};

// Phase vocoder resynthesis of Size independent channels. Transforms and per-bin
// analysis run across channels at once; phase propagation order and shifted
// synthesis depend on each channel's spectrum and run per channel.
//
template<typename Number, size_t Size>
struct ResynthesizerGeneric
{
    using Vector = MultiChannelNumber<Number, Size>;
    using DoubleVector = MultiChannelNumber<double, Size>;
    using Input = ResynthesizerInput;

    static constexpr size_t x_bits = 12;
    static constexpr size_t x_hopDenom = 4;
    static constexpr size_t x_numOscillators = 3;
    static constexpr float x_immediateSlewUp = ResynthesizerInput::x_immediateSlewUp;
    using Buffer = BasicWaveTableGeneric<Number, Size, x_bits>;
    using DFT = DiscreteFourierTransformGeneric<Number, Size, x_bits>;
    using ComplexVector = typename DFT::ComplexVector;
    static constexpr size_t x_tableSize = Buffer::x_tableSize;
    static constexpr size_t x_maxComponents = DFT::x_maxComponents;
    static constexpr size_t x_N = x_tableSize;
    static constexpr size_t x_H = x_N / x_hopDenom;
    static_assert(x_N == 4096 && x_H == 1024 && Size <= 4);

    static Vector GetUnisonBase()
    {
        struct InputView
        {
            size_t m_count;
        };

        using Kernel = std::conditional_t<std::is_same_v<Number, float>, UnisonBase32, UnisonBase64>;
        const auto values = Kernel::Evaluate(InputView{Size});
        Vector result;
        for (size_t lane = 0; lane < Size; ++lane)
        {
            result[lane] = values.m_lanes[lane];
        }

        return result;
    }

    // Analysis runs on host-rate buffers. Synthesis interpolates the same
    // spectrum to the engine rate, so a frame covers the analysis window's
    // duration in engine samples.
    //
    static constexpr size_t x_synthesisBits = x_bits + SampleTimer::x_oversampleBits;
    using SynthesisFrame = BasicWaveTableGeneric<Number, Size, x_synthesisBits>;
    static constexpr size_t x_synthesisFrameSize = SynthesisFrame::x_tableSize;

    // Phase propagation order for one channel.
    //
    struct PVDR
    {
        using Owner = ResynthesizerGeneric;

        enum class GateState : uint8_t
        {
            Low,
            Triggered,
            High
        };

        struct Result
        {
            int16_t m_bin;
            int16_t m_parent;
            double m_delta;
            bool m_small;

            Result()
                : m_bin(0)
                , m_parent(0)
                , m_delta(0.0f)
                , m_small(false)
            {
            }
        };

        struct Entry
        {
            int16_t m_bin;
            bool m_isPrev;
            Number m_magnitude;

            Entry()
                : m_bin(0)
                , m_isPrev(false)
                , m_magnitude(0.0f)
            {
            }

            Entry(int16_t bin, bool isPrev, Number magnitude)
                : m_bin(bin)
                , m_isPrev(isPrev)
                , m_magnitude(magnitude)
            {
            }

            bool operator<(const Entry& other) const
            {
                return m_magnitude < other.m_magnitude;
            }
        };

        template<typename Draw>
        struct Storage
        {
            struct LinkValue
            {
                size_t m_root;
                double m_delta;
                bool m_small;
            };

            struct EntryValue
            {
                size_t m_bin;
                bool m_previous;
                Number m_magnitude;
            };

            struct PopResult
            {
                bool m_present;
                EntryValue m_value;
            };

            PVDR& m_tracker;
            Draw& m_draw;
            bool m_computed[x_maxComponents - 1]{};
            PriorityQueue<Entry, 2 * x_maxComponents> m_queue;

            Number Magnitude(size_t index) const
            {
                return m_tracker.m_owner->AnalysisMagnitudes()[index + 1][m_tracker.m_channel];
            }

            Number PreviousMagnitude(size_t index) const
            {
                return m_tracker.m_owner->m_prevAnalysisMagnitudes[index + 1][m_tracker.m_channel];
            }

            Number Phase(size_t index) const
            {
                return m_tracker.m_owner->m_analysisPhase[index + 1][m_tracker.m_channel];
            }

            double Omega(size_t index) const
            {
                return m_tracker.m_owner->m_omegaInstantaneous[index + 1][m_tracker.m_channel];
            }

            unsigned Gate(size_t index) const
            {
                return static_cast<unsigned>(m_tracker.m_gate[index + 1]);
            }

            void SetGate(size_t index, unsigned gate)
            {
                m_tracker.m_gate[index + 1] = static_cast<GateState>(gate);
            }

            bool Computed(size_t index) const
            {
                return m_computed[index];
            }

            LinkValue Link(size_t index) const
            {
                const auto& result = m_tracker.m_results[m_tracker.m_resultIndices[index + 1]];
                return {static_cast<size_t>(result.m_parent) - 1, result.m_delta, result.m_small};
            }

            void Put(size_t index, LinkValue link)
            {
                const auto bin = index + 1;
                const auto slot = m_tracker.m_size;
                auto& result = m_tracker.m_results[slot];
                result.m_bin = static_cast<int16_t>(bin);
                result.m_parent = static_cast<int16_t>(link.m_root + 1);
                result.m_delta = link.m_delta;
                result.m_small = link.m_small;
                m_tracker.m_resultIndices[bin] = static_cast<uint16_t>(slot);
                ++m_tracker.m_size;
                m_computed[index] = true;
            }

            void Push(EntryValue entry)
            {
                m_queue.Push(Entry(static_cast<int16_t>(entry.m_bin + 1), entry.m_previous, entry.m_magnitude));
            }

            PopResult Pop()
            {
                if (m_queue.IsEmpty())
                {
                    return {false, {}};
                }

                const auto entry = m_queue.Pop();
                return {true, {static_cast<size_t>(entry.m_bin) - 1, entry.m_isPrev, entry.m_magnitude}};
            }

            Number NextPhase()
            {
                return m_draw();
            }
        };

        template<typename Draw>
        void AnalyzeStorage(Draw draw, bool schmitt, double fixedFloor)
        {
            struct InputView
            {
                double m_hop;
                bool m_schmitt;
                double m_fixedFloor;
            };

            using Kernel = std::conditional_t<std::is_same_v<Number, float>, AnalyzePVDR32, AnalyzePVDR64>;
            Storage<Draw> storage{*this, draw, {}, {}};
            Kernel::Process(storage, InputView{static_cast<double>(x_H), schmitt, fixedFloor});
        }

        template<typename Draw>
        void AnalyzeStorage(Draw draw)
        {
            AnalyzeStorage(draw, true, 0.0);
        }

        void AnalyzeWithFloor(double floor)
        {
            RGen random;
            AnalyzeStorage([&]() { return random.UniGenRange(-0.5f, 0.5f); }, false, floor);
        }

        void Analyze()
        {
            RGen random;
            AnalyzeStorage([&]() { return random.UniGenRange(-0.5f, 0.5f); });
        }

        bool IsLeader(int16_t bin)
        {
            return m_results[m_resultIndices[bin]].m_parent == bin && !m_results[m_resultIndices[bin]].m_small;
        }

        PVDR()
            : PVDR(nullptr, 0)
        {
        }

        PVDR(ResynthesizerGeneric* owner, size_t channel)
            : m_size(0)
            , m_owner(owner)
            , m_channel(channel)
        {
        }

        // Clear the per-grain graph while preserving the Schmitt history.
        //
        void Clear()
        {
            m_size = 0;
        }

        GateState m_gate[x_maxComponents]{};
        Result m_results[x_maxComponents];
        uint16_t m_resultIndices[x_maxComponents];
        size_t m_size;
        ResynthesizerGeneric* m_owner;
        size_t m_channel;
    };

    struct Oscillator
    {
        explicit Oscillator(ResynthesizerGeneric* owner)
            : m_owner(owner)
        {
        }

        void FixupPhases(const Vector& detune, PVDR* trackers)
        {
            m_owner->RunSpectrum(SpectrumStage::Fixup, *this, m_owner->m_synthDft, trackers, nullptr, detune);
        }

        void SynthesizeNoShift(DFT& dft, const Vector& gain)
        {
            m_owner->RunSpectrum(SpectrumStage::Dry, *this, dft, m_owner->m_pvdr, nullptr, gain);
        }

        DoubleVector m_synthesisPhase[x_maxComponents];
        ResynthesizerGeneric* m_owner;
    };

    struct SpectrumStorage
    {
        struct PitchValue
        {
            int64_t m_numerator;
            int64_t m_denominator;
        };

        struct ControlValue
        {
            PitchValue m_shift;
            Number m_fade;
            Number m_unisonGain;
            Number m_unisonDetune;
            Number m_slewUp;
        };

        struct LinkValue
        {
            size_t m_root;
            double m_delta;
            bool m_small;
        };

        struct VisitValue
        {
            size_t m_bin;
            LinkValue m_link;
        };

        struct CoefficientValue
        {
            Number m_real;
            Number m_imag;
        };

        ResynthesizerGeneric& m_owner;
        const Input* m_controls;
        DFT& m_output;
        PVDR* m_trackers;
        const Vector& m_drive;

        ControlValue Controls(size_t lane) const
        {
            const auto& control = m_controls[lane];
            return {{control.m_shift[0].m_num, control.m_shift[0].m_denom}, control.m_fade[0],
                control.m_unisonGain, control.m_unisonDetune, control.m_slewUp};
        }

        size_t GraphSize(size_t lane) const
        {
            return m_trackers[lane].m_size;
        }

        VisitValue VisitAt(size_t lane, size_t index) const
        {
            const auto& result = m_trackers[lane].m_results[index];
            return {static_cast<size_t>(result.m_bin) - 1,
                {static_cast<size_t>(result.m_parent) - 1, result.m_delta, result.m_small}};
        }

        unsigned Gate(size_t lane, size_t bin) const
        {
            return static_cast<unsigned>(m_trackers[lane].m_gate[bin + 1]);
        }

        Number AnalysisPhase(size_t lane, size_t bin) const
        {
            return m_owner.m_analysisPhase[bin + 1][lane];
        }

        double Omega(size_t lane, size_t bin) const
        {
            return m_owner.m_omegaInstantaneous[bin + 1][lane];
        }

        void SetOmega(size_t lane, size_t bin, double omega)
        {
            m_owner.m_omegaInstantaneous[bin + 1][lane] = omega;
        }

        Number SynthesisMagnitude(size_t lane, size_t bin) const
        {
            return m_owner.SynthesisMagnitudes()[bin + 1][lane];
        }

        double Phase(size_t lane, size_t oscillator, size_t bin) const
        {
            return m_owner.m_oscillators[oscillator].m_synthesisPhase[bin + 1][lane];
        }

        void SetPhase(size_t lane, size_t oscillator, size_t bin, double phase)
        {
            m_owner.m_oscillators[oscillator].m_synthesisPhase[bin + 1][lane] = phase;
        }

        void ClearOutput()
        {
            m_output.Init();
        }

        CoefficientValue Coefficient(size_t lane, size_t bin) const
        {
            const auto& value = m_output.m_components[bin + 1];
            return {value.m_real[lane], value.m_imag[lane]};
        }

        void SetCoefficient(size_t lane, size_t bin, CoefficientValue value)
        {
            auto& coefficient = m_output.m_components[bin + 1];
            coefficient.m_real[lane] = value.m_real;
            coefficient.m_imag[lane] = value.m_imag;
        }

        static CoefficientValue Polar(Number radius, double angle)
        {
            const auto value = Math::Polar2pi(static_cast<float>(radius), angle);
            return {value.real(), value.imag()};
        }

        static int64_t Round(double value)
        {
            // The next Lean operation clamps to the positive spectrum bounds.
            // Saturate before the native integer conversion.
            //
            const double extent = static_cast<double>(x_tableSize);
            const double rounded = std::isnan(value) ? 0.0 : std::round(value);
            return static_cast<int64_t>(std::clamp(rounded, -extent, extent));
        }

        Number Drive(size_t lane) const
        {
            return m_drive[lane];
        }

        static double Ratio(PitchValue shift)
        {
            return static_cast<double>(shift.m_numerator) / static_cast<double>(shift.m_denominator);
        }
    };

    enum class SpectrumStage : unsigned
    {
        Spectrum,
        Fixup,
        Dry
    };

    void RunSpectrum(SpectrumStage stage, Oscillator& oscillator, DFT& output, PVDR* trackers,
        const Input* controls, const Vector& drive)
    {
        struct InputView
        {
            unsigned m_stage;
            size_t m_count;
            size_t m_oscillator;
        };

        static_assert(Size <= 4);
        using Kernel = std::conditional_t<std::is_same_v<Number, float>, SynthesizeSpectrum32, SynthesizeSpectrum64>;
        SpectrumStorage storage{*this, controls, output, trackers, drive};
        Kernel::Process(storage, InputView{static_cast<unsigned>(stage), Size,
            static_cast<size_t>(&oscillator - m_oscillators)});
    }

    struct FramePorts
    {
        Buffer* m_previous;
        Buffer* m_current;
        DFT* m_dft;
        SynthesisFrame* m_frame;
        const Input* m_controls;
        PVDR* m_trackers;
    };

    struct FrameStorage
    {
        struct MeasurementValue
        {
            Number m_magnitude;
            Number m_phase;
        };

        ResynthesizerGeneric& m_owner;
        FramePorts m_ports;

        void FFT(bool previous)
        {
            auto& buffer = previous ? *m_ports.m_previous : *m_ports.m_current;
            m_ports.m_dft->Transform(buffer, m_owner.m_workspace);
        }

        void IFFT()
        {
            m_owner.m_synthDft.InverseTransform(*m_ports.m_frame, x_maxComponents, m_owner.m_workspace);
        }

        MeasurementValue Measurement(size_t lane, size_t bin) const
        {
            const auto component = m_ports.m_dft->m_components[bin + 1][lane];
            const auto real = component.real();
            const auto imag = component.imag();
            return {std::sqrt(real * real + imag * imag),
                static_cast<Number>(static_cast<double>(std::atan2(imag, real)) / (2.0 * M_PI))};
        }

        Number PreviousPhase(size_t lane, size_t bin) const
        {
            return m_owner.m_prevAnalysisPhase[bin + 1][lane];
        }

        MeasurementValue Current(size_t lane, size_t bin) const
        {
            return {m_owner.AnalysisMagnitudes()[bin + 1][lane], m_owner.m_analysisPhase[bin + 1][lane]};
        }

        void SetPrevious(size_t lane, size_t bin, MeasurementValue value)
        {
            m_owner.m_prevAnalysisMagnitudes[bin + 1][lane] = value.m_magnitude;
            m_owner.m_prevAnalysisPhase[bin + 1][lane] = value.m_phase;
        }

        void SetCurrent(size_t lane, size_t bin, MeasurementValue value, double frequency)
        {
            m_owner.AnalysisMagnitudes()[bin + 1][lane] = value.m_magnitude;
            m_owner.m_analysisPhase[bin + 1][lane] = value.m_phase;
            m_owner.m_omegaInstantaneous[bin + 1][lane] = frequency;
        }

        Number Magnitude(size_t lane, size_t bin) const
        {
            return m_owner.SynthesisMagnitudes()[bin + 1][lane];
        }

        void SetMagnitude(size_t lane, size_t bin, Number value)
        {
            m_owner.SynthesisMagnitudes()[bin + 1][lane] = value;
        }

        Number SlewCutoff(size_t lane) const
        {
            return m_owner.m_slewCutoff[lane];
        }

        void PrepareSlew(size_t lane)
        {
            m_owner.m_slewCutoff[lane] = m_ports.m_controls[lane].m_slewUp;
        }

        static Number SlewCoefficient(Number cutoff)
        {
            using Filter = OPLowPassFilterGeneric<Number, 1>;
            return Filter::AlphaFromNatFreqApprox(Filter::Vector::Splat(cutoff))[0];
        }

        Number GetSample(size_t lane, size_t sample) const
        {
            return m_ports.m_frame->m_table[sample][lane];
        }

        void PutSample(size_t lane, size_t sample, Number value)
        {
            m_ports.m_frame->m_table[sample][lane] = value;
        }

        static Number Window(size_t sample)
        {
            return MathGeneric<x_synthesisBits>::Hann(sample);
        }

        void AnalyzeGraph(size_t lane)
        {
            auto& tracker = m_ports.m_trackers[lane];
            tracker.Clear();
            tracker.Analyze();
        }

        void Synthesize(size_t count)
        {
            assert(count == Size);
            static_cast<void>(count);
            m_owner.RunSpectrum(SpectrumStage::Spectrum, m_owner.m_oscillators[0], m_owner.m_synthDft,
                m_ports.m_trackers, m_ports.m_controls, Vector::Splat(0));
        }

        template<typename Table>
        static void FillLane(Table& table, size_t lane)
        {
            for (auto& value : table)
            {
                value[lane] = 0;
            }
        }

        void ClearAnalysisMagnitude(size_t lane)
        {
            FillLane(m_owner.AnalysisMagnitudes(), lane);
        }

        void ClearPreviousMagnitude(size_t lane)
        {
            FillLane(m_owner.m_prevAnalysisMagnitudes, lane);
        }

        void ClearAnalysisPhase(size_t lane)
        {
            FillLane(m_owner.m_analysisPhase, lane);
        }

        void ClearPreviousPhase(size_t lane)
        {
            FillLane(m_owner.m_prevAnalysisPhase, lane);
        }

        void ClearOmega(size_t lane)
        {
            FillLane(m_owner.m_omegaInstantaneous, lane);
        }

        void ClearOscillatorPhase(size_t lane, size_t oscillator)
        {
            FillLane(m_owner.m_oscillators[oscillator].m_synthesisPhase, lane);
        }

        void ClearGates(size_t lane)
        {
            std::fill_n(m_ports.m_trackers[lane].m_gate, x_maxComponents, PVDR::GateState::Low);
        }

        void ClearOrder(size_t lane)
        {
            m_ports.m_trackers[lane].Clear();
        }

        void ClearMagnitude(size_t lane)
        {
            FillLane(m_owner.SynthesisMagnitudes(), lane);
        }

        void SetSlewCutoff(size_t lane, Number cutoff)
        {
            m_owner.m_slewCutoff[lane] = cutoff;
        }
    };

    enum class FrameStage : unsigned
    {
        Process,
        Synthesize,
        Analyze,
        Prime,
        PrimeSpectrum,
        Measure,
        Copy,
        UpdateFrequency,
        Clear,
        ClearChannel,
        RenderCurrent
    };

    void RunFrame(FrameStage stage, FramePorts ports, size_t lane, size_t bin)
    {
        struct InputView
        {
            unsigned m_stage;
            size_t m_count;
            size_t m_frameSize;
            size_t m_lane;
            size_t m_bin;
        };

        using Kernel = std::conditional_t<std::is_same_v<Number, float>, ResynthesisFrame32, ResynthesisFrame64>;
        FrameStorage storage{*this, ports};
        Kernel::Process(storage, InputView{static_cast<unsigned>(stage), Size, x_synthesisFrameSize, lane, bin});
    }

    void ClearChannel(size_t channel)
    {
        RunFrame(FrameStage::ClearChannel, {nullptr, nullptr, nullptr, nullptr, nullptr, m_pvdr}, channel, 0);
    }

    void Clear()
    {
        RunFrame(FrameStage::Clear, {nullptr, nullptr, nullptr, nullptr, nullptr, m_pvdr}, 0, 0);
    }

    ResynthesizerGeneric()
        : m_oscillators{Oscillator(this), Oscillator(this), Oscillator(this)}
    {
        for (size_t channel = 0; channel < Size; ++channel)
        {
            m_pvdr[channel].m_owner = this;
            m_pvdr[channel].m_channel = channel;
        }

        Clear();
    }

    ResynthesizerGeneric(const ResynthesizerGeneric&) = delete;
    ResynthesizerGeneric& operator=(const ResynthesizerGeneric&) = delete;

    void SetSlewUp(const Vector& cyclesPerSample)
    {
        m_slewCutoff = cyclesPerSample;
    }

    void PrimePhases(DFT& dft)
    {
        RunFrame(FrameStage::PrimeSpectrum, {nullptr, nullptr, &dft, nullptr, nullptr, m_pvdr}, 0, 0);
    }

    void PrimeFromPrevious()
    {
        RunFrame(FrameStage::Copy, {nullptr, nullptr, nullptr, nullptr, nullptr, m_pvdr}, 0, 0);
    }

    void ProcessPhases(DFT& dft)
    {
        RunFrame(FrameStage::Measure, {nullptr, nullptr, &dft, nullptr, nullptr, m_pvdr}, 0, 0);
    }

    static double PrincArg(double arg)
    {
        double wrapped = arg - std::floor(arg);
        return 0.5 < wrapped ? wrapped - 1.0 : wrapped;
    }

    void SetAnalysisOmega(int bin)
    {
        RunFrame(FrameStage::UpdateFrequency, {nullptr, nullptr, nullptr, nullptr, nullptr, m_pvdr},
            0, static_cast<size_t>(bin - 1));
    }

    void Analyze(PVDR* trackers, Buffer& buffer)
    {
        RunFrame(FrameStage::Analyze, {nullptr, &buffer, &m_analysisDft, nullptr, nullptr, trackers}, 0, 0);
    }

    void PrimeAnalysis(Buffer& waveTable)
    {
        RunFrame(FrameStage::Prime, {&waveTable, nullptr, &m_analysisDft, nullptr, nullptr, m_pvdr}, 0, 0);
    }

    void Synthesize(Buffer& buffer, SynthesisFrame& frame, Input* input)
    {
        RunFrame(FrameStage::RenderCurrent, {nullptr, &buffer, &m_analysisDft, &frame, input, m_pvdr}, 0, 0);
    }

    void SynthesizeSpectrum(const Input* input)
    {
        RunFrame(FrameStage::Synthesize, {nullptr, nullptr, nullptr, nullptr, input, m_pvdr}, 0, 0);
    }

    void Process(Buffer& previousWaveTable, Buffer& buffer, SynthesisFrame& frame, Input* input)
    {
        RunFrame(FrameStage::Process, {&previousWaveTable, &buffer, &m_analysisDft, &frame, input, m_pvdr}, 0, 0);
    }

    void PrintTopStuff()
    {
        for (size_t channel = 0; channel < Size; ++channel)
        {
            double maxMag = 0.0;
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                maxMag = std::max<double>(maxMag, SynthesisMagnitudes()[i][channel]);
            }

            if (maxMag < 1e-4)
            {
                continue;
            }

            INFO("========================");
            for (size_t i = 1; i < x_maxComponents; ++i)
            {
                if (SynthesisMagnitudes()[i][channel] > maxMag / 10)
                {
                    INFO("%p channel %zu High bin: %zu, mag: %f, omega instantaneous: %f, an phase: %f, synth phase: %f", 
                        this, channel, i, SynthesisMagnitudes()[i][channel],
                        m_omegaInstantaneous[i][channel], m_analysisPhase[i][channel], m_oscillators[0].m_synthesisPhase[i][channel]);
                }
            }
        }
    }

    // Analysis hop in host samples.
    //
    static constexpr size_t GetGrainLaunchSamples()
    {
        return x_H;
    }

    // Synthesis hop in engine samples: the analysis hop's duration.
    //
    static constexpr size_t GetSynthesisHopSamples()
    {
        return x_H * SampleTimer::x_oversample;
    }

    auto& SynthesisMagnitudes()
    {
        return m_synthesisMagnitudes;
    }

    const auto& SynthesisMagnitudes() const
    {
        return m_synthesisMagnitudes;
    }

    auto& AnalysisMagnitudes()
    {
        return m_analysisMagnitudes;
    }

    const auto& AnalysisMagnitudes() const
    {
        return m_analysisMagnitudes;
    }

    std::array<Vector, x_maxComponents> m_synthesisMagnitudes{};
    std::array<Vector, x_maxComponents> m_analysisMagnitudes{};
    Vector m_slewCutoff{};
    Vector m_prevAnalysisMagnitudes[x_maxComponents];
    DoubleVector m_omegaInstantaneous[x_maxComponents];
    Vector m_analysisPhase[x_maxComponents];
    Vector m_prevAnalysisPhase[x_maxComponents];
    Oscillator m_oscillators[x_numOscillators];
    PVDR m_pvdr[Size];
    DFT m_analysisDft;
    DFT m_synthDft;
    // Shared by the analysis transform and the larger synthesis transform.
    //
    ComplexVector m_workspace[x_synthesisFrameSize];
};

using Resynthesizer = ResynthesizerGeneric<float, 1>;

#include "SpectralModel.hpp"

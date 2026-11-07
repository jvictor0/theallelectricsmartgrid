#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "Resynthesis.hpp"
#include "OLA.hpp"

namespace
{
constexpr int x_rate = 48000;
constexpr double x_pi = 3.14159265358979323846;
constexpr int x_onset = 8192;

struct Configuration
{
    std::string m_mode;
    std::string m_initial;
    std::string m_signal;
    double m_frequency;
    int m_noteSamples;
    int m_attackSamples;
    int m_delaySamples;
    float m_feedback;
    std::string m_pattern;
    unsigned m_seed;
    std::string m_prefix;
};

float DrySample(int n, const Configuration& config)
{
    int relative = n - x_onset;
    int repeats = config.m_pattern == "single" ? 1 : 4;
    int interval = 2 * config.m_noteSamples;
    double sample = 0;
    for (int note = 0; note < repeats; ++note)
    {
        int position = relative - note * interval;
        if (position < 0 || position >= config.m_noteSamples)
        {
            continue;
        }

        double envelope = 1;
        if (position < config.m_attackSamples)
        {
            envelope *= 0.5 - 0.5 * std::cos(x_pi * position / config.m_attackSamples);
        }

        int remaining = config.m_noteSamples - position;
        if (remaining < config.m_attackSamples)
        {
            envelope *= 0.5 - 0.5 * std::cos(x_pi * remaining / config.m_attackSamples);
        }

        double carrier = 0;
        int harmonics = config.m_signal == "harmonic" ? 8 : 1;
        double normalization = 0;
        for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
        {
            double amplitude = 1.0 / harmonic;
            carrier += amplitude * std::cos(2 * x_pi * (config.m_frequency * harmonic * relative / x_rate + 0.137 * harmonic));
            normalization += amplitude;
        }

        double polarity = config.m_pattern == "alternating" && note % 2 == 1 ? -1 : 1;
        sample += 0.2 * polarity * envelope * carrier / normalization;
    }

    return static_cast<float>(sample);
}

void WriteAudio(const std::string& path, const std::vector<float>& samples)
{
    std::ofstream stream(path, std::ios::binary);
    assert(stream.good());
    stream.write(reinterpret_cast<const char*>(samples.data()), static_cast<std::streamsize>(samples.size() * sizeof(float)));
    assert(stream.good());
}

struct Experiment
{
    using Resynth = Resynthesizer;
    Resynth m_synth;
    Resynth::Buffer m_previous;
    Resynth::Buffer m_current;
    Resynth::SynthesisFrame m_frame;
    OLAGeneric<float, 1, Resynth::x_synthesisBits> m_ola;
    bool m_initialized = false;
    std::mt19937 m_initialRng;
    std::uniform_real_distribution<double> m_phaseDistribution;

    explicit Experiment(unsigned seed)
        : m_initialRng(seed)
        , m_phaseDistribution(-0.5, 0.5)
    {
        static_assert(SampleTimer::x_oversample == 1, "Neutral experiment uses the existing host-rate test configuration");
        m_synth.SetSlewUp(Resynth::Vector::Splat(Resynth::x_immediateSlewUp));
    }

    void Launch(int n, const Configuration& config, const std::vector<float>& tape, std::ofstream& diagnostics)
    {
        // GrainManager consumes OLA before launching. The +1 compensates that
        // sample of latency, making the conventional and spectral delays equal.
        //
        int start = n - config.m_delaySamples + 1;
        double energy = 0;
        for (size_t k = 0; k < Resynth::x_N; ++k)
        {
            int currentIndex = start + static_cast<int>(k);
            int previousIndex = currentIndex - static_cast<int>(Resynth::x_H);
            assert(currentIndex < n);
            float current = currentIndex >= 0 ? tape[currentIndex] : 0;
            float previous = previousIndex >= 0 ? tape[previousIndex] : 0;
            float window = Math4096::Hann(k);
            m_current.m_table[k] = current * window;
            m_previous.m_table[k] = previous * window;
            energy += current * current;
        }

        // This is the production neutral center-oscillator path, exposed here
        // between analysis and phase fixup to seed phases and measure errors.
        //
        m_synth.PrimeAnalysis(m_previous);
        m_synth.Analyze(m_synth.m_pvdr, m_current);
        auto& oscillator = m_synth.m_oscillators[0];
        if (!m_initialized && energy > 0)
        {
            for (size_t bin = 1; bin < Resynth::x_maxComponents; ++bin)
            {
                double offset = config.m_initial == "random" ? m_phaseDistribution(m_initialRng) : 0;
                oscillator.m_synthesisPhase[bin][0] = m_synth.m_prevAnalysisPhase[bin][0] + offset;
            }

            m_initialized = true;
        }

        if (config.m_mode == "oracle")
        {
            for (size_t bin = 1; bin < Resynth::x_maxComponents; ++bin)
            {
                oscillator.m_synthesisPhase[bin][0] = m_synth.m_analysisPhase[bin][0];
            }
        }
        else
        {
            oscillator.FixupPhases(Resynth::Vector::Splat(1), m_synth.m_pvdr);
        }

        double weightedError = 0;
        double totalEnergy = 0;
        double smallEnergy = 0;
        int triggers = 0;
        int triggeredFollowers = 0;
        int leaders = 0;
        auto& pvdr = m_synth.m_pvdr[0];
        for (size_t i = 0; i < pvdr.m_size; ++i)
        {
            const auto& result = pvdr.m_results[i];
            int bin = result.m_bin;
            double magnitude = m_synth.m_analysisMagnitudes[bin][0];
            double error = Resynth::PrincArg(oscillator.m_synthesisPhase[bin][0] - m_synth.m_analysisPhase[bin][0]);
            double weight = magnitude * magnitude;
            totalEnergy += weight;
            weightedError += weight * error * error;
            if (result.m_small)
            {
                smallEnergy += weight;
            }

            if (result.m_bin == result.m_parent && !result.m_small)
            {
                ++leaders;
            }

#ifdef PVDR_EXPERIMENT_SCHMITT
            if (pvdr.m_gate[bin] == Resynth::PVDR::GateState::Triggered)
            {
                if (result.m_bin == result.m_parent)
                {
                    ++triggers;
                }
                else
                {
                    ++triggeredFollowers;
                }
            }
#endif
        }

        diagnostics << n << ',' << totalEnergy << ',' << weightedError << ',' << smallEnergy << ','
                    << leaders << ',' << triggers << ',' << triggeredFollowers << '\n';
        m_synth.m_synthDft.Init();
        oscillator.SynthesizeNoShift(m_synth.m_synthDft, Resynth::Vector::Splat(1));
        m_synth.m_synthDft.InverseTransform(m_frame, Resynth::x_maxComponents, m_synth.m_workspace);
        MathGeneric<Resynth::x_synthesisBits>::Hann(m_frame);
        m_ola.Write(m_frame);
    }
};

void VerifyContracts()
{
    auto synth = std::make_unique<Resynthesizer>();
    std::mt19937 random(7531);
    std::uniform_real_distribution<float> phases(-0.5f, 0.5f);
    std::uniform_real_distribution<float> magnitudes(0.2f, 1.0f);
    for (size_t bin = 1; bin < Resynthesizer::x_maxComponents; ++bin)
    {
        synth->m_analysisPhase[bin] = phases(random);
        synth->m_analysisMagnitudes[bin] = magnitudes(random);
        synth->m_oscillators[0].m_synthesisPhase[bin][0] = synth->m_analysisPhase[bin][0];
    }

    // Arbitrary frame-to-frame phases and changing heap order exercise the
    // identity invariant without assuming slowly varying sinusoidal input.
    //
    for (int frame = 0; frame < 128; ++frame)
    {
        synth->PrimeFromPrevious();
        for (size_t bin = 1; bin < Resynthesizer::x_maxComponents; ++bin)
        {
            synth->m_analysisPhase[bin] = phases(random);
            synth->m_analysisMagnitudes[bin] = magnitudes(random);
            synth->SetAnalysisOmega(static_cast<int>(bin));
        }

        synth->m_pvdr[0].Clear();
        synth->m_pvdr[0].Analyze();
        synth->m_oscillators[0].FixupPhases(Resynthesizer::Vector::Splat(1), synth->m_pvdr);
        for (size_t bin = 1; bin < Resynthesizer::x_maxComponents; ++bin)
        {
            double error = Resynthesizer::PrincArg(synth->m_oscillators[0].m_synthesisPhase[bin][0] - synth->m_analysisPhase[bin][0]);
            assert(std::abs(error) < 2e-5);
        }
    }

#ifdef PVDR_EXPERIMENT_SCHMITT
    synth = std::make_unique<Resynthesizer>();
    using State = Resynthesizer::PVDR::GateState;
    const float levels[] = {0, 0.0005f, 0.00101f, 0.0008f, 0.0002512f, 0.00025f, 0.0009f, 0.0011f};
    const State expected[] = {State::Low, State::Low, State::Triggered, State::High, State::High, State::Low, State::Low, State::Triggered};
    constexpr int x_testBin = 200;
    synth->m_analysisMagnitudes[50] = 1;
    for (size_t frame = 0; frame < std::size(levels); ++frame)
    {
        synth->PrimeFromPrevious();
        synth->m_analysisMagnitudes[x_testBin] = levels[frame];
        synth->m_analysisPhase[x_testBin] = 0.37f;
        synth->SetAnalysisOmega(x_testBin);
        synth->m_pvdr[0].Clear();
        synth->m_pvdr[0].Analyze();
        assert(synth->m_pvdr[0].m_gate[x_testBin] == expected[frame]);
        synth->m_oscillators[0].FixupPhases(Resynthesizer::Vector::Splat(1), synth->m_pvdr);
        if (expected[frame] == State::Triggered)
        {
            assert(synth->m_pvdr[0].IsLeader(x_testBin));
            assert(synth->m_oscillators[0].m_synthesisPhase[x_testBin][0] == synth->m_analysisPhase[x_testBin][0]);
        }
    }
#endif
    std::cout << "Phase identity and applicable Schmitt contracts passed\n";
}
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--verify")
    {
        VerifyContracts();
        return 0;
    }

    if (argc != 12)
    {
        std::cerr << "mode initial signal hz note_samples attack_samples delay_samples feedback pattern seed output_prefix\n";
        return 2;
    }

    Configuration config{argv[1], argv[2], argv[3], std::stod(argv[4]), std::stoi(argv[5]),
                         std::stoi(argv[6]), std::stoi(argv[7]), std::stof(argv[8]), argv[9],
                         static_cast<unsigned>(std::stoul(argv[10])), argv[11]};
    assert(config.m_delaySamples > static_cast<int>(Resynthesizer::x_N));
    assert(config.m_attackSamples > 0 && 2 * config.m_attackSamples <= config.m_noteSamples);
    SampleTimer::Init(512);
    RGen::s_gen.seed(config.m_seed + 1);
    int sourceLength = config.m_pattern == "single" ? config.m_noteSamples : 7 * config.m_noteSamples;
    int samples = x_onset + sourceLength + 10 * config.m_delaySamples + static_cast<int>(Resynthesizer::x_N);
    std::vector<float> tape(samples, 0);
    std::vector<float> dry(samples, 0);
    std::vector<float> wet(samples, 0);
    auto experiment = std::make_unique<Experiment>(config.m_seed);
    std::ofstream diagnostics(config.m_prefix + ".csv");
    diagnostics << "sample,energy,weighted_phase_error,small_energy,leaders,triggers,triggered_followers\n";
    diagnostics << std::setprecision(12);
    for (int n = 0; n < samples; ++n)
    {
        dry[n] = DrySample(n, config);
        if (config.m_mode == "conventional")
        {
            wet[n] = n >= config.m_delaySamples ? tape[n - config.m_delaySamples] : 0;
        }
        else
        {
            wet[n] = experiment->m_ola.Process()[0] / 1.5f;
            if (n % Resynthesizer::x_H == 0)
            {
                experiment->Launch(n, config, tape, diagnostics);
            }
        }

        tape[n] = dry[n] + config.m_feedback * wet[n];
        assert(std::isfinite(tape[n]));
    }

    WriteAudio(config.m_prefix + ".f32", wet);
    if (config.m_mode == "conventional")
    {
        WriteAudio(config.m_prefix + "-dry.f32", dry);
    }

    return 0;
}

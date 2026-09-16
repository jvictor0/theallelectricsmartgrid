#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>
#include <vector>
#include "support/GlobalEnv.hpp"
#include "QuadReverb.hpp"
#include "QuadDelay.hpp"
#include "PartialMachine.hpp"

double Energy(QuadFloat value)
{
    double result = 0;
    for (int i = 0; i < 4; ++i)
    {
        result += value[i] * value[i];
    }

    return result;
}

void ProbeMapping()
{
    FrequencyDependentParameter::Input input;
    input.m_linearFreqs = FrequencyDependentParameter::Parameter(1.0f);
    FrequencyDependentParameter::Parameter values;
    for (int i = 0; i < 4; ++i)
    {
        values.m_parameters[i] = std::pow(2.0f, i);
    }

    for (float position : {0.74999f, 0.75f, 0.75001f, -0.00001f, 0.0f})
    {
        auto index = FrequencyDependentParameter::GetIndexForLogFrequency(position, input);
        std::printf("mapping log=%+.8f Hz=%.6f index=%d fraction=%.8f geometric=%.8f linear=%.8f\n",
            position, 48000.0f * std::exp2(position - 10.0f), index.m_index,
            index.m_interp, values.Process(index), values.ProcessLinear(index));
    }
}

void ProbeReturnAndDiffusion()
{
    for (float knob : {0.25f, 0.5f, 0.75f, 1.0f})
    {
        PhaseUtils::ZeroedExpParam reverb;
        PhaseUtils::ZeroedExpParam delay;
        double r = reverb.Update(knob / 2.0f);
        double d = delay.Update(knob);
        std::printf("return knob=%.2f reverb=%.9f %.3fdB others=%.9f %.3fdB\n", knob, r, 20 * std::log10(r), d, 20 * std::log10(d));
    }

    auto reverb = std::make_unique<QuadReverb>();
    double energy = 0;
    for (int n = 0; n < 96000; ++n)
    {
        float y = reverb->m_inputFilter.m_allPassFilter[0].Process(n == 0 ? 1.0f : 0.0f);
        energy += y * y;
    }

    std::printf("input diffuser impulse energy=%.9f gain=%.3fdB\n", energy, 10 * std::log10(energy));
    std::printf("saturator small-signal gain=%.9f %.3fdB\n", reverb->m_saturator.DerivativeZero(), 20 * std::log10(reverb->m_saturator.DerivativeZero()));
}

void ProbeReverb(float feedbackKnob)
{
    auto reverb = std::make_unique<QuadReverb>();
    QuadReverbInputSetter setter;
    QuadReverbInputSetter::Input knobs;
    QuadReverb::Input input;
    for (int i = 0; i < 4; ++i)
    {
        knobs.m_reverbTimeKnob[i] = 0.5f;
        knobs.m_feedbackKnob[i] = feedbackKnob;
        knobs.m_dampingBaseKnob[i] = 0.3f;
        knobs.m_dampingWidthKnob[i] = 0.5f;
        knobs.m_modFreqKnob[i] = 0.1f;
        knobs.m_lfoPhaseKnob[i] = 1.0f;
    }

    for (int n = 0; n < 96000; ++n)
    {
        setter.Process(knobs, input);
    }

    std::mt19937 generator(42);
    std::uniform_real_distribution<float> noise(-0.01f, 0.01f);
    double inEnergy = 0;
    double outEnergy = 0;
    for (int n = 0; n < 240000; ++n)
    {
        float x = noise(generator);
        input.m_input = QuadFloat(x, 0, 0, 0);
        QuadFloat output = reverb->Process(input);
        input.m_return = output;
        if (n >= 96000)
        {
            inEnergy += x * x;
            outEnergy += Energy(output);
        }
    }

    PhaseUtils::ZeroedExpParam returnGain;
    double r = returnGain.Update(0.5f);
    std::printf("reverb feedback knob=%.2f loop coefficient=%.6f raw gain=%.3fdB max-return gain=%.3fdB\n", feedbackKnob, input.m_feedback[0], 10 * std::log10(outEnergy / inEnergy), 10 * std::log10(outEnergy / inEnergy) + 20 * std::log10(r));
}

PartialMachine::SynthesisContext::Input SynthesisInput(float shift)
{
    using Parameter = FrequencyDependentParameter::Parameter;
    PartialMachine::SynthesisContext::Input input;
    input.m_bwBaseFrequency = Parameter(1.0f / 4096.0f);
    input.m_bwWidth = Parameter(4096.0f);
    input.m_volume = Parameter(1.0f);
    input.m_bassCutoff = Parameter(0.5f);
    input.m_azimuthFactor = Parameter(1.0f);
    input.m_organicGain = Parameter(1.0f);
    input.m_syntheticGain = Parameter(1.0f);
    input.m_reductionFeedback = Parameter(0.0f);
    input.m_unison = Parameter(0.0f);
    input.m_pitchShiftDepth = Parameter(shift);
    input.m_pitchShift = Parameter(1.0f);
    return input;
}

void ProbePitch(int bin, float shift, bool referencePhase)
{
    auto input = SynthesisInput(shift);
    PartialMachine::SpectralModel::Atom atom;
    atom.m_synthesisOmega = static_cast<float>(bin) / 4096.0f;
    atom.m_synthesisMagnitude = 0.1f;
    QuadOLA ola;
    OLA::Buffer output;
    double energy = 0;
    for (int n = 0; n < 32768; ++n)
    {
        if (n % 1024 == 0)
        {
            PartialMachine::SynthesisContext context;
            double phase = atom.m_synthesisPhase;
            context.ProcessAtom(atom, input);
            if (referencePhase)
            {
                atom.m_synthesisPhase = phase + 1024.0 * atom.m_synthesisOmega * shift;
            }

            ola.Write(context.m_dft);
        }

        float sample = ola.Process().Sum();
        if (n >= 28672)
        {
            output.m_table[n - 28672] = sample;
            energy += sample * sample;
        }
    }

    OLA::DFT spectrum;
    spectrum.Transform(output);
    int peak = 1;
    for (int i = 2; i < 2048; ++i)
    {
        if (std::abs(spectrum.m_components[i]) > std::abs(spectrum.m_components[peak]))
        {
            peak = i;
        }
    }

    std::printf("partial pitch inputBin=%d shift=%.2f referencePhase=%d rms=%.9f peakBin=%d targetBin=%.1f targetMagnitude=%.9f\n", bin, shift, referencePhase, std::sqrt(energy / 4096), peak, bin * shift, std::abs(spectrum.m_components[static_cast<int>(bin * shift)]));
}

void ProbeOrganicGain(float gain)
{
    auto input = SynthesisInput(1.0f);
    input.m_organicGain = FrequencyDependentParameter::Parameter(gain);
    PartialMachine::SpectralModel::Atom atom;
    atom.m_synthesisOmega = 18.0f / 4096.0f;
    atom.m_synthesisMagnitude = 0.1f;
    PartialMachine::SynthesisContext context;
    context.ProcessAtom(atom, input);
    std::printf("partial organic gain=%.1f emitted target magnitude=%.9f\n", gain, std::abs(context.m_dft.m_dfts[0].m_components[18]));
}

void ProbeDelay(bool shifted)
{
    auto delay = std::make_unique<QuadDelay>();
    QuadDelay::Input input;
    for (int i = 0; i < 4; ++i)
    {
        input.m_feedback[i] = 0.0f;
        input.m_bffBase[i] = 1.0f / 65536.0f;
        input.m_bffWidth[i] = 65536.0f;
        input.m_lfoInput.m_freq[i] = 0.5f / 48000.0f;
        if (shifted)
        {
            input.m_grainManagerInput.m_input[i].m_resynthInput.m_shift[0] = Q(2, 1);
            input.m_grainManagerInput.m_input[i].m_resynthInput.m_fade[0] = 1.0f;
        }
    }

    double inEnergy = 0;
    double outEnergy = 0;
    OLA::Buffer buffer;
    for (int n = 0; n < 65536; ++n)
    {
        float sample = 0.01f * std::sin(2.0 * M_PI * 18.0 * n / 4096.0);
        input.m_input = QuadFloat(sample, 0, 0, 0);
        for (int i = 0; i < 4; ++i)
        {
            input.m_writeHeadPosition[i] = n;
            input.m_readHeadPosition[i] = n - 8192.0;
        }

        QuadFloat output = delay->Process(input);
        if (n >= 32768)
        {
            inEnergy += sample * sample;
            outEnergy += Energy(output);
        }

        if (n >= 61440)
        {
            buffer.m_table[n - 61440] = output[0];
        }
    }

    OLA::DFT spectrum;
    spectrum.Transform(buffer);
    int peak = 1;
    for (int i = 2; i < 2048; ++i)
    {
        if (std::abs(spectrum.m_components[i]) > std::abs(spectrum.m_components[peak]))
        {
            peak = i;
        }
    }

    std::printf("delay inputBin=18 shifted=%d gain=%.3fdB peakBin=%d\n", shifted, 10 * std::log10(outEnergy / inEnergy), peak);
}

void ProbeResidual()
{
    using Model = PartialMachine::SpectralModel;
    Model model;
    Model::Input input;
    Model::Buffer buffer;
    constexpr int x_bin = 32;
    for (int n = 0; n < 4096; ++n)
    {
        buffer.m_table[n] = 0.1f * Math4096::Sin2pi(static_cast<float>(x_bin * n) / 4096.0f) * Math4096::Hann(n);
    }

    Model::DFT original;
    original.Transform(buffer);
    model.ExtractAtomsAndResidual(buffer, input);
    std::printf("residual Hann-windowed sine original magnitude=%.9f atom magnitude=%.9f residual magnitude=%.9f fraction=%.6f\n",
        std::abs(original.m_components[x_bin]), model.m_atoms[0]->m_analysisMagnitude,
        model.m_residualModel.GetEnvelope(x_bin), model.m_residualModel.GetEnvelope(x_bin) / std::abs(original.m_components[x_bin]));
    Model::DFT reference = original;
    for (size_t i = 0; i < model.m_atoms.Size(); ++i)
    {
        const auto& atom = *model.m_atoms[i];
        reference.WriteWindowedPartial(atom.m_analysisPhase + 0.5f, 2.0f * atom.m_analysisMagnitude, atom.m_analysisOmega);
    }

    std::printf("residual with diagnostic 2x subtraction magnitude=%.9f\n", std::abs(reference.m_components[x_bin]));
}

int main()
{
    GlobalEnv::Init();
    ProbeMapping();
    ProbeReturnAndDiffusion();
    for (float feedback : {0.0f, 0.75f, 0.95f, 1.0f})
    {
        ProbeReverb(feedback);
    }

    for (int bin : {16, 17, 18})
    {
        ProbePitch(bin, 1.0f, false);
        ProbePitch(bin, 2.0f, false);
        ProbePitch(bin, 2.0f, true);
    }

    ProbeOrganicGain(0.0f);
    ProbeOrganicGain(1.0f);
    ProbeDelay(false);
    ProbeDelay(true);
    ProbeResidual();
}

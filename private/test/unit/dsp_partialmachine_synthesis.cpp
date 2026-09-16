#include "doctest.h"

#include "../support/GlobalEnv.hpp"
#include "PartialMachine.hpp"

namespace
{

PartialMachine::SynthesisContext::Input SynthesisInput(float ratio)
{
    using Parameter = PartialMachine::Parameter;
    PartialMachine::SynthesisContext::Input input;
    input.m_bwBaseFrequency = Parameter(1.0f / 4096.0f);
    input.m_bwWidth = Parameter(4096.0f);
    input.m_volume = Parameter(1.0f);
    input.m_bassCutoff = Parameter(0.5f);
    input.m_organicGain = Parameter(1.0f);
    input.m_syntheticGain = Parameter(1.0f);
    input.m_pitchShiftDepth = Parameter(ratio);
    input.m_pitchShift = Parameter(1.0f);
    return input;
}

OLA::DFT RenderAtom(int sourceBin, float ratio, float unison = 0.0f)
{
    PartialMachine::SpectralModel::Atom atom;
    atom.m_synthesisOmega = static_cast<float>(sourceBin) / 4096.0f;
    atom.m_synthesisMagnitude = 0.1f;
    auto input = SynthesisInput(ratio);
    input.m_unison = PartialMachine::Parameter(unison);
    QuadOLA ola;
    OLA::Buffer buffer;
    for (size_t sample = 0; sample < 16384; ++sample)
    {
        if (sample % 1024 == 0)
        {
            PartialMachine::SynthesisContext context;
            context.ProcessAtom(atom, input);
            ola.Write(context.m_dft);
        }

        float output = ola.Process()[0];
        if (sample >= 12288)
        {
            buffer.m_table[sample - 12288] = output;
        }
    }

    OLA::DFT spectrum;
    spectrum.Transform(buffer);
    return spectrum;
}

}

DOCTEST_TEST_CASE("PartialMachine shifted partials preserve pitch and magnitude across overlap-add frames")
{
    GlobalEnv::ResetPerTest();
    struct Case
    {
        int m_sourceBin;
        float m_ratio;
        int m_targetBin;
    };

    const Case cases[] = {{17, 2.0f, 34}, {18, 2.0f, 36}, {18, 0.5f, 9}, {18, 1.5f, 27}};
    for (const auto& example : cases)
    {
        DOCTEST_CAPTURE(example.m_sourceBin);
        DOCTEST_CAPTURE(example.m_ratio);
        auto spectrum = RenderAtom(example.m_sourceBin, example.m_ratio);
        float targetMagnitude = std::abs(spectrum.m_components[example.m_targetBin]);
        DOCTEST_CHECK(targetMagnitude > 0.07f);
        DOCTEST_CHECK(targetMagnitude < 0.08f);
        for (size_t bin = 1; bin < OLA::x_maxComponents; ++bin)
        {
            if (bin != static_cast<size_t>(example.m_targetBin))
            {
                DOCTEST_REQUIRE(std::abs(spectrum.m_components[bin]) < 1e-4f);
            }
        }
    }
}

DOCTEST_TEST_CASE("PartialMachine shifted unison keeps its center and detuned upper voices audible")
{
    GlobalEnv::ResetPerTest();
    auto spectrum = RenderAtom(225, 2.0f, 1.0f);
    float centerMagnitude = std::abs(spectrum.m_components[450]);
    float upperMagnitude = std::abs(spectrum.m_components[477]);
    DOCTEST_CHECK(centerMagnitude > 0.032f);
    DOCTEST_CHECK(centerMagnitude < 0.035f);
    DOCTEST_CHECK(upperMagnitude > 0.065f);
    DOCTEST_CHECK(upperMagnitude < 0.069f);
}

DOCTEST_TEST_CASE("PartialMachine organic and synthetic gains mute their respective atoms")
{
    for (bool synthetic : {false, true})
    {
        auto input = SynthesisInput(1.0f);
        input.m_organicGain = PartialMachine::Parameter(synthetic ? 1.0f : 0.0f);
        input.m_syntheticGain = PartialMachine::Parameter(synthetic ? 0.0f : 1.0f);
        PartialMachine::SpectralModel::Atom atom;
        atom.m_isSynthetic = synthetic;
        atom.m_synthesisOmega = 32.0f / 4096.0f;
        atom.m_synthesisMagnitude = 0.1f;
        PartialMachine::SynthesisContext context;
        context.ProcessAtom(atom, input);
        for (const auto& dft : context.m_dft.m_dfts)
        {
            for (const auto& component : dft.m_components)
            {
                DOCTEST_REQUIRE(std::abs(component) < 1e-10f);
            }
        }

        DOCTEST_CHECK(atom.m_synthesisMagnitude == doctest::Approx(0.1f));
    }
}

DOCTEST_TEST_CASE("PartialMachine pitch ratio scales emitted phase while the accumulator stays unshifted")
{
    PartialMachine::SpectralModel::Atom atom;
    atom.m_synthesisOmega = 18.0f / 4096.0f;
    atom.m_synthesisMagnitude = 0.1f;
    atom.m_synthesisPhase = 0.125;
    const float ratios[] = {1.0f, 2.0f, 0.5f, 1.0f};
    const int bins[] = {18, 36, 9, 18};
    const double phases[] = {0.125, 0.25, -0.4375, -0.375};
    const double accumulatedPhases[] = {4.625, 9.125, 13.625, 18.125};
    for (size_t frame = 0; frame < 4; ++frame)
    {
        auto input = SynthesisInput(ratios[frame]);
        PartialMachine::SynthesisContext context;
        context.ProcessAtom(atom, input);
        DOCTEST_CHECK(atom.m_synthesisPhase == doctest::Approx(accumulatedPhases[frame]));
        auto component = context.m_dft.m_dfts[0].m_components[bins[frame]];
        DOCTEST_REQUIRE(std::abs(component) > 0.01f);
        auto unit = component / std::abs(component);
        DOCTEST_CHECK(unit.real() == doctest::Approx(std::cos(2.0 * M_PI * phases[frame])).epsilon(1e-4));
        DOCTEST_CHECK(unit.imag() == doctest::Approx(std::sin(2.0 * M_PI * phases[frame])).epsilon(1e-4));
    }
}

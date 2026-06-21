#include "doctest.h"

#include "SpectralModel.hpp"

namespace
{

using Model = SpectralModel;
using Input = Model::Input;
using AnalysisAtom = Model::AnalysisAtom;
using AnalysisAtomArray = Model::AnalysisAtomArray;
using Atom = Model::Atom;

Input MakeInput(float density)
{
    Input input;
    input.m_slewUpAlpha = ScalarParameter::Parameter(1.0f);
    input.m_slewDownAlpha = ScalarParameter::Parameter(1.0f);
    input.m_omegaPortamentoAlpha = ScalarParameter::Parameter(1.0f);
    input.m_omegaDensity = ScalarParameter::Parameter(density);
    return input;
}

Atom MakeAtom(float omega)
{
    return Atom(omega, 1.0f, ScalarParameter::Index(), omega, 1.0f, 0.0f);
}

void AddAnalysisAtom(AnalysisAtomArray& atoms, float omega, float magnitude, bool isSynthetic = false)
{
    atoms.Add(AnalysisAtom(omega, magnitude, ScalarParameter::Index(), isSynthetic));
}

}  // namespace

DOCTEST_TEST_CASE("SpectralModel tracking theta prefers nearby continuation over slightly louder farther peak")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.101f, 0.90f);
    AddAnalysisAtom(atoms, 0.108f, 1.00f);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.101f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(0.90f));
}

DOCTEST_TEST_CASE("SpectralModel tracking theta still allows much stronger farther peak")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.101f, 0.90f);
    AddAnalysisAtom(atoms, 0.108f, 10.0f);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.108f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(10.0f));
}

DOCTEST_TEST_CASE("SpectralModel tracking keeps organic priority before theta")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.101f, 0.50f, true);
    AddAnalysisAtom(atoms, 0.108f, 0.10f, false);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.108f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(0.10f));
    DOCTEST_CHECK(atom.m_isSynthetic == false);
}

DOCTEST_TEST_CASE("SpectralModel tracking ignores first lower_bound candidate outside density window")
{
    Model model;
    Input input = MakeInput(0.01f);
    Atom atom = MakeAtom(0.10f);
    AnalysisAtomArray atoms;

    AddAnalysisAtom(atoms, 0.12f, 1.0f);

    model.SearchAndMerge(atoms, atom, input);

    DOCTEST_CHECK(atom.m_analysisOmega == doctest::Approx(0.10f));
    DOCTEST_CHECK(atom.m_analysisMagnitude == doctest::Approx(0.0f));
    DOCTEST_CHECK(atom.m_synthesisMagnitude == doctest::Approx(0.0f));
}

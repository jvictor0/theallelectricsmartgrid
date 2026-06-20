#pragma once

#include "AdaptiveWaveTable.hpp"
#include "Array.hpp"
#include "FixedAllocator.hpp"
#include "FrequencyDependentParameter.hpp"
#include "Slew.hpp"

#include <algorithm>
#include <cmath>

template <size_t Bits, typename ParameterProvider>
struct SpectralModelGeneric
{
    typedef BasicWaveTableGeneric<Bits> Buffer;
    typedef DiscreteFourierTransformGeneric<Bits> DFT;
    typedef typename ParameterProvider::Index ParameterIndex;
    typedef typename ParameterProvider::Parameter Parameter;
    static constexpr size_t x_tableSize = Buffer::x_tableSize;
    static constexpr size_t x_maxComponents = DFT::x_maxComponents;
    static constexpr size_t x_maxAtoms = 8192;
    static constexpr size_t x_numSyntheticHarmonics = 6;
    static constexpr size_t x_hopDenom = 4;
    static constexpr size_t x_H = x_tableSize / x_hopDenom;
    static constexpr float x_deathMag = 1e-5f;
    static constexpr float x_mergeGainThreshold = 1e-3f;

    struct Input
    {
        float m_gainThreshold;
        size_t m_numAtoms;
        Parameter m_slewUpAlpha;
        Parameter m_slewDownAlpha;
        Parameter m_omegaPortamentoAlpha;
        Parameter m_omegaDensity;
        bool m_useSyntheticHarmonics;
        Parameter m_syntheticHarmonics[x_numSyntheticHarmonics];
        typename ParameterProvider::Input m_parameterInput;

        Input()
            : m_gainThreshold(0.001f)
            , m_numAtoms(64)
            , m_slewUpAlpha(1.0f)
            , m_slewDownAlpha(1.0f)
            , m_omegaPortamentoAlpha(1.0f)
            , m_omegaDensity(1.0 / x_tableSize)
            , m_useSyntheticHarmonics(false)
        {
        }
    };

    struct AnalysisAtom
    {
        float m_analysisOmega;
        float m_analysisMagnitude;
        ParameterIndex m_index;
        bool m_isSynthetic;
        
        AnalysisAtom()
            : m_analysisOmega(0.0f)
            , m_analysisMagnitude(0.0f)
            , m_index()
            , m_isSynthetic(false)
        {
        }

        AnalysisAtom(float analysisOmega, float analysisMagnitude, ParameterIndex index, bool isSynthetic)
            : m_analysisOmega(analysisOmega)
            , m_analysisMagnitude(analysisMagnitude)
            , m_index(index)
            , m_isSynthetic(isSynthetic)
        {
        }

        static bool CmpReverseMagnitude(const AnalysisAtom& a, const AnalysisAtom& b)
        {
            return b.m_analysisMagnitude < a.m_analysisMagnitude;
        }

        static bool IsPreferred(const AnalysisAtom& candidate, const AnalysisAtom& current)
        {
            if (candidate.m_isSynthetic != current.m_isSynthetic)
            {
                return !candidate.m_isSynthetic;
            }

            return current.m_analysisMagnitude < candidate.m_analysisMagnitude;
        }

        static bool CmpOmega(const AnalysisAtom& a, const AnalysisAtom& b)
        {
            return a.m_analysisOmega < b.m_analysisOmega;
        }

        static bool CmpOmegaFloat(const AnalysisAtom& a, float omega)
        {
            return a.m_analysisOmega < omega;
        }
    };

    struct Atom : public AnalysisAtom
    {
        float m_synthesisOmega;
        float m_synthesisMagnitude;
        double m_synthesisPhase;

        Atom()
            : AnalysisAtom()
            , m_synthesisOmega(0.0f)
            , m_synthesisMagnitude(0.0f)
            , m_synthesisPhase(0.0f)
        {
        }

        Atom(float analysisOmega, float analysisMagnitude, ParameterIndex index, float synthesisOmega, float synthesisMagnitude, float synthesisPhase)
            : AnalysisAtom(analysisOmega, analysisMagnitude, index, false)
            , m_synthesisOmega(synthesisOmega)
            , m_synthesisMagnitude(synthesisMagnitude)
            , m_synthesisPhase(synthesisPhase)
        {
        }

        void Merge(const AnalysisAtom& analysisAtom, Input& input)
        {
            float slewUpAlpha = input.m_slewUpAlpha.Process(analysisAtom.m_index);
            float slewDownAlpha = input.m_slewDownAlpha.Process(analysisAtom.m_index);
            float omegaPortamentoAlpha = input.m_omegaPortamentoAlpha.Process(analysisAtom.m_index);
            m_synthesisMagnitude = BiDirectionalSlew::Process(m_synthesisMagnitude, analysisAtom.m_analysisMagnitude, slewUpAlpha, slewDownAlpha);
            AnalysisAtom::m_analysisOmega = analysisAtom.m_analysisOmega;
            AnalysisAtom::m_analysisMagnitude = analysisAtom.m_analysisMagnitude;
            AnalysisAtom::m_index = analysisAtom.m_index;
            AnalysisAtom::m_isSynthetic = analysisAtom.m_isSynthetic;
            m_synthesisOmega = Slew::Process(m_synthesisOmega, analysisAtom.m_analysisOmega, omegaPortamentoAlpha);
        }

        void MergeNoMatch(Input& input)
        {
            float slewDownAlpha = input.m_slewDownAlpha.Process(AnalysisAtom::m_index);
            float omegaPortamentoAlpha = input.m_omegaPortamentoAlpha.Process(AnalysisAtom::m_index);
            m_synthesisMagnitude = Slew::Process(m_synthesisMagnitude, 0.0, slewDownAlpha);
            AnalysisAtom::m_analysisMagnitude = 0.0;
            m_synthesisOmega = Slew::Process(m_synthesisOmega, AnalysisAtom::m_analysisOmega, omegaPortamentoAlpha);
        }

        void UpdatePhase()
        {
            m_synthesisPhase += x_H * m_synthesisOmega;
        }

        static bool CmpReverseMagnitude(const Atom& a, const Atom& b)
        {
            return AnalysisAtom::CmpReverseMagnitude(a, b);
        }

        static bool CmpReverseMagnitudePtr(Atom* const & a, Atom* const & b)
        {
            return CmpReverseMagnitude(*a, *b);
        }
    };

    using AnalysisAtomArray = Array<AnalysisAtom, x_maxAtoms>;
    using AtomArray = Array<Atom, x_maxAtoms>;
    using AtomStarArray = Array<Atom*, x_maxAtoms>;

    struct AtomsArrayWithIndex
    {
        FixedAllocator<Atom, x_maxAtoms> m_allocator;
        AtomStarArray m_atoms;

        void Add(const Atom& atom)
        {
            Atom* newAtom = m_allocator.Allocate();
            *newAtom = atom;
            m_atoms.Add(newAtom);
        }

        void SortByReverseMagnitude()
        {
            m_atoms.Sort(Atom::CmpReverseMagnitudePtr);
        }

        void Pop()
        {
            Atom* atom = Back();
            m_atoms.Pop();
            m_allocator.Free(atom);
        }

        void ShrinkIfNecessary(size_t maxSize)
        {
            while (Size() > maxSize)
            {
                Pop();
            }
        }

        Atom* Back()
        {
            return m_atoms.Back();
        }

        const Atom* Back() const
        {
            return m_atoms.Back();
        }

        size_t Size() const
        {
            return m_atoms.Size();
        }

        bool Empty() const
        {
            return m_atoms.Empty();
        }

        Atom* operator[](size_t index)
        {
            return m_atoms[index];
        }

        const Atom* operator[](size_t index) const
        {
            return m_atoms[index];
        }

        bool IsAtomAllocated(Atom* atom) const
        {
            return m_allocator.IsAllocated(atom);
        }
    };

    void ExtractAnalysisAtoms(DFT& dft, AnalysisAtomArray& analysisAtoms, Input& input)
    {
        analysisAtoms.Clear();

        // Compute magnitudes
        //
        float mags[x_maxComponents];
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            mags[i] = std::abs(dft.m_components[i]);            
        }

        // Find local maxima and apply parabolic interpolation
        //
        constexpr float x_logEps = 1e-20f;

        for (int k = static_cast<int>(x_maxComponents) - 2; 2 <= k; --k)
        {
            float mag = mags[k];
            if (mags[k - 1] < mag && mags[k + 1] < mag && input.m_gainThreshold <= mag)
            {
                // Log-domain parabolic interpolation
                //
                float magLo = std::max(mags[k - 1], x_logEps);
                float magMid = std::max(mag, x_logEps);
                float magHi = std::max(mags[k + 1], x_logEps);

                float alpha = std::log(magLo);
                float beta = std::log(magMid);
                float gamma = std::log(magHi);
                float denom = alpha - 2.0f * beta + gamma;

                float p = 0.0f;
                float peakMag = mag;
                if (1e-10f < std::abs(denom))
                {
                    p = 0.5f * (alpha - gamma) / denom;
                    peakMag = std::exp(beta - 0.25f * (alpha - gamma) * p);
                }

                float peakOmega = (static_cast<float>(k) + p) / static_cast<float>(x_tableSize);
                ParameterIndex index = ParameterProvider::GetIndexForFrequency(peakOmega, input.m_parameterInput);
                analysisAtoms.Add(AnalysisAtom(peakOmega, peakMag, index, false));
            }
        }

        size_t numAnalysisAtoms = analysisAtoms.Size();
        if (input.m_useSyntheticHarmonics)
        {
            for (size_t i = 0; i < numAnalysisAtoms; ++i)
            {
                AnalysisAtom& analysisAtom = analysisAtoms[i];
                for (size_t j = 0; j < x_numSyntheticHarmonics; ++j)
                {
                    if (x_maxAtoms <= analysisAtoms.Size())
                    {
                        break;
                    }

                    float harmonicOmega = analysisAtom.m_analysisOmega * static_cast<float>(j + 2);
                    ParameterIndex index = ParameterProvider::GetIndexForFrequency(harmonicOmega, input.m_parameterInput);
                    float harmonicMagnitude = analysisAtom.m_analysisMagnitude * input.m_syntheticHarmonics[j].Process(index);
                    analysisAtoms.Add(AnalysisAtom(harmonicOmega, harmonicMagnitude, index, true));
                }
            }
        }

        if (input.m_numAtoms < analysisAtoms.Size())
        {
            analysisAtoms.Sort(AnalysisAtom::CmpReverseMagnitude);
            analysisAtoms.ShrinkIfNecessary(input.m_numAtoms);
        }

        analysisAtoms.Sort(AnalysisAtom::CmpOmega);
    }

    void SearchAndMerge(AnalysisAtomArray& analysisAtoms, Atom& atom, Input& input)
    {
        float omegaDensity = input.m_omegaDensity.Process(atom.m_index);
        float lowerOmega = atom.m_analysisOmega - omegaDensity;
        auto it = std::lower_bound(analysisAtoms.begin(), analysisAtoms.end(), lowerOmega, AnalysisAtom::CmpOmegaFloat);
        auto bestIt = analysisAtoms.end();
        for (; it != analysisAtoms.end(); ++it)
        {
            if (atom.m_analysisOmega + omegaDensity < it->m_analysisOmega)
            {
                break;
            }

            if (it->m_analysisMagnitude < x_deathMag)
            {
                continue;
            }

            if (bestIt == analysisAtoms.end() || AnalysisAtom::IsPreferred(*it, *bestIt))
            {
                bestIt = it;
            }
        }

        if (bestIt == analysisAtoms.end())
        {
            atom.MergeNoMatch(input);
            return;
        }

        if (bestIt->m_analysisMagnitude / atom.m_synthesisMagnitude < x_mergeGainThreshold)
        {
            atom.MergeNoMatch(input);
        }
        else
        {
            atom.Merge(*bestIt, input);
        }

        for (auto forIt = bestIt; forIt != analysisAtoms.end(); ++forIt)
        {
            if (atom.m_analysisOmega + omegaDensity < forIt->m_analysisOmega)
            {
                break;
            }
            
            forIt->m_analysisMagnitude = -1.0f;
        }

        for (auto revIt = bestIt; revIt != analysisAtoms.begin();)
        {
            --revIt;
            if (revIt->m_analysisOmega < atom.m_analysisOmega - omegaDensity)
            {
                break;
            }

            revIt->m_analysisMagnitude = -1.0f;
        }
    }

    void ExtractAtoms(Buffer& buffer, Input& input)
    {
        DFT dft;
        dft.Transform(buffer);
        AnalysisAtomArray analysisAtoms;
        ExtractAnalysisAtoms(dft, analysisAtoms, input);
        m_atoms.SortByReverseMagnitude();
        for (size_t i = 0; i < m_atoms.Size(); ++i)
        {
            SearchAndMerge(analysisAtoms, *m_atoms[i], input);
        }

        for (AnalysisAtom& analysisAtom : analysisAtoms)
        {
            if (x_deathMag <= analysisAtom.m_analysisMagnitude)
            {
                float slewUpAlpha = input.m_slewUpAlpha.Process(analysisAtom.m_index);
                float initMag = std::max(Slew::Process(0, analysisAtom.m_analysisMagnitude, slewUpAlpha), x_deathMag);
                Atom newAtom(analysisAtom.m_analysisOmega, analysisAtom.m_analysisMagnitude, analysisAtom.m_index, analysisAtom.m_analysisOmega, initMag, 0);
                m_atoms.Add(newAtom);
            }
        }

        m_atoms.SortByReverseMagnitude();
        m_atoms.ShrinkIfNecessary(input.m_numAtoms);
        while (!m_atoms.Empty() && m_atoms.Back()->m_synthesisMagnitude < x_deathMag)
        {
            m_atoms.Pop();
        }
    }

    bool IsAtomAllocated(Atom* atom) const
    {
        return m_atoms.IsAtomAllocated(atom);
    }

    AtomsArrayWithIndex m_atoms;
};

using SpectralModel = SpectralModelGeneric<12, ScalarParameter>;

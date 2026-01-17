#pragma once

#include "AdaptiveWaveTable.hpp"
#include "Slew.hpp"
#include "Array.hpp"
#include "FixedAllocator.hpp"
#include <algorithm>
#include <cmath>

template <size_t Bits>
struct SpectralModelGeneric
{
    typedef BasicWaveTableGeneric<Bits> Buffer;
    typedef DiscreteFourierTransformGeneric<Bits> DFT;
    static constexpr size_t x_tableSize = Buffer::x_tableSize;
    static constexpr size_t x_maxComponents = DFT::x_maxComponents;
    static constexpr size_t x_hopDenom = 4;
    static constexpr size_t x_H = x_tableSize / x_hopDenom;
    static constexpr float x_deathMag = 1e-5f;
    static constexpr float x_mergeGainThreshold = 1e-3f;

    struct Input
    {
        float m_gainThreshold;
        size_t m_numAtoms;
        float m_slewUpAlpha;
        float m_slewDownAlpha;
        float m_omegaDensity;

        Input()
            : m_gainThreshold(0.001f)
            , m_numAtoms(64)
            , m_slewUpAlpha(1.0f)
            , m_slewDownAlpha(1.0f)
            , m_omegaDensity(1.0 / x_tableSize)
        {
        }
    };

    struct AnalysisAtom
    {
        float m_analysisOmega;
        float m_analysisMagnitude;
        
        AnalysisAtom()
            : m_analysisOmega(0.0f)
            , m_analysisMagnitude(0.0f)
        {
        }

        AnalysisAtom(float analysisOmega, float analysisMagnitude)
            : m_analysisOmega(analysisOmega)
            , m_analysisMagnitude(analysisMagnitude)
        {
        }

        static bool CmpReverseMagnitude(const AnalysisAtom& a, const AnalysisAtom& b)
        {
            return b.m_analysisMagnitude < a.m_analysisMagnitude;
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

        Atom(float analysisOmega, float analysisMagnitude, float synthesisOmega, float synthesisMagnitude, float synthesisPhase)
            : AnalysisAtom(analysisOmega, analysisMagnitude)
            , m_synthesisOmega(synthesisOmega)
            , m_synthesisMagnitude(synthesisMagnitude)
            , m_synthesisPhase(synthesisPhase)
        {
        }

        void Merge(const AnalysisAtom& analysisAtom, Input& input)
        {
            m_synthesisMagnitude = BiDirectionalSlew::Process(m_synthesisMagnitude, analysisAtom.m_analysisMagnitude, input.m_slewUpAlpha, input.m_slewDownAlpha);
            AnalysisAtom::m_analysisOmega = analysisAtom.m_analysisOmega;
            AnalysisAtom::m_analysisMagnitude = analysisAtom.m_analysisMagnitude;
            m_synthesisOmega = analysisAtom.m_analysisOmega;
        }

        void MergeNoMatch(Input& input)
        {
            m_synthesisMagnitude = Slew::Process(m_synthesisMagnitude, 0.0, input.m_slewDownAlpha);
            AnalysisAtom::m_analysisMagnitude = 0.0;
            m_synthesisOmega = AnalysisAtom::m_analysisOmega;
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

    using AnalysisAtomArray = Array<AnalysisAtom, x_maxComponents>;
    using AtomArray = Array<Atom, x_maxComponents>;
    using AtomStarArray = Array<Atom*, x_maxComponents>;

    struct AtomsArrayWithIndex
    {
        FixedAllocator<Atom, x_maxComponents> m_allocator;
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

    void ExtractAnalysisAtoms(DFT& dft, AnalysisAtomArray& analysisAtoms, const Input& input)
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
                analysisAtoms.Add(AnalysisAtom(peakOmega, peakMag));
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
        float lowerOmega = atom.m_analysisOmega - input.m_omegaDensity;
        auto it = std::lower_bound(analysisAtoms.begin(), analysisAtoms.end(), lowerOmega, AnalysisAtom::CmpOmegaFloat);
        auto bestIt = it;
        for (; it != analysisAtoms.end(); ++it)
        {
            if (atom.m_analysisOmega + input.m_omegaDensity < it->m_analysisOmega)
            {
                break;
            }
            if (bestIt->m_analysisMagnitude < it->m_analysisMagnitude)
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
            if (atom.m_analysisOmega + input.m_omegaDensity < forIt->m_analysisOmega)
            {
                break;
            }
            
            forIt->m_analysisMagnitude = -1.0f;
        }

        for (auto revIt = bestIt - 1; analysisAtoms.begin() <= revIt; --revIt)
        {
            if (revIt->m_analysisOmega < atom.m_analysisOmega - input.m_omegaDensity)
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
                float initMag = std::max(Slew::Process(0, analysisAtom.m_analysisMagnitude, input.m_slewUpAlpha), x_deathMag);
                Atom newAtom(analysisAtom.m_analysisOmega, analysisAtom.m_analysisMagnitude, analysisAtom.m_analysisOmega, initMag, 0);
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

using SpectralModel = SpectralModelGeneric<12>;

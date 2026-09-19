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
        typename ParameterProvider::Input m_parameterInput;

        Input()
            : m_gainThreshold(0.001f)
            , m_numAtoms(64)
            , m_slewUpAlpha(1.0f)
            , m_slewDownAlpha(1.0f)
            , m_omegaPortamentoAlpha(1.0f)
            , m_omegaDensity(1.0f / 1200.0f)
        {
        }
    };

    struct AnalysisAtom
    {
        float m_analysisOmega;
        float m_logAnalysisOmega;
        float m_analysisMagnitude;
        float m_analysisPhase;
        ParameterIndex m_index;

        AnalysisAtom()
            : m_analysisOmega(0.0f)
            , m_logAnalysisOmega(std::numeric_limits<float>::lowest())
            , m_analysisMagnitude(0.0f)
            , m_analysisPhase(0.0f)
            , m_index()
        {
        }

        AnalysisAtom(float analysisOmega, float analysisMagnitude, float analysisPhase, ParameterIndex index)
            : m_analysisOmega(analysisOmega)
            , m_logAnalysisOmega(std::log2f(analysisOmega))
            , m_analysisMagnitude(analysisMagnitude)
            , m_analysisPhase(analysisPhase)
            , m_index(index)
        {
        }

        AnalysisAtom(float analysisOmega, float logAnalysisOmega, float analysisMagnitude, float analysisPhase, ParameterIndex index)
            : m_analysisOmega(analysisOmega)
            , m_logAnalysisOmega(logAnalysisOmega)
            , m_analysisMagnitude(analysisMagnitude)
            , m_analysisPhase(analysisPhase)
            , m_index(index)
        {
        }

        static bool CmpReverseMagnitude(const AnalysisAtom& a, const AnalysisAtom& b)
        {
            return b.m_analysisMagnitude < a.m_analysisMagnitude;
        }

        static float PreferredMatchTheta(const AnalysisAtom& candidate, float targetLogOmega, float density)
        {
            float distance = std::abs(candidate.m_logAnalysisOmega - targetLogOmega);
            if (density <= 0.0f)
            {
                if (distance <= 0.0f)
                {
                    return candidate.m_analysisMagnitude;
                }

                return 0.0f;
            }

            float distanceWeight = std::max(0.0f, 1.0f - distance / density);
            return candidate.m_analysisMagnitude * distanceWeight;
        }

        static bool CmpLogOmega(const AnalysisAtom& a, const AnalysisAtom& b)
        {
            return a.m_logAnalysisOmega < b.m_logAnalysisOmega;
        }

        static bool CmpLogOmegaFloat(const AnalysisAtom& a, float logOmega)
        {
            return a.m_logAnalysisOmega < logOmega;
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
            : AnalysisAtom(analysisOmega, analysisMagnitude, 0.0f, index)
            , m_synthesisOmega(synthesisOmega)
            , m_synthesisMagnitude(synthesisMagnitude)
            , m_synthesisPhase(synthesisPhase)
        {
        }

        Atom(
            float analysisOmega,
            float logAnalysisOmega,
            float analysisMagnitude,
            float analysisPhase,
            ParameterIndex index,
            float synthesisOmega,
            float synthesisMagnitude,
            float synthesisPhase)
            : AnalysisAtom(analysisOmega, logAnalysisOmega, analysisMagnitude, analysisPhase, index)
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
            AnalysisAtom::m_logAnalysisOmega = analysisAtom.m_logAnalysisOmega;
            AnalysisAtom::m_analysisMagnitude = analysisAtom.m_analysisMagnitude;
            AnalysisAtom::m_analysisPhase = analysisAtom.m_analysisPhase;
            AnalysisAtom::m_index = analysisAtom.m_index;
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

        void MergeDominated(const AnalysisAtom& analysisAtom, float density, Input& input)
        {
            float factor = AnalysisAtom::PreferredMatchTheta(
                analysisAtom,
                AnalysisAtom::m_logAnalysisOmega,
                density) / m_synthesisMagnitude;
            if (1.0 < factor)
            {
                m_synthesisMagnitude /= factor;
            }

            MergeNoMatch(input);
        }

        void UpdatePhase()
        {
            m_synthesisPhase += x_H * m_synthesisOmega;
        }

        static bool CmpReverseSynthesisMagnitude(const Atom& a, const Atom& b)
        {
            bool aFinite = std::isfinite(a.m_synthesisMagnitude);
            bool bFinite = std::isfinite(b.m_synthesisMagnitude);

            if (aFinite != bFinite)
            {
                return aFinite;
            }

            return b.m_synthesisMagnitude < a.m_synthesisMagnitude;
        }

        static bool CmpReverseSynthesisMagnitudePtr(Atom* const & a, Atom* const & b)
        {
            return CmpReverseSynthesisMagnitude(*a, *b);
        }
    };

    using AnalysisAtomArray = Array<AnalysisAtom, x_maxAtoms>;
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

        void SortByReverseSynthesisMagnitude()
        {
            m_atoms.Sort(Atom::CmpReverseSynthesisMagnitudePtr);
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

    struct ResidualModel
    {
        static constexpr size_t x_numBuckets = DFT::x_maxComponents;

        struct Input
        {
            float m_analysisResidualMagnitudes[x_numBuckets];

            Input()
                : m_analysisResidualMagnitudes{}
            {
            }
        };

        float m_magnitudes[x_numBuckets];
        float m_frequencies[x_numBuckets];
        float m_logFrequencies[x_numBuckets];

        ResidualModel()
            : m_magnitudes{}
            , m_frequencies{}
            , m_logFrequencies{}
        {
            for (size_t i = 0; i < x_numBuckets; ++i)
            {
                m_frequencies[i] = static_cast<float>(i) / static_cast<float>(x_tableSize);
                size_t parameterBucket = std::max<size_t>(i, 1);
                float parameterFrequency = static_cast<float>(parameterBucket) / static_cast<float>(x_tableSize);
                m_logFrequencies[i] = ParameterProvider::FrequencyToLinear(parameterFrequency);
            }
        }

        float GetEnvelope(size_t bucketIndex) const
        {
            return m_magnitudes[bucketIndex];
        }

        void Process(SpectralModelGeneric::Input& spectralInput, Input& residualInput)
        {
            for (size_t i = 0; i < x_numBuckets; ++i)
            {
                ParameterIndex index = ParameterProvider::GetIndexForLogFrequency(m_logFrequencies[i], spectralInput.m_parameterInput);
                float slewUpAlpha = spectralInput.m_slewUpAlpha.Process(index);
                float slewDownAlpha = spectralInput.m_slewDownAlpha.Process(index);
                m_magnitudes[i] = BiDirectionalSlew::Process(
                    m_magnitudes[i],
                    residualInput.m_analysisResidualMagnitudes[i],
                    slewUpAlpha,
                    slewDownAlpha);
            }
        }
    };

    void ExtractAndSubtractAnalysisAtoms(DFT& dft, AnalysisAtomArray& analysisAtoms, Input& input)
    {
        analysisAtoms.Clear();

        // Compute magnitudes
        //
        float mags[x_maxComponents];
        for (size_t i = 1; i < x_maxComponents; ++i)
        {
            mags[i] = std::abs(dft.m_components[i]);            
        }

        // Locate each peak, fit its coefficient, and subtract it from the DFT.
        //
        constexpr float x_logEps = 1e-20f;

        for (int k = static_cast<int>(x_maxComponents) - 2; 2 <= k; --k)
        {
            float mag = mags[k];
            if (mags[k - 1] < mag && mags[k + 1] <= mag && input.m_gainThreshold <= mag)
            {
                // Log-domain parabolic frequency interpolation
                //
                float magLo = std::max(mags[k - 1], x_logEps);
                float magMid = std::max(mag, x_logEps);
                float magHi = std::max(mags[k + 1], x_logEps);

                float alpha = std::log(magLo);
                float beta = std::log(magMid);
                float gamma = std::log(magHi);
                float denom = alpha - 2.0f * beta + gamma;

                float p = 0.0f;
                if (1e-10f < std::abs(denom))
                {
                    p = 0.5f * (alpha - gamma) / denom;
                }

                float exactBin = static_cast<float>(k) + p;
                float peakOmega = exactBin / static_cast<float>(x_tableSize);
                int centerBin = static_cast<int>(std::floor(exactBin));
                int firstBin = std::max(1, centerBin - DFT::x_partialKernelRadius);
                int lastBin = std::min(static_cast<int>(x_maxComponents) - 1, centerBin + DFT::x_partialKernelRadius);
                std::complex<float> projection(0.0f, 0.0f);
                float kernelEnergy = 0.0f;
                for (int bin = firstBin; bin <= lastBin; ++bin)
                {
                    auto kernel = MathGeneric<Bits>::HannKernel(exactBin - static_cast<float>(bin));
                    projection += std::conj(kernel) * dft.m_components[bin];
                    kernelEnergy += std::norm(kernel);
                }

                if (kernelEnergy > 0.0f)
                {
                    auto coefficient = projection / kernelEnergy;

                    // Analysis magnitude is A/4 for a cosine of amplitude A;
                    // the writer expects A/2 and frame-start phase in cycles.
                    //
                    float peakMag = 0.5f * std::abs(coefficient);
                    float peakPhase = std::arg(coefficient) / (2.0f * static_cast<float>(M_PI));
                    dft.WriteWindowedPartial(peakPhase + 0.5f, 2.0f * peakMag, peakOmega);
                    ParameterIndex index = ParameterProvider::GetIndexForFrequency(peakOmega, input.m_parameterInput);
                    analysisAtoms.Add(AnalysisAtom(peakOmega, peakMag, peakPhase, index));
                }
            }
        }

        if (input.m_numAtoms < analysisAtoms.Size())
        {
            analysisAtoms.Sort(AnalysisAtom::CmpReverseMagnitude);
            analysisAtoms.ShrinkIfNecessary(input.m_numAtoms);
        }

        analysisAtoms.Sort(AnalysisAtom::CmpLogOmega);
    }

    void MergeAtom(Atom& atom, AnalysisAtom* analysisAtom, bool isMatch, Input& input)
    {
        if (!analysisAtom ||
            analysisAtom->m_analysisMagnitude / atom.m_synthesisMagnitude < x_mergeGainThreshold ||
            (!isMatch && analysisAtom->m_analysisMagnitude < atom.m_synthesisMagnitude))
        {
            atom.MergeNoMatch(input);
        }
        else if (!isMatch)
        {
            float density = input.m_omegaDensity.Process(atom.m_index);
            atom.MergeDominated(*analysisAtom, density, input);
        }
        else
        {
            atom.Merge(*analysisAtom, input);
        }
    }

    struct AtomMatcher
    {
        struct Score
        {
            double m_theta = 0.0;
            size_t m_numMatches = 0;
        };

        struct Workspace
        {
            Atom* m_atoms[x_maxAtoms];
            AnalysisAtom* m_matches[x_maxAtoms];
            float m_densities[x_maxAtoms];
            Score m_forward[x_maxAtoms + 1];
            Score m_backward[x_maxAtoms + 1];

            void Clear()
            {
                std::fill(m_atoms, m_atoms + x_maxAtoms, nullptr);
                std::fill(m_matches, m_matches + x_maxAtoms, nullptr);
                std::fill(m_densities, m_densities + x_maxAtoms, 0.0f);
                std::fill(m_forward, m_forward + x_maxAtoms + 1, Score{});
                std::fill(m_backward, m_backward + x_maxAtoms + 1, Score{});
            }
        };

        struct SynthesisAtomResult
        {
            Atom* m_atom;
            AnalysisAtom* m_analysisAtom;
            bool m_isMatch;

            SynthesisAtomResult()
                : m_atom(nullptr)
                , m_analysisAtom(nullptr)
                , m_isMatch(false)
            {
            }
        };

        struct AnalysisAtomResult
        {
            AnalysisAtom* m_analysisAtom;
            bool m_isMatched;

            AnalysisAtomResult()
                : m_analysisAtom(nullptr)
                , m_isMatched(false)
            {
            }
        };

        struct Result
        {
            Array<SynthesisAtomResult, x_maxAtoms> m_synthesisAtomResults;
            Array<AnalysisAtomResult, x_maxAtoms> m_analysisAtomResults;

            void Clear()
            {
                m_synthesisAtomResults.Clear();
                m_analysisAtomResults.Clear();
            }
        };

        Workspace m_workspace;
        Result m_result;

        void Clear()
        {
            m_result.Clear();
            m_workspace.Clear();
        }

        static bool IsBetter(const Score& candidate, const Score& current)
        {
            return candidate.m_theta > current.m_theta
                || (candidate.m_theta == current.m_theta && candidate.m_numMatches > current.m_numMatches);
        }

        static bool CmpAtomLogOmega(const Atom* a, const Atom* b)
        {
            if (a->m_logAnalysisOmega == b->m_logAnalysisOmega)
            {
                return a < b;
            }

            return a->m_logAnalysisOmega < b->m_logAnalysisOmega;
        }

        float GetMatchTheta(AnalysisAtomArray& analysisAtoms, size_t atomIndex, size_t peakIndex) const
        {
            const Atom& atom = *m_workspace.m_atoms[atomIndex];
            const AnalysisAtom& peak = analysisAtoms[peakIndex];
            float density = m_workspace.m_densities[atomIndex];
            float distance = std::abs(peak.m_logAnalysisOmega - atom.m_logAnalysisOmega);
            if (distance > density
                || peak.m_analysisMagnitude / atom.m_synthesisMagnitude < x_mergeGainThreshold)
            {
                return -1.0f;
            }

            return AnalysisAtom::PreferredMatchTheta(peak, atom.m_logAnalysisOmega, density);
        }

        void ComputeRow(AnalysisAtomArray& analysisAtoms, size_t atomBegin, size_t atomEnd,
            size_t peakBegin, size_t peakEnd, bool reverse, Score* row)
        {
            size_t width = peakEnd - peakBegin;
            std::fill(row, row + width + 1, Score{});
            for (size_t offset = 0; offset < atomEnd - atomBegin; ++offset)
            {
                size_t atomIndex = reverse ? atomEnd - 1 - offset : atomBegin + offset;
                Score diagonal = row[0];
                for (size_t j = 1; j <= width; ++j)
                {
                    Score above = row[j];
                    Score best = IsBetter(row[j - 1], above) ? row[j - 1] : above;
                    size_t peakIndex = reverse ? peakEnd - j : peakBegin + j - 1;
                    float theta = GetMatchTheta(analysisAtoms, atomIndex, peakIndex);
                    if (theta >= 0.0f)
                    {
                        Score matched{diagonal.m_theta + static_cast<double>(theta), diagonal.m_numMatches + 1};
                        if (IsBetter(matched, best))
                        {
                            best = matched;
                        }
                    }

                    row[j] = best;
                    diagonal = above;
                }
            }
        }

        void FindMatches(AnalysisAtomArray& analysisAtoms, size_t atomBegin, size_t atomEnd,
            size_t peakBegin, size_t peakEnd)
        {
            if (atomBegin == atomEnd || peakBegin == peakEnd)
            {
                return;
            }

            if (atomEnd - atomBegin == 1)
            {
                Score best;
                for (size_t j = peakBegin; j < peakEnd; ++j)
                {
                    float theta = GetMatchTheta(analysisAtoms, atomBegin, j);
                    if (theta >= 0.0f)
                    {
                        Score candidate{static_cast<double>(theta), 1};
                        if (IsBetter(candidate, best))
                        {
                            best = candidate;
                            m_workspace.m_matches[atomBegin] = &analysisAtoms[j];
                        }
                    }
                }

                return;
            }

            // Maximize total theta, preferring more matches only when scores tie.
            // Hirschberg reconstruction uses two rows instead of a full table.
            //
            size_t atomMiddle = atomBegin + (atomEnd - atomBegin) / 2;
            size_t width = peakEnd - peakBegin;
            ComputeRow(analysisAtoms, atomBegin, atomMiddle, peakBegin, peakEnd, false, m_workspace.m_forward);
            ComputeRow(analysisAtoms, atomMiddle, atomEnd, peakBegin, peakEnd, true, m_workspace.m_backward);

            Score best;
            size_t split = 0;
            for (size_t j = 0; j <= width; ++j)
            {
                const Score& left = m_workspace.m_forward[j];
                const Score& right = m_workspace.m_backward[width - j];
                Score combined{left.m_theta + right.m_theta, left.m_numMatches + right.m_numMatches};
                if (IsBetter(combined, best))
                {
                    best = combined;
                    split = j;
                }
            }

            // Save the split before either recursive call reuses the rows.
            // The two disjoint peak ranges prevent crossing or shared matches.
            //
            FindMatches(analysisAtoms, atomBegin, atomMiddle, peakBegin, peakBegin + split);
            FindMatches(analysisAtoms, atomMiddle, atomEnd, peakBegin + split, peakEnd);
        }

        void Match(AtomsArrayWithIndex& atoms, AnalysisAtomArray& analysisAtoms, Input& input)
        {
            Clear();

            const size_t numAtoms = atoms.Size();
            const size_t numPeaks = analysisAtoms.Size();
            analysisAtoms.Sort(AnalysisAtom::CmpLogOmega);
            for (size_t i = 0; i < numAtoms; ++i)
            {
                m_workspace.m_atoms[i] = atoms[i];
            }

            std::sort(m_workspace.m_atoms, m_workspace.m_atoms + numAtoms, CmpAtomLogOmega);
            for (size_t i = 0; i < numAtoms; ++i)
            {
                m_workspace.m_densities[i] = input.m_omegaDensity.Process(m_workspace.m_atoms[i]->m_index);
            }

            for (AnalysisAtom& peak : analysisAtoms)
            {
                AnalysisAtomResult result;
                result.m_analysisAtom = &peak;
                m_result.m_analysisAtomResults.Add(result);
            }

            FindMatches(analysisAtoms, 0, numAtoms, 0, numPeaks);
            for (size_t i = 0; i < numAtoms; ++i)
            {
                Atom& atom = *m_workspace.m_atoms[i];
                AnalysisAtom* peak = m_workspace.m_matches[i];
                bool isMatch = peak != nullptr;
                if (isMatch)
                {
                    size_t peakIndex = static_cast<size_t>(peak - analysisAtoms.begin());
                    m_result.m_analysisAtomResults[peakIndex].m_isMatched = true;
                }
                else
                {
                    // Any current peak can dominate an unmatched old atom,
                    // including a peak that will subsequently become a new atom.
                    //
                    float density = m_workspace.m_densities[i];
                    float lowerLogOmega = atom.m_logAnalysisOmega - density;
                    float upperLogOmega = atom.m_logAnalysisOmega + density;
                    auto it = std::lower_bound(analysisAtoms.begin(), analysisAtoms.end(),
                        lowerLogOmega, AnalysisAtom::CmpLogOmegaFloat);
                    float bestTheta = -1.0f;
                    for (; it != analysisAtoms.end() && it->m_logAnalysisOmega <= upperLogOmega; ++it)
                    {
                        size_t peakIndex = static_cast<size_t>(it - analysisAtoms.begin());
                        float theta = GetMatchTheta(analysisAtoms, i, peakIndex);
                        if (theta >= 0.0f && theta > bestTheta)
                        {
                            bestTheta = theta;
                            peak = &*it;
                        }
                    }
                }

                SynthesisAtomResult result;
                result.m_atom = &atom;
                result.m_analysisAtom = peak;
                result.m_isMatch = isMatch;
                m_result.m_synthesisAtomResults.Add(result);
            }
        }
    };

    void TrackAnalysisAtoms(AnalysisAtomArray& analysisAtoms, Input& input)
    {
        m_matcher.Match(m_atoms, analysisAtoms, input);

        for (typename AtomMatcher::SynthesisAtomResult& synthesisAtomResult : m_matcher.m_result.m_synthesisAtomResults)
        {
            MergeAtom(*synthesisAtomResult.m_atom, synthesisAtomResult.m_analysisAtom, synthesisAtomResult.m_isMatch, input);
        }

        for (typename AtomMatcher::AnalysisAtomResult& analysisAtomResult : m_matcher.m_result.m_analysisAtomResults)
        {
            if (!analysisAtomResult.m_isMatched
                && x_deathMag <= analysisAtomResult.m_analysisAtom->m_analysisMagnitude)
            {
                AnalysisAtom& analysisAtom = *analysisAtomResult.m_analysisAtom;
                float slewUpAlpha = input.m_slewUpAlpha.Process(analysisAtom.m_index);
                float initMag = std::max(Slew::Process(0, analysisAtom.m_analysisMagnitude, slewUpAlpha), x_deathMag);
                Atom newAtom(
                    analysisAtom.m_analysisOmega,
                    analysisAtom.m_logAnalysisOmega,
                    analysisAtom.m_analysisMagnitude,
                    analysisAtom.m_analysisPhase,
                    analysisAtom.m_index,
                    analysisAtom.m_analysisOmega,
                    initMag,
                    0.0f);
                m_atoms.Add(newAtom);
            }
        }

        m_atoms.SortByReverseSynthesisMagnitude();
        m_atoms.ShrinkIfNecessary(input.m_numAtoms);
        while (!m_atoms.Empty()
            && (!std::isfinite(m_atoms.Back()->m_synthesisMagnitude)
                || m_atoms.Back()->m_synthesisMagnitude < x_deathMag))
        {
            m_atoms.Pop();
        }
    }

    void ExtractAtoms(Buffer& buffer, Input& input)
    {
        DFT dft;
        dft.Transform(buffer);
        AnalysisAtomArray analysisAtoms;
        ExtractAndSubtractAnalysisAtoms(dft, analysisAtoms, input);
        TrackAnalysisAtoms(analysisAtoms, input);
    }

    void ExtractAtomsAndResidual(Buffer& buffer, Input& input)
    {
        DFT dft;
        dft.Transform(buffer);
        AnalysisAtomArray analysisAtoms;
        ExtractAndSubtractAnalysisAtoms(dft, analysisAtoms, input);
        typename ResidualModel::Input residualInput;
        for (size_t i = 0; i < ResidualModel::x_numBuckets; ++i)
        {
            residualInput.m_analysisResidualMagnitudes[i] = std::abs(dft.m_components[i]);
        }

        m_residualModel.Process(input, residualInput);
        TrackAnalysisAtoms(analysisAtoms, input);
    }

    bool IsAtomAllocated(Atom* atom) const
    {
        return m_atoms.IsAtomAllocated(atom);
    }

    AtomsArrayWithIndex m_atoms;
    ResidualModel m_residualModel;
    AtomMatcher m_matcher;
};

using SpectralModel = SpectralModelGeneric<12, ScalarParameter>;

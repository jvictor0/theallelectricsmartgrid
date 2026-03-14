#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include "Array.hpp"

struct HarmonicSheaf
{
    static constexpr size_t x_rank = 3;
    static constexpr size_t x_dimension = 6;
    static constexpr size_t x_numBasePoints = 1 << x_dimension;

    struct BitVector
    {
        BitVector()
            : m_bits(0)
        {
        }

        BitVector(uint8_t bits)
            : m_bits(bits)
        {
        }
        
        bool Get(size_t i)
        {
            return (m_bits & (1 << i)) >> i;
        }

        void Set(size_t i, bool value)
        {
            if (value)
            {
                m_bits |= 1 << i;
            }
            else
            {
                m_bits &= ~(1 << i);
            }
        }

        size_t CountSetBits()
        {
            static const uint8_t x_bitsSet [16] =
                {
                    0, 1, 1, 2, 1, 2, 2, 3, 
                    1, 2, 2, 3, 2, 3, 3, 4
                };
            
            return x_bitsSet[m_bits & 0x0F] + x_bitsSet[m_bits >> 4];
        }

        std::string ToString() 
        {
            std::string ret;
            for (size_t i = 0; i < 6; ++i)
            {
                ret += Get(i) ? "1" : "0";
            }
            return ret;
        }

        bool operator==(BitVector other) const
        {
            return m_bits == other.m_bits;
        }

        bool operator!=(BitVector other) const
        {
            return m_bits != other.m_bits;
        }

        uint8_t m_bits;
    };

    struct Lens : public BitVector
    {
        Lens()
            : BitVector(0)
        {
        }

        Lens(uint8_t bits)
            : BitVector(bits)
        {
        }

        bool Equivalent(BitVector a, BitVector b) const
        {
            // Check if two input vectors are equivalent under the lens
            // This means they differ only in positions where the lens is 0
            //
            uint8_t diff = a.m_bits ^ b.m_bits;
            return (diff & m_bits) == 0;
        }

        BitVector Canonicalize(BitVector timeSlice) const
        {
            return BitVector(m_bits & timeSlice.m_bits);
        }
    };

    struct Section
    {
        uint8_t m_high[x_rank];
        uint8_t m_total[x_rank];

        Section()
        {
            for (size_t i = 0; i < x_rank; ++i)
            {
                m_high[i] = 0;
                m_total[i] = 0;
            }
        }

        float Coefficient(size_t i) const
        {
            return m_total[i] > 0 ? static_cast<float>(m_high[i]) / static_cast<float>(m_total[i]) : 0.0f;
        }

        Section GCD(Section other) const
        {
            Section result;
            for (size_t i = 0; i < x_rank; ++i)
            {
                result.m_high[i] = std::min(m_high[i], other.m_high[i]);
            }

            return result;
        }

        void Clear()
        {
            for (size_t i = 0; i < x_rank; ++i)
            {
                m_high[i] = 0;
                m_total[i] = 0;
            }
        }

        std::string ToString() const
        {
            std::string ret = "<";
            for (size_t i = 0; i < x_rank; ++i)
            {
                ret += std::to_string(m_high[i]) + "/" + std::to_string(m_total[i]) + ", ";
            }
            ret += ">";
            return ret;
        }

        bool operator==(Section other) const
        {
            for (size_t i = 0; i < x_rank; ++i)
            {
                if (m_high[i] != other.m_high[i])
                {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(Section other) const
        {
            return !(*this == other);
        }

        bool operator<(Section other) const
        {
            for (size_t i = 0; i < x_rank; ++i)
            {
                if (m_high[i] != other.m_high[i])
                {
                    return m_high[i] < other.m_high[i];
                }
            }

            return false;
        }

        size_t RotateAvoidOffset(size_t lane)
        {
            return lane % x_rank;
        }

        uint8_t Timbre256(size_t i) const
        {
            float c = Coefficient(i % x_rank);
            return static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, c * 255.0f)));
        }

        float Timbre(size_t i) const
        {
            return Coefficient(i % x_rank);
        }
    };

    struct Evaluator
    {
        float m_coefficients[x_rank];

        Evaluator()
        {
            for (size_t i = 0; i < x_rank; ++i)
            {
                m_coefficients[i] = 0.0f;
            }
        }
        
        float Evaluate(Section& section)
        {
            float sum = 0.0f;
            for (size_t i = 0; i < x_rank; ++i)
            {
                sum += m_coefficients[i] * section.m_high[i];
            }

            return sum;
        }
    };
    
    struct SectionWithValue
    {
        Section m_section;
        float m_value;

        SectionWithValue()
          : m_section()
          , m_value(0.0f)
        {        
        }

        SectionWithValue(Section section, Evaluator& evaluator)
        {
            m_section = section;
            m_value = evaluator.Evaluate(section);
        }

        bool operator<(SectionWithValue other) const
        {
            return m_value < other.m_value;
        }            

        bool operator==(SectionWithValue other) const
        {
            return m_section == other.m_section && m_value == other.m_value;
        }

        bool operator!=(SectionWithValue other) const
        {
            return !(*this == other);
        }

        std::string ToString() const
        {
            return m_section.ToString() + " (" + std::to_string(m_value) + ")";
        }
    };

    struct Sheaf
    {
        struct UIState
        {
            std::atomic<Section> m_sections[x_numBasePoints];
        };

        static_assert(sizeof(Section) == x_rank * 2);

        Section m_sections[x_numBasePoints];

        Sheaf()
        {
            for (size_t i = 0; i < x_numBasePoints; ++i)
            {
                m_sections[i] = Section{};
            }
        }
        
        void Set(BitVector index, Section& section)
        {
            m_sections[index.m_bits] = section;
        }

        Section& Get(BitVector index)
        {
            return m_sections[index.m_bits];
        }

        float Evaluate(BitVector index, Evaluator& evaluator)
        {
            return evaluator.Evaluate(Get(index));
        }

    };

    struct TimeSliceOrdinalConverter
    {
        Lens m_lens;
        size_t m_lensDimension;
        size_t m_forwardingIndices[x_dimension];

        TimeSliceOrdinalConverter()
            : m_lens(0)
            , m_lensDimension(0)
        {
        }

        TimeSliceOrdinalConverter(Lens lens)
            : m_lens(0)
            , m_lensDimension(0)
            , m_forwardingIndices{}
        {
            SetLens(lens);
        }

        void SetLens(Lens lens)
        {
            m_lens = lens;
            m_lensDimension = lens.CountSetBits();
            
            size_t j = 0;
            for (size_t i = 0; i < m_lensDimension; ++i)
            {
                while (!m_lens.Get(j))
                {
                    ++j;
                }
                
                m_forwardingIndices[i] = j;
                ++j;
            }
        }

        BitVector Convert(uint8_t ordinal)
        {
            BitVector result(0);
            // Shift the bits of ordinal into the unset positions of the lens.
            //
            for (size_t i = 0; i < m_lensDimension; ++i)
            {
                result.Set(m_forwardingIndices[i], BitVector(ordinal).Get(i));
            }
            
            return result;
        }
    };

    struct TimeSliceClassOrdinalConverter
    {
        Lens m_lens;
        size_t m_lensCoDimension;
        size_t m_forwardingIndices[x_dimension];

        TimeSliceClassOrdinalConverter()
            : m_lens(0)
            , m_lensCoDimension(0)
        {
        }

        TimeSliceClassOrdinalConverter(Lens lens)
            : m_lens(0)
            , m_lensCoDimension(0)
            , m_forwardingIndices{}
        {
            SetLens(lens);
        }

        void SetLens(Lens lens)
        {
            m_lens = lens;
            m_lensCoDimension = x_dimension - lens.CountSetBits();
            
            size_t j = 0;
            for (size_t i = 0; i < m_lensCoDimension; ++i)
            {
                while (m_lens.Get(j))
                {
                    ++j;
                }
                
                m_forwardingIndices[i] = j;
                ++j;
            }
        }

        BitVector Convert(BitVector representative, uint8_t ordinal)
        {
            // Shift the bits of ordinal into the set positions of the lens.
            //
            for (size_t i = 0; i < m_lensCoDimension; ++i)
            {
                representative.Set(m_forwardingIndices[i], BitVector(ordinal).Get(i));
            }

            return representative;
        }
    };

    struct TimeSliceClassIterator
    {
        uint8_t m_ordinal = 0;
        BitVector m_defaultVector;
        TimeSliceClassOrdinalConverter m_converter;

        TimeSliceClassIterator(Lens lens, BitVector defaultVector)
            : m_defaultVector(defaultVector)
            , m_converter(lens)
        {
        }

        BitVector Get()
        {
            return m_converter.Convert(m_defaultVector, m_ordinal);
        }
        
        void Next()
        {
            ++m_ordinal;
        }
        
        bool Done()
        {
            return (1 << m_converter.m_lensCoDimension) <= m_ordinal;
        }        
    };

    struct TimeSliceIterator
    {
        uint8_t m_ordinal = 0;
        TimeSliceOrdinalConverter m_converter;

        TimeSliceIterator(Lens lens)
            : m_converter(lens)
        {
        }

        BitVector Get()
        {
            return m_converter.Convert(m_ordinal);
        }
        
        void Next()
        {
            ++m_ordinal;
        }
        
        bool Done()
        {
            return (1 << m_converter.m_lensDimension) <= m_ordinal;
        }        
    };

    enum class SectionChoiceStrategy : int
    {
        None = 0,
        Lowest = 1,
        GCD = 2,
        Closest = 3,
        ClosestModOne = 4,
        Percentile = 5
    };

    struct SectionChooser
    {
        SectionChoiceStrategy m_strategy;
        Evaluator m_evaluator;
        float m_choiceArg;

        SectionChooser()
         : m_strategy(SectionChoiceStrategy::None)
         , m_evaluator()
         , m_choiceArg(0.0f)
        {
        }

        SectionWithValue ChooseLowest(Sheaf& sheaf, Lens lens, BitVector defaultVector)
        {
            Section lowestSection;
            float lowestScore = std::numeric_limits<float>::max();
            TimeSliceClassIterator iterator(lens, defaultVector);
            while (!iterator.Done())
            {
                BitVector index = iterator.Get();
                Section section = sheaf.Get(index);
                float score = m_evaluator.Evaluate(section);
                if (score < lowestScore)
                {
                    lowestScore = score;
                    lowestSection = section;
                }

                iterator.Next();
            }

            return SectionWithValue(lowestSection, m_evaluator);
        }

        SectionWithValue ChooseGCD(Sheaf& sheaf, Lens lens, BitVector defaultVector)
        {
            Section gcdSection;
            TimeSliceClassIterator iterator(lens, defaultVector);
            while (!iterator.Done())
            {
                BitVector index = iterator.Get();
                Section section = sheaf.Get(index);
                gcdSection = gcdSection.GCD(section);
                iterator.Next();
            }

            return SectionWithValue(gcdSection, m_evaluator);
        }

        SectionWithValue ChooseClosest(Sheaf& sheaf, Lens lens, BitVector defaultVector, float target, bool modOne)
        {
            Section closestSection;
            float closestScore = std::numeric_limits<float>::max();
            TimeSliceClassIterator iterator(lens, defaultVector);
            float targetReduced = modOne ? target - std::floor(target) : target;
            while (!iterator.Done())
            {
                BitVector index = iterator.Get();
                Section section = sheaf.Get(index);
                float value = m_evaluator.Evaluate(section);
                value = modOne ? value - std::floor(value) : value;
                float score = std::abs(value - targetReduced);
                score = modOne && score > 0.5f ? 1.0f - score : score;
                if (score < closestScore)
                {
                    closestScore = score;
                    closestSection = section;
                }

                iterator.Next();
            }

            SectionWithValue result(closestSection, m_evaluator);
            if (modOne)
            {
                result.m_value = result.m_value - std::floor(result.m_value) + std::floor(target);
                if (std::abs(result.m_value - target + 1) < std::abs(result.m_value - target))
                {
                    result.m_value += 1;
                }
                else if (std::abs(result.m_value - target - 1) < std::abs(result.m_value - target))
                {
                    result.m_value -= 1;
                }
            }

            return result;
        }

        SectionWithValue ChoosePercentile(Sheaf& sheaf, Lens lens, BitVector defaultVector, float percentile)
        {
            Array<SectionWithValue, x_numBasePoints> sheafValues;
            TimeSliceClassIterator iterator(lens, defaultVector);
            while (!iterator.Done())
            {
                BitVector index = iterator.Get();
                Section section = sheaf.Get(index);
                sheafValues.Add(SectionWithValue(section, m_evaluator));
                iterator.Next();
            }

            sheafValues.Sort();
            float fractional = percentile - std::floor(percentile);
            size_t ix = static_cast<size_t>(fractional * sheafValues.Size());
            ix = std::min(ix, sheafValues.Size() - 1);
            SectionWithValue result(sheafValues[ix].m_section, m_evaluator);
            result.m_value += std::floor(percentile);
            return result;
        }

        SectionWithValue Choose(Sheaf& sheaf, Lens lens, BitVector defaultVector)
        {
            switch (m_strategy)
            {
                case SectionChoiceStrategy::None:
                    return SectionWithValue(Section(), m_evaluator);
                case SectionChoiceStrategy::Lowest:
                    return ChooseLowest(sheaf, lens, defaultVector);
                case SectionChoiceStrategy::GCD:
                    return ChooseGCD(sheaf, lens, defaultVector);
                case SectionChoiceStrategy::Closest:
                    return ChooseClosest(sheaf, lens, defaultVector, m_choiceArg, false);
                case SectionChoiceStrategy::ClosestModOne:
                    return ChooseClosest(sheaf, lens, defaultVector, m_choiceArg, true);
                case SectionChoiceStrategy::Percentile:
                    return ChoosePercentile(sheaf, lens, defaultVector, m_choiceArg);
            }

            return SectionWithValue(Section(), m_evaluator);
        }
    };
};
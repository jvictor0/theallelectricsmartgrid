#pragma once
#include "plugin.hpp"
#include <cstddef>
#include "LameJuisConstants.hpp"

struct LameJuisInternal
{
    static constexpr size_t x_maxPoly = 16;

    static constexpr size_t x_numInputs = 6;
    static constexpr size_t x_numOperations = 6;
    static constexpr size_t x_numAccumulators = 3;
    
    struct InputBit
    {
        bool m_value;
        uint8_t m_counter;
        bool m_changed;
        InputBit* m_prev;

        struct Input
        {
            Input()
                : m_connected(false)
                , m_value(false)
            {
            }
            
            bool m_connected;
            bool m_value;
            bool* m_reset;
        };

        void Init(InputBit* prev)
        {
            m_value = false;
            m_counter = 0;
            m_changed = false;
            m_prev = prev;
        }
        
        void Process(Input& input)
        {                        
            bool oldValue = m_value;
            m_changed = false;

            if (*input.m_reset)
            {
                m_counter = 0;
                m_changed = true;
            }
            
            // If a cable is connected, use that value.
            //
            if (input.m_connected)
            {
                m_value = input.m_value;
                if (m_value && !oldValue)
                {                    
                    // Count down instead of up so each input is high on its "even" beats.
                    //
                    --m_counter;
                }
            }
            
            // Each input (except the first) is normaled to divide-by-two of the previous input,
            // but in order to keep the phases in line, they are actually a divide-by-2-to-the-n
            // of first plugged input above, where n is the number of unplugged inputs between.
            // This is easiest to implement as cascading counters.
            //
            else if (m_prev)
            { 
                m_value = m_prev->m_counter % 2 != 0;
                m_counter = m_prev->m_counter / 2;
            }
            
            if (oldValue != m_value)
            {
                m_changed = true;
            }
        }
    };

    enum class MatrixSwitch : char
    {
        Inverted = 0,
        Muted = 1,
        Normal = 2
    };

    struct InputVector
    {
        InputVector()
            : m_bits(0)
        {
        }

        InputVector(uint8_t bits)
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

        uint8_t m_bits;
    };
    
    struct LogicOperation
    {
        enum class Operator : char
        {
            Or = 0,
            And = 1,
            Xor = 2,
            AtLeastTwo = 3,
            Majority = 4,
            Off = 5,
            Direct,
            NumOperations = 7,
        };

        static std::vector<std::string> GetLogicNames()
        {
            return std::vector<std::string>({
                    "OR",
                    "AND",
                    "XOR",
                    "At least two",
                    "Majority",
                    "Off"
                });
        }
        
        enum class SwitchVal : char
        {
            Down = 0,
            Middle = 1,
            Up = 2
        };

        struct Input
        {
            Operator m_operator;
            bool m_direct[x_numInputs + 1];
            SwitchVal m_switch;
            MatrixSwitch m_elements[x_numInputs];
            InputVector* m_inputVector;

            Input()
            {
                m_operator = Operator::And;
                m_switch = SwitchVal::Up;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    m_elements[i] = MatrixSwitch::Muted;
                }

                for (size_t i = 0; i < x_numInputs + 1; ++i)
                {
                    m_direct[i] = false;
                }                
            }
        };
                
        void SetBitVectors()
        {
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                MatrixSwitch switchVal = m_elements[i];
                m_active.Set(i, switchVal != MatrixSwitch::Muted);
                m_inverted.Set(i, switchVal == MatrixSwitch::Inverted);
            }
        }
        
        void Init(LameJuisInternal* owner)
        {
            m_operator = Operator::And;
            m_switch = SwitchVal::Up;
            m_gate = false;
            m_countTotal = 0;
            m_countHigh = 0;
            m_owner = owner;

            for (size_t i = 0; i < x_numInputs; ++i)
            {
                m_elements[i] = MatrixSwitch::Muted;
                m_highParticipant[i] = false;
            }

            for (size_t i = 0; i < x_numInputs + 1; ++i)
            {
                m_direct[i] = false;
            }
        }

        void GetTotalAndHigh(InputVector inputVector, size_t* countTotal, size_t* countHigh)
        {
            // And with m_active to mute the muted inputs.
            // Xor with m_inverted to invert the inverted ones.
            //
            inputVector.m_bits &= m_active.m_bits;
            inputVector.m_bits ^= m_inverted.m_bits;
            
            *countTotal = m_active.CountSetBits();
            *countHigh = inputVector.CountSetBits();                        
        }

        bool ComputeOperation(size_t countTotal, size_t countHigh)
        {
            bool ret = false;
            switch (m_operator)
            {
                case Operator::Or: ret = (countHigh > 0); break;
                case Operator::And: ret = (countHigh == countTotal); break;
                case Operator::Xor: ret = (countHigh % 2 == 1); break;
                case Operator::AtLeastTwo: ret = (countHigh >= 2); break;
                case Operator::Majority: ret = (2 * countHigh > countTotal); break;
                case Operator::Off: ret = false; break;
                case Operator::Direct:
                {
                    ret = m_direct[countHigh];
                    break;
                }
                case Operator::NumOperations: ret = false; break;
            }
            
            return ret;
        }

        bool GetValue(InputVector inputVector)
        {
            size_t countTotal;
            size_t countHigh;
            GetTotalAndHigh(inputVector, &countTotal, &countHigh);            
            return ComputeOperation(countTotal, countHigh);
        }

        // Up is output zero but input id 2, so invert.
        //
        size_t GetOutputTarget()
        {
            return x_numAccumulators - static_cast<size_t>(m_switch) - 1;
        }

        void Process(Input& input)
        {
            bool anyEffect = false;
            bool switchChange = false;

            for (size_t i = 0; i < x_numInputs; ++i)
            {
                if (m_owner->m_inputs[i].m_changed && 
                    (m_elements[i] != MatrixSwitch::Muted ||
                     input.m_elements[i] != MatrixSwitch::Muted))
                {
                    anyEffect = true;
                    if (m_elements[i] != input.m_elements[i])
                    {
                        m_elements[i] = input.m_elements[i];
                        m_owner->m_needsInvalidateCache = true;
                        switchChange = true;
                    }
                }
            }

            if (switchChange)
            {
                SetBitVectors();
            }

            if (anyEffect)
            {
                if (m_operator != input.m_operator)
                {
                    m_operator = input.m_operator;
                    m_owner->m_needsInvalidateCache = true;
                }

                for (size_t i = 0; i < x_numInputs + 1; ++i)
                {
                    if (m_direct[i] != input.m_direct[i])
                    {
                        m_direct[i] = input.m_direct[i];
                        m_owner->m_needsInvalidateCache = true;
                    }
                }
                
                if (m_switch != input.m_switch)
                {
                    m_switch = input.m_switch;
                    m_owner->m_needsInvalidateCache = true;
                }

                GetTotalAndHigh(*input.m_inputVector, &m_countTotal, &m_countHigh);            
                m_gate = ComputeOperation(m_countTotal, m_countHigh);                
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    bool up = input.m_inputVector->Get(i);
                    bool proper = m_inverted.Get(i) ? !up : up;
                    m_highParticipant[i] = m_gate && proper && m_active.Get(i);
                }
            }
        }

        InputVector m_active;
        InputVector m_inverted;

        Operator m_operator;
        bool m_direct[x_numInputs + 1];
        SwitchVal m_switch;
        size_t m_countTotal;
        size_t m_countHigh;
        bool m_highParticipant[x_numInputs];
        bool m_gate;

        LameJuisInternal* m_owner;

        MatrixSwitch m_elements[x_numInputs];
    };

    struct Accumulator
    {        
        enum class Interval
        {
            Off = 0,
            Octave = 1,
            PerfectFifth = 2,
            MajorThird = 3,
            PerfectFourth = 4,
            MinorThird = 5,            
            WholeStep = 6,
            HalfStep = 7,
            SevenHarm = 8,
            ElevenHarm = 9,
            ThirteenHarm = 10,
            ThirtyOneHarm = 11,
            NumIntervals = 12,
        };

        struct Input
        {
            bool m_12EDOMode;
            Interval m_interval;
            float m_intervalOffset;
            
            Input()
                : m_12EDOMode(false)
                , m_interval(Interval::Off)
                , m_intervalOffset(0)
            {
            }
        };        

        static std::vector<std::string> GetIntervalNames()
        {
            return std::vector<std::string>({
                    "Off",
                    "Octave",
                    "Perfect Fifth",
                    "Major Third",
                    "Perfect Fourth",
                    "Minor Third",
                    "Whole Step",
                    "Half Step",
                    "Seventh Harmonic",
                    "Eleventh Harmonic",
                    "Thirteenth Harmonic",
                    "Thirty-First Harmonic"});
        }
        
        static constexpr float x_voltages[] = {
            0 /*Off*/,
            1.0 /*octave = log_2(2)*/,
            0.5849625007211562 /*pefect fifth = log_2(3/2)*/,
            0.32192809488736235 /*major third = log_2(5/4)*/,
            0.4150374992788437 /*perfect fourth = log_2(4/3)*/,
            0.2630344058337938 /*minor third = log_2(6/5)*/,
            0.16992500144231237/*whole tone = log_2(9/8)*/,
            0.09310940439 /*half step = log_2(16/15)*/,
            0.8073549220576041 /*log_2(7/4)*/,
            0.45943161863 /*log_2(11/8)*/,
            0.70043971814 /*log_2(13/8)*/,
            0.95419631038 /*log_2(31/16)*/,
        };

        // Fake semitones map for the expander.
        //
        static constexpr int x_semitones[] = {
            0 /*Off*/,
            0 /*octave*/,
            7 /*pefect fifth*/,
            4 /*major third*/,
            5 /*perfect fourth*/,
            3 /*minor third*/,
            2 /*whole tone*/,
            1 /*half step*/,
            0 /*7/4*/,
            0 /*11/8*/,
            0 /*13/8*/,
            0 /*31/16*/,
        };

        static constexpr size_t x_end12EDOLikeIx = 8;
        
        Interval m_interval;
        float m_intervalOffset;
        float m_intervalValue;
        bool m_12EDOMode;
        LameJuisInternal* m_owner;
        
        int GetSemitones()
        {
            return x_semitones[static_cast<int>(m_interval)];
        }

        bool Is12EDOLike()
        {
            // More sophisticated analysis could work here, but honestly if the user provides an analog
            // interval just have the expander display in cents (that is, say its not 12-EDO-like).
            //
            return m_intervalOffset == 0 &&
                static_cast<size_t>(m_interval) < x_end12EDOLikeIx;
        }

        void Process(Input& input)
        {
            bool change = false;
            if (m_interval != input.m_interval)
            {
                change = true;
                m_interval = input.m_interval;
            }

            if (m_intervalOffset != input.m_intervalOffset)
            {
                change = true;
                m_intervalOffset = input.m_intervalOffset;
            }

            if (m_12EDOMode != input.m_12EDOMode)
            {
                change = true;
                m_12EDOMode = input.m_12EDOMode;
            }

            if (change)
            {
                m_intervalValue = x_voltages[static_cast<size_t>(m_interval)] + m_intervalOffset;
                if (m_12EDOMode)
                {
                    m_intervalValue = static_cast<float>(static_cast<int>(m_intervalValue * 12 + 0.5)) / 12.0;
                }

                m_owner->m_needsInvalidateCache = true;
            }
        }

        void Init(LameJuisInternal* owner)
        {
            m_owner = owner;
            m_interval = Interval::Off;
            m_intervalOffset = 0;
            m_intervalValue = 0;
            m_12EDOMode = false;
        }
    };

    struct MatrixEvalResult
    {
        MatrixEvalResult()
        {
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                m_high[i] = 0;
                m_total[i] = 0;
            }
        }

        void Clear()
        {
            memset(m_high, 0, x_numAccumulators);
            memset(m_total, 0, x_numAccumulators);
        }

        float ComputePitch(Accumulator* accumulators) const
        {
            float result = 0;
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                result += accumulators[i].m_intervalValue * m_high[i];
            }

            return result;
        }

        bool operator==(const MatrixEvalResult& other)
        {
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                if (m_high[i] != other.m_high[i])
                {
                    return false;
                }
            }

            return true;
        }

        bool operator!=(const MatrixEvalResult& other)
        {
            return !(*this == other);
        }

        bool operator<(const MatrixEvalResult& other) const
        {            
            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                if (m_high[i] != other.m_high[i])
                {
                    return m_high[i] < other.m_high[i];
                }
            }

            return false;
        }

        float Timbre(size_t i) const
        {
            if (m_high[i] == 0)
            {
                return 0;
            }
            else if (m_high[i] == m_total[i])
            {
                return 1;
            }
            else
            {
                return static_cast<float>(m_high[i]) / m_total[i];
            }
        }

        uint8_t Timbre256(size_t i) const
        {
            if (m_high[i] == 0)
            {
                return 0;
            }
            else if (m_high[i] == m_total[i])
            {
                return 255;
            }
            else
            {
                return static_cast<uint8_t>(m_high[i] * 255 / m_total[i]);
            }
        }
        
        uint8_t m_high[x_numAccumulators];
        uint8_t m_total[x_numAccumulators];
    };

    void ClearCaches()
    {        
        memset(m_isEvaluated, 0, sizeof(m_isEvaluated));
        ClearOutputCaches();
        m_needsInvalidateCache = false;
    }

    void ClearOutputCaches()
    {                
        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_outputs[i].ClearOutputCaches();
        }
    }

    void ClearLastStep()
    {               
        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_outputs[i].ClearLastStep();
        }        
    }

    struct MatrixEvalResultWithPitch
    {
        MatrixEvalResultWithPitch()
            : m_pitch(0)
        {
        }

        MatrixEvalResultWithPitch(
            const MatrixEvalResult& result,
            Accumulator* accumulators)
            : m_result(result)
            , m_pitch(result.ComputePitch(accumulators))
        {
        }

        bool operator<(const MatrixEvalResultWithPitch& other) const
        {
            return std::tie(m_pitch, m_result) < std::tie(other.m_pitch, other.m_result);
        }

        bool operator<(float pitch) const
        {
            return m_pitch < pitch;
        }

        void OctaveReduce()
        {
            m_pitch = m_pitch - std::floor(m_pitch);
        }

        float Timbre(size_t i) const
        {
            return m_result.Timbre(i);
        }
        
        MatrixEvalResult m_result;
        float m_pitch;
    };

    MatrixEvalResult EvalMatrixBase(InputVector inputVector)
    {
        MatrixEvalResult& result = m_evalResults[inputVector.m_bits];
        
        if (!m_isEvaluated[inputVector.m_bits])
        {
            result.Clear();
            for (size_t i = 0; i < x_numOperations; ++i)
            {
                size_t outputId = m_operations[i].GetOutputTarget();
                bool isHigh = m_operations[i].GetValue(inputVector);
                ++result.m_total[outputId];
                if (isHigh)
                {
                    ++result.m_high[outputId];
                }
            }
            
            m_isEvaluated[inputVector.m_bits] = true;
        }
        
        return result;
    }

    MatrixEvalResultWithPitch EvalMatrix(InputVector inputVector)
    {
        return MatrixEvalResultWithPitch(EvalMatrixBase(inputVector), m_accumulators);
    }

    struct TimeSliceOrdinalConverter
    {
        InputVector m_lens;
        size_t m_lensCoDimension;
        size_t m_forwardingIndices[x_numInputs];

        TimeSliceOrdinalConverter()
            : m_lens(0)
            , m_lensCoDimension(0)
        {
        }

        TimeSliceOrdinalConverter(InputVector lens)
        {
            SetLens(lens);
        }

        void SetLens(InputVector lens)
        {
            m_lens = lens;
            m_lensCoDimension = x_numInputs - lens.CountSetBits();
            
            size_t j = 0;
            for (size_t i = 0; i < m_lensCoDimension; ++i)
            {
                while (!m_lens.Get(j))
                {
                    ++j;
                }
                
                m_forwardingIndices[i] = j;
                ++j;
            }
        }

        InputVector Convert(uint8_t ordinal)
        {
            InputVector result(0);
            // Shift the bits of ordinal into the unset positions of the lens.
            //
            for (size_t i = 0; i < m_lensCoDimension; ++i)
            {
                result.Set(m_forwardingIndices[i], InputVector(ordinal).Get(i));
            }
            
            return result;
        }
    };

    struct TimeSliceClassOrdinalConverter
    {
        InputVector m_lens;
        size_t m_lensCoDimension;
        size_t m_forwardingIndices[x_numInputs];

        TimeSliceClassOrdinalConverter()
            : m_lens(0)
            , m_lensCoDimension(0)
        {
        }

        TimeSliceClassOrdinalConverter(InputVector lens)
        {
            SetLens(lens);
        }

        void SetLens(InputVector lens)
        {
            m_lens = lens;
            m_lensCoDimension = x_numInputs - lens.CountSetBits();
            
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

        InputVector Convert(InputVector representative, uint8_t ordinal)
        {
            // Shift the bits of ordinal into the set positions of the lens.
            //
            for (size_t i = 0; i < m_lensCoDimension; ++i)
            {
                representative.Set(m_forwardingIndices[i], InputVector(ordinal).Get(i));
            }

            return representative;
        }
    };

    struct TimeSliceClassIterator
    {
        uint8_t m_ordinal = 0;
        InputVector m_defaultVector;
        TimeSliceClassOrdinalConverter m_converter;

        TimeSliceClassIterator(InputVector lens, InputVector defaultVector)
            : m_defaultVector(defaultVector)
            , m_converter(lens)
        {
        }

        InputVector Get()
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

        TimeSliceIterator(InputVector lens)
            : m_converter(lens)
        {
        }

        InputVector Get()
        {
            return m_converter.Convert(m_ordinal);
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

    struct GridSheafView
    {
        static constexpr size_t x_gridSizeBits = 3;
        static constexpr size_t x_gridSize = 1 << x_gridSizeBits;
        
        struct CellInfo
        {
            InputVector m_baseTimeSlice;
            bool m_isCurrentSlice;
            MatrixEvalResult m_localHarmonicPosition;
            bool m_isPlaying[x_maxPoly];
        };   

        struct TimeSliceXYConverter
        {
            InputVector m_lens;
            size_t m_lensCoDimension;
            size_t m_forwardingIndex[x_numInputs];

            void SetLens(InputVector lens)
            {
                m_lens = lens;
                m_lensCoDimension = x_numInputs - lens.CountSetBits();
                
                // First populate a backward index
                //
                size_t j = 0;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    if (!lens.Get(i))
                    {
                        m_forwardingIndex[i] = j / 2 + x_gridSizeBits * (j % 2);
                        ++j;
                    }
                    else
                    {
                        size_t readIx = i - j;
                        m_forwardingIndex[i] = readIx + (m_lensCoDimension + 1) / 2 < x_gridSizeBits
                                         ? readIx + (m_lensCoDimension + 1) / 2
                                         : readIx + m_lensCoDimension;
                    }
                }
            }

            InputVector Convert(uint8_t x, uint8_t y) 
            {
                InputVector xy(x | (y << x_gridSizeBits));
                InputVector result;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    result.Set(i, xy.Get(m_forwardingIndex[i]));
                }

                return result;
            }
        };

        TimeSliceXYConverter m_timeSliceXYConverter;
        InputVector m_cellBaseTimeSlices[x_gridSize][x_gridSize];

        bool LensEquivalent(InputVector a, InputVector b)
        {
            // Check if two input vectors are equivalent under the lens
            // This means they differ only in positions where the lens is 0
            //
            uint8_t diff = a.m_bits ^ b.m_bits;
            return (diff & m_timeSliceXYConverter.m_lens.m_bits) == 0;
        }

        void SetLens(InputVector lens)
        {
            m_timeSliceXYConverter.SetLens(lens);
            ComputeCells();
        }

        void ComputeCells()
        {
            for (uint8_t x = 0; x < x_gridSize; ++x)
            {
                for (uint8_t y = 0; y < x_gridSize; ++y)
                {
                    m_cellBaseTimeSlices[x][y] = m_timeSliceXYConverter.Convert(x, y);
                }
            }
        }   
    };   

    struct Output
    {
        struct CoMuteState
        {
            struct Input
            {
                bool m_coMutes[x_numInputs];
                size_t m_polyChans;
                float m_percentiles[x_maxPoly];
                bool m_harmonic[x_maxPoly];
                bool m_usePercentile[x_maxPoly];
                float m_toQuantize[x_maxPoly];
                int m_octave[x_maxPoly];
                
                Input()
                {                
                    m_polyChans = 0;
                    for (size_t i = 0; i < x_numInputs; ++i)
                    {
                        m_coMutes[i] = false;
                    }
                    
                    for (size_t i = 0; i < x_maxPoly; ++i)
                    {
                        m_percentiles[i] = 0;
                        m_harmonic[i] = false;
                        m_usePercentile[i] = false;
                        m_toQuantize[i] = 0;
                        m_octave[i] = 0;
                    }
                }
            };
            
            InputVector GetLens()
            {                                
                InputVector result;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    result.Set(i, !m_coMutes[i]);
                }

                return result;
            }
            
            void Init(Output* owner)
            {                                
                m_polyChans = 0;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    m_coMutes[i] = false;
                }

                for (size_t i = 0; i < x_maxPoly; ++i)
                {
                    m_percentiles[i] = 0;
                    m_harmonic[i] = false;
                    m_usePercentile[i] = false;
                    m_toQuantize[i] = 0;
                    m_octave[i] = 0;
                }

                m_owner = owner;
            }
            
            void Process(Input& input)
            {                               
                m_polyChans = input.m_polyChans;
                
                for (size_t i = 0; i < m_polyChans; ++i)
                {
                    m_percentiles[i] = input.m_percentiles[i];
                    m_harmonic[i] = input.m_harmonic[i];
                    m_usePercentile[i] = input.m_usePercentile[i];
                    m_toQuantize[i] = input.m_toQuantize[i];
                    m_octave[i] = input.m_octave[i];
                }
                
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    if (m_coMutes[i] != input.m_coMutes[i] &&
                        m_owner->m_owner->m_inputs[i].m_changed)
                    {
                        m_coMutes[i] = input.m_coMutes[i];
                        m_owner->m_needsInvalidateCache = true;
                    }
                }
            }
            
            bool m_coMutes[x_numInputs];
            float m_percentiles[x_maxPoly];
            bool m_harmonic[x_maxPoly];
            bool m_usePercentile[x_maxPoly];
            float m_toQuantize[x_maxPoly];
            int m_octave[x_maxPoly];
            size_t m_polyChans;
            Output* m_owner;
        };

        template<bool Harmonic>
        struct CacheForSingleInputVector
        {
            CacheForSingleInputVector() :
                m_isEvaluated(false)
            {
            }
            
            void ClearCache()
            {
                m_isEvaluated = false;
            }
            
            MatrixEvalResultWithPitch ComputePitch(
                LameJuisInternal* matrix,
                LameJuisInternal::Output* output,
                LameJuisInternal::InputVector defaultVector,
                size_t chan)
            {
                Eval(matrix, output, defaultVector);

                int octave = output->m_coMuteState.m_octave[chan];
                MatrixEvalResultWithPitch result;
                if (output->m_coMuteState.m_usePercentile[chan])
                {
                    result = m_cachedResults[PercentileToIx(output->m_coMuteState.m_percentiles[chan])];
                }
                else 
                {
                    result = Quantize(output->m_coMuteState.m_toQuantize[chan]);
                }

                result.m_pitch += octave;
                return result;
            }

            void Eval(
                LameJuisInternal* matrix,
                LameJuisInternal::Output* output,
                LameJuisInternal::InputVector defaultVector)
            {
                if (!m_isEvaluated)
                {
                    TimeSliceClassIterator itr = output->GetInputVectorIterator(defaultVector);
                    for (; !itr.Done(); itr.Next())
                    {
                        m_cachedResults[itr.m_ordinal] = matrix->EvalMatrix(itr.Get());
                    }
                    
                    m_numResults = itr.m_ordinal;

                    if (!Harmonic)
                    {
                        for (size_t i = 0; i < m_numResults; ++i)
                        {                            
                            m_cachedResults[i].OctaveReduce();
                        }
                    }
                    
                    std::sort(m_cachedResults, m_cachedResults + m_numResults);

                    size_t cur = 0;
                    m_resultOrd[0] = 0;
                    m_reverseIndex[0] = 0;

                    for (size_t i = 1; i < m_numResults; ++i)
                    {
                        if (m_cachedResults[i].m_pitch != m_cachedResults[i - 1].m_pitch)
                        {
                            ++cur;
                            m_reverseIndex[cur] = i;
                        }

                        m_resultOrd[i] = cur;
                    }
                    
                    m_numDistinctResults = cur + 1;
                    
                    m_isEvaluated = true;
                }
            }
            
            ssize_t PercentileToIx(float percentile)
            {
                ssize_t ix = static_cast<size_t>(percentile * m_numResults);
                ix = std::min<ssize_t>(ix, m_numResults - 1);
                ix = std::max<ssize_t>(ix, 0);
                return ix;
            }             

            MatrixEvalResultWithPitch Quantize(float pitch)
            {
                int octave = std::floor(pitch);
                float pitchModOctave = pitch - octave;
                MatrixEvalResultWithPitch result = QuantizeModOctave(pitchModOctave);
                result.m_pitch += octave;
                return result;
            }

            MatrixEvalResultWithPitch QuantizeModOctave(float pitch)
            {
                auto itr = std::lower_bound(m_cachedResults, m_cachedResults + m_numResults, pitch);
                if (itr == m_cachedResults && itr->m_pitch - pitch > pitch + 1 - m_cachedResults[m_numResults - 1].m_pitch)
                {
                    return m_cachedResults[m_numResults - 1];
                }
                else if (itr == m_cachedResults + m_numResults) 
                {
                    if (pitch - m_cachedResults[m_numResults - 1].m_pitch > m_cachedResults[0].m_pitch - pitch + 1)
                    {
                        return m_cachedResults[0];
                    }
                    else
                    {
                        return m_cachedResults[m_numResults - 1];
                    }
                }
                else
                {
                    return *itr;
                }
            }
            
            MatrixEvalResultWithPitch m_cachedResults[1 << x_numInputs];
            size_t m_resultOrd[1 << x_numInputs];
            size_t m_reverseIndex[1 << x_numInputs];
            size_t m_numResults;
            size_t m_numDistinctResults;
            bool m_isEvaluated;
        };

        struct Input
        {
            CoMuteState::Input m_coMuteInput;
            InputVector* m_prevVector;
            InputVector* m_inputVector;
        };

        bool m_needsInvalidateCache;
        bool m_trigger[x_maxPoly];
        MatrixEvalResultWithPitch m_pitch[x_maxPoly];
        CoMuteState m_coMuteState;

        GridSheafView m_gridSheafView;

        CacheForSingleInputVector<true> m_harmonicOutputCaches[1 << x_numInputs];
        CacheForSingleInputVector<false> m_melodicOutputCaches[1 << x_numInputs];
        bool m_lastStepEvaluated;

        LameJuisInternal* m_owner;

        size_t GetPolyChans()
        {
            return m_coMuteState.m_polyChans;
        }
        
        void ClearOutputCaches()
        {            
            for (size_t i = 0; i < (1 << x_numInputs); ++i)
            {
                m_harmonicOutputCaches[i].ClearCache();
                m_melodicOutputCaches[i].ClearCache();
            }

            ClearLastStep();

            m_gridSheafView.SetLens(m_coMuteState.GetLens());

            m_needsInvalidateCache = false;
        }

        void ClearLastStep()
        {
            m_lastStepEvaluated = false;
        }
        
        void Process(Input& input)
        {
            m_coMuteState.Process(input.m_coMuteInput);
            if (m_needsInvalidateCache)
            {
                ClearOutputCaches();
            }
                    
            for (size_t i = 0; i < GetPolyChans(); ++i)
            {
                if (!m_lastStepEvaluated)
                {
                    m_pitch[i] = ComputePitch(m_owner, *input.m_prevVector, i);
                }
                
                SetPitch(ComputePitch(m_owner, *input.m_inputVector, i), i);
            }
            
            m_lastStepEvaluated = true;            
        }

        GridSheafView::CellInfo GetCellInfo(uint8_t x, uint8_t y)
        {
            GridSheafView::CellInfo cellInfo;
            cellInfo.m_baseTimeSlice = m_gridSheafView.m_cellBaseTimeSlices[x][y];
            InputVector currentBaseSlice = m_owner->m_inputVector;
            cellInfo.m_isCurrentSlice = m_gridSheafView.LensEquivalent(currentBaseSlice, cellInfo.m_baseTimeSlice);
            cellInfo.m_localHarmonicPosition = m_owner->EvalMatrixBase(cellInfo.m_baseTimeSlice);
            for (size_t i = 0; i < GetPolyChans(); ++i)
            {
                cellInfo.m_isPlaying[i] = m_pitch[i].m_result == cellInfo.m_localHarmonicPosition;
            }

            return cellInfo;
        }

        MatrixEvalResultWithPitch ComputePitch(LameJuisInternal* matrix, InputVector defaultVector, size_t chan)
        {
            bool harmonic = m_coMuteState.m_harmonic[chan];
            if (harmonic)
            {
                return m_harmonicOutputCaches[defaultVector.m_bits].ComputePitch(matrix, this, defaultVector, chan);
            }
            else
            {
                return m_melodicOutputCaches[defaultVector.m_bits].ComputePitch(matrix, this, defaultVector, chan);
            }
        }            
       
        void SetPitch(MatrixEvalResultWithPitch pitch, size_t chan)
        {
            bool newTrigger = (pitch.m_result != m_pitch[chan].m_result &&
                               pitch.m_pitch != m_pitch[chan].m_pitch);
            m_trigger[chan] = newTrigger;
            m_pitch[chan] = pitch;
        }
                
        TimeSliceClassIterator GetInputVectorIterator(InputVector defaultVector)
        {
            return TimeSliceClassIterator(m_coMuteState.GetLens(), defaultVector);
        }
        
        void Init(LameJuisInternal* owner)
        {
            m_owner = owner;
            m_coMuteState.Init(this);
            m_lastStepEvaluated = false;
        }
    };

    GridSheafView::CellInfo GetCellInfo(size_t outputId, uint8_t x, uint8_t y)
    {
        return m_outputs[outputId].GetCellInfo(x, y);
    }   
    
    struct Input
    {        
        InputBit::Input m_inputBitInput[x_numInputs];
        LogicOperation::Input m_operationInput[x_numOperations];
        Accumulator::Input m_accumulatorInput[x_numAccumulators];
        Output::Input m_outputInput[x_numAccumulators];

        InputVector m_inputVector;
        InputVector m_prevVector;
        bool m_reset;

        void SetInputVectors(LameJuisInternal* owner)
        {            
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                m_inputVector.Set(i, owner->m_inputs[i].m_value);
                m_prevVector.Set(i, owner->m_inputs[i].m_value != owner->m_inputs[i].m_changed);
            }
        }

        void ClearPrevVector()
        {
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                m_prevVector.Set(i, false);
            }
        }

        Input()
        {
            m_reset = false;
            
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                m_inputBitInput[i].m_reset = &m_reset;
            }
            
            for (size_t i = 0; i < x_numOperations; ++i)
            {
                m_operationInput[i].m_inputVector = &m_inputVector;
            }

            for (size_t i = 0; i < x_numAccumulators; ++i)
            {
                m_outputInput[i].m_inputVector = &m_inputVector;
                m_outputInput[i].m_prevVector = &m_prevVector;
            }
        }
    };
    
    void Process(Input& input)
    {       
        for (size_t i = 0; i < x_numInputs; ++i)
        {
            m_inputs[i].Process(input.m_inputBitInput[i]);
        }

        input.SetInputVectors(this);
        m_inputVector = input.m_inputVector;

        for (size_t i = 0; i < x_numOperations; ++i)
        {
            m_operations[i].Process(input.m_operationInput[i]);
        }

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_accumulators[i].Process(input.m_accumulatorInput[i]);
        }
        
        if (m_needsInvalidateCache)
        {
            ClearCaches();
        }

        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_outputs[i].Process(input.m_outputInput[i]);
        }
    }

    InputVector m_inputVector;
    MatrixEvalResult m_evalResults[1 << x_numInputs];
    bool m_isEvaluated[1 << x_numInputs];

    LameJuisInternal()
    {
        Init();
    }

    void Init()
    {    
        for (size_t i = 0; i < x_numInputs; ++i)
        {
            m_inputs[i].Init(i > 0 ? &m_inputs[i - 1] : nullptr);
        }
        
        for (size_t i = 0; i < x_numOperations; ++i)
        {
            m_operations[i].Init(this);
        }
        
        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            m_accumulators[i].Init(this);
            m_outputs[i].Init(this);
        }

        m_needsInvalidateCache = false;
    }

    bool m_needsInvalidateCache;

    InputBit m_inputs[x_numInputs];
    LogicOperation m_operations[x_numOperations];
    Accumulator m_accumulators[x_numAccumulators];
    Output m_outputs[x_numAccumulators];
};
    
constexpr float LameJuisInternal::Accumulator::x_voltages[];
constexpr int LameJuisInternal::Accumulator::x_semitones[];    

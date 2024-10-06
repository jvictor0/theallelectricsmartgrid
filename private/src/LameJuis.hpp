#pragma once
#include "plugin.hpp"
#include <cstddef>
#include "LameJuisConstants.hpp"
#include "IndexArp.hpp"

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
            }
        }

        void Clear()
        {
            memset(m_high, 0, x_numAccumulators);
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
        
        uint8_t m_high[x_numAccumulators];
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

        void OctaveReduce()
        {
            m_pitch = m_pitch - std::floor(m_pitch);
        }
        
        MatrixEvalResult m_result;
        float m_pitch;
    };

    MatrixEvalResultWithPitch EvalMatrix(InputVector inputVector)
    {
        
        MatrixEvalResult& result = m_evalResults[inputVector.m_bits];
        
        if (!m_isEvaluated[inputVector.m_bits])
        {
            result.Clear();
            for (size_t i = 0; i < x_numOperations; ++i)
            {
                size_t outputId = m_operations[i].GetOutputTarget();
                bool isHigh = m_operations[i].GetValue(inputVector);
                if (isHigh)
                {
                    ++result.m_high[outputId];
                }
            }
            
            m_isEvaluated[inputVector.m_bits] = true;
        }
        
        return MatrixEvalResultWithPitch(result, m_accumulators);
    }

    struct InputVectorIterator
    {
        uint8_t m_ordinal = 0;
        InputVector m_coMuteVector;
        size_t m_coMuteSize = 0;
        InputVector m_defaultVector;
        size_t m_forwardingIndices[x_numInputs];

        InputVectorIterator(InputVector coMuteVector, InputVector defaultVector)
            : m_coMuteVector(coMuteVector)
            , m_coMuteSize(m_coMuteVector.CountSetBits())
            , m_defaultVector(defaultVector)
        {
            size_t j = 0;
            for (size_t i = 0; i < m_coMuteSize; ++i)
            {
                while (!m_coMuteVector.Get(j))
                {
                    ++j;
                }
                
                m_forwardingIndices[i] = j;
                ++j;
            }
        }

        InputVector Get()
        {
            InputVector result = m_defaultVector;
            
            // Shift the bits of m_ordinal into the set positions of the co muted vector.
            //
            for (size_t i = 0; i < m_coMuteSize; ++i)
            {
                result.Set(m_forwardingIndices[i], InputVector(m_ordinal).Get(i));
            }
            
            return result;
        }
        
        void Next()
        {
            ++m_ordinal;
        }
        
        bool Done()
        {
            return (1 << m_coMuteSize) <= m_ordinal;
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
                int m_index[x_maxPoly];
                int m_octave[x_maxPoly];
                IndexArp* m_indexArp[x_maxPoly];
                IndexArp* m_preIndexArp[x_maxPoly];
                
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
                        m_index[i] = 0;
                        m_octave[i] = 0;
                        m_indexArp[i] = nullptr;
                        m_preIndexArp[i] = nullptr;
                    }
                }
            };
            
            InputVector GetCoMuteVector()
            {                                
                InputVector result;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    result.Set(i, m_coMutes[i]);
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
                    m_index[i] = 0;
                    m_octave[i] = 0;
                    m_indexArp[i] = nullptr;
                    m_preIndexArp[i] = nullptr;
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
                    m_octave[i] = input.m_octave[i];
                    m_indexArp[i] = input.m_indexArp[i];
                    m_preIndexArp[i] = input.m_preIndexArp[i];
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
            int m_index[x_maxPoly];
            int m_octave[x_maxPoly];
            IndexArp* m_indexArp[x_maxPoly];
            IndexArp* m_preIndexArp[x_maxPoly];
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

                int octave;
                ssize_t ix = GetIx(output, chan, &octave);
                MatrixEvalResultWithPitch result = m_cachedResults[ix];
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
                    InputVectorIterator itr = output->GetInputVectorIterator(defaultVector);
                    for (; !itr.Done(); itr.Next())
                    {
                        m_cachedResults[itr.m_ordinal] = matrix->EvalMatrix(itr.Get());
                        if (!Harmonic)
                        {
                            m_cachedResults[itr.m_ordinal].OctaveReduce();
                        }
                    }
                    
                    m_numResults = itr.m_ordinal;
                    
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

            ssize_t GetIx(LameJuisInternal::Output* output, size_t chan, int* octave)
            {
                *octave = output->m_coMuteState.m_octave[chan];
                if (output->m_coMuteState.m_usePercentile[chan])
                {
                    return PercentileToIx(output->m_coMuteState.m_percentiles[chan]);
                }
                else if (output->m_coMuteState.m_indexArp[chan])
                {
                    int result;
                    output->m_coMuteState.m_preIndexArp[chan]->Get(m_numDistinctResults, &result, octave);
                    output->m_coMuteState.m_indexArp[chan]->Get(m_numDistinctResults, &result, octave);
                    return m_reverseIndex[result];
                }
                else
                {
                    return m_reverseIndex[output->m_coMuteState.m_index[chan]];
                }
            }
            
            ssize_t PercentileToIx(float percentile)
            {
                ssize_t ix = static_cast<size_t>(percentile * m_numResults);
                ix = std::min<ssize_t>(ix, m_numResults - 1);
                ix = std::max<ssize_t>(ix, 0);
                return ix;
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
                
        InputVectorIterator GetInputVectorIterator(InputVector defaultVector)
        {
            return InputVectorIterator(m_coMuteState.GetCoMuteVector(), defaultVector);
        }
        
        void Init(LameJuisInternal* owner)
        {
            m_owner = owner;
            m_coMuteState.Init(this);
            m_lastStepEvaluated = false;
        }
    };

    struct SeqPaletteState
    {
        SeqPaletteState()
            : m_ord(0)
            , m_ordMax(1)
            , m_isCur(false)
        {
        }
        
        size_t m_ord;
        size_t m_ordMax;
        bool m_isCur;
    };
    
    SeqPaletteState GetSeqPaletteState(
        size_t outputId,
        size_t ix,
        InputVector input)
    {
        constexpr size_t x_tot = 1 << x_numInputs;
        Output& o = m_outputs[outputId];
        Output::CacheForSingleInputVector<true>& c = o.m_harmonicOutputCaches[input.m_bits];
        SeqPaletteState result;
        if (!c.m_isEvaluated)
        {
            return result;
        }

        result.m_ordMax = c.m_resultOrd[c.m_numResults - 1] + 1;
        size_t repeats = x_tot / c.m_numResults;
        size_t rIx = ix / repeats;
        result.m_ord = c.m_resultOrd[rIx];
        
        for (size_t i = 0; i < o.GetPolyChans(); ++i)
        {
            size_t pIx = c.GetIx(&o, i, nullptr);
            if (pIx == rIx)
            {
                result.m_isCur = true;
                break;
            }
        }
        
        return result;
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

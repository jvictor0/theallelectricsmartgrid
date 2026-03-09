#pragma once
#include "plugin.hpp"
#include <cstddef>
#include "HarmonicSheaf.hpp"
#include "LameJuisConstants.hpp"

struct LameJuisInternal
{
    static constexpr size_t x_numInputs = 6;
    static constexpr size_t x_numOperations = 6;
    static constexpr size_t x_numAccumulators = 3;
    static constexpr size_t x_numLanes = 3;
    static constexpr size_t x_channelsPerLane = 3;

    struct InputBit
    {
        bool m_value;
        bool m_changed;

        struct Input
        {
            Input()
                : m_value(false)
            {
            }
            
            bool m_value;
        };

        void Init(InputBit* prev)
        {
            m_value = false;
            m_changed = false;
        }
        
        void Process(Input& input)
        {                        
            bool oldValue = m_value;
            m_changed = false;

            m_value = input.m_value;

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

    struct LogicOperation
    {
        enum class SwitchVal : char
        {
            Down = 0,
            Middle = 1,
            Up = 2
        };

        struct Input
        {
            bool m_rhs[x_numInputs + 1];
            SwitchVal m_switch;
            MatrixSwitch m_elements[x_numInputs];
            HarmonicSheaf::BitVector* m_inputVector;

            Input()
                : m_rhs{}
                , m_switch(SwitchVal::Up)
                , m_elements{}
                , m_inputVector(nullptr)
            {
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    m_elements[i] = MatrixSwitch::Muted;
                }

                for (size_t i = 0; i < x_numInputs + 1; ++i)
                {
                    m_rhs[i] = i % 2 == 1;
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
                m_rhs[i] = i % 2 == 1;
            }
        }

        void GetTotalAndHigh(HarmonicSheaf::BitVector inputVector, size_t* countTotal, size_t* countHigh)
        {
            // And with m_active to mute the muted inputs.
            // Xor with m_inverted to invert the inverted ones.
            //
            inputVector.m_bits &= m_active.m_bits;
            inputVector.m_bits ^= m_inverted.m_bits;
            
            *countTotal = m_active.CountSetBits();
            *countHigh = inputVector.CountSetBits();                        
        }

        bool ComputeOperation(size_t countHigh)
        {
            return m_rhs[countHigh];            
        }

        bool GetValue(HarmonicSheaf::BitVector inputVector)
        {
            size_t countTotal;
            size_t countHigh;
            GetTotalAndHigh(inputVector, &countTotal, &countHigh);            
            return ComputeOperation(countHigh);
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
                for (size_t i = 0; i < x_numInputs + 1; ++i)
                {
                    if (m_rhs[i] != input.m_rhs[i])
                    {
                        m_rhs[i] = input.m_rhs[i];
                        m_owner->m_needsInvalidateCache = true;
                    }
                }
                
                if (m_switch != input.m_switch)
                {
                    m_switch = input.m_switch;
                    m_owner->m_needsInvalidateCache = true;
                }

                GetTotalAndHigh(*input.m_inputVector, &m_countTotal, &m_countHigh);            
                m_gate = ComputeOperation(m_countHigh);                
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    bool up = input.m_inputVector->Get(i);
                    bool proper = m_inverted.Get(i) ? !up : up;
                    m_highParticipant[i] = m_gate && proper && m_active.Get(i);
                }
            }
        }

        HarmonicSheaf::BitVector m_active;
        HarmonicSheaf::BitVector m_inverted;

        bool m_rhs[x_numInputs + 1]{};
        SwitchVal m_switch{SwitchVal::Up};
        size_t m_countTotal{0};
        size_t m_countHigh{0};
        bool m_highParticipant[x_numInputs]{};
        bool m_gate{false};

        LameJuisInternal* m_owner{nullptr};

        MatrixSwitch m_elements[x_numInputs]{};
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
            Interval m_interval;
            
            Input()
                : m_interval(Interval::Off)
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
        
        Interval m_interval;
        float m_intervalValue;
        LameJuisInternal* m_owner;
        
        void Process(Input& input)
        {
            bool change = false;
            if (m_interval != input.m_interval)
            {
                change = true;
                m_interval = input.m_interval;
            }

            if (change)
            {
                m_intervalValue = x_voltages[static_cast<size_t>(m_interval)];
                m_owner->m_needsInvalidateCache = true;
            }
        }

        void Init(LameJuisInternal* owner)
        {
            m_owner = owner;
            m_interval = Interval::Off;
            m_intervalValue = 0;
        }
    };

    HarmonicSheaf::Evaluator GetEvaluator()
    {
        HarmonicSheaf::Evaluator evaluator;
        for (size_t i = 0; i < x_numAccumulators; ++i)
        {
            evaluator.m_coefficients[i] = m_accumulators[i].m_intervalValue;
        }

        return evaluator;
    }

    void RebuildSheafIfNeeded()
    {
        if (m_needsInvalidateCache)
        {
            m_needsInvalidateCache = false;
            RebuildSheaf();            
        }
    }

    void RebuildSheaf()
    {       
        for (uint8_t i = 0; i < HarmonicSheaf::x_numBasePoints; ++i)
        {
            HarmonicSheaf::BitVector index(i);
            HarmonicSheaf::Section& section = m_sheaf.Get(index);
            section.Clear();
            for (size_t j = 0; j < x_numOperations; ++j)
            {
                bool isHigh = m_operations[j].GetValue(index);
                ++section.m_total[m_operations[j].GetOutputTarget()];
                if (isHigh)
                {
                    ++section.m_high[m_operations[j].GetOutputTarget()];
                }
            }
        }
    }

    struct GridSheafView
    {
        static constexpr size_t x_gridSizeBits = 3;
        static constexpr size_t x_gridSize = 1 << x_gridSizeBits;
        
        struct CellInfo
        {
            HarmonicSheaf::BitVector m_baseTimeSlice;
            bool m_isCurrentSlice;
            HarmonicSheaf::Section m_localHarmonicPosition;
            bool m_isPlaying[x_channelsPerLane];
            bool m_isMuted;

            CellInfo()
                : m_baseTimeSlice(0)
                , m_isCurrentSlice(false)
                , m_localHarmonicPosition()
                , m_isPlaying{}
                , m_isMuted(false)
            {
            }
        };   

        struct TimeSliceXYConverter
        {
            HarmonicSheaf::Lens m_lens;
            size_t m_lensCoDimension;
            size_t m_forwardingIndex[x_numInputs];

            TimeSliceXYConverter()
                : m_lens()
                , m_lensCoDimension(0)
                , m_forwardingIndex{}
            {
            }

            void SetLens(HarmonicSheaf::Lens lens)
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

            HarmonicSheaf::BitVector Convert(uint8_t x, uint8_t y) 
            {
                HarmonicSheaf::BitVector xy(x | (y << x_gridSizeBits));
                HarmonicSheaf::BitVector result;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    result.Set(i, xy.Get(m_forwardingIndex[i]));
                }

                return result;
            }
        };

        TimeSliceXYConverter m_timeSliceXYConverter;
        HarmonicSheaf::BitVector m_cellBaseTimeSlices[x_gridSize][x_gridSize];

        bool LensEquivalent(HarmonicSheaf::BitVector a, HarmonicSheaf::BitVector b)
        {
            return m_timeSliceXYConverter.m_lens.Equivalent(a, b);
        }

        void SetLens(HarmonicSheaf::Lens lens)
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

    struct Lane
    {
        struct CoMuteState
        {
            struct Input
            {
                bool m_coMutes[x_numInputs];
                
                Input()
                    : m_coMutes{}
                {
                    for (size_t i = 0; i < x_numInputs; ++i)
                    {
                        m_coMutes[i] = false;
                    }
                }
            };
            
            HarmonicSheaf::Lens GetLens()
            {                                
                HarmonicSheaf::Lens result;
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    result.Set(i, !m_coMutes[i]);
                }

                return result;
            }
            
            void Init(Lane* owner)
            {                                
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    m_coMutes[i] = false;
                }

                m_owner = owner;
            }
            
            void Process(Input& input)
            {                               
                for (size_t i = 0; i < x_numInputs; ++i)
                {
                    if (m_coMutes[i] != input.m_coMutes[i] &&
                        m_owner->m_owner->m_inputs[i].m_changed)
                    {
                        m_coMutes[i] = input.m_coMutes[i];
                        m_owner->m_gridSheafView.SetLens(GetLens());
                    }
                }
            }
            
            bool m_coMutes[x_numInputs];
            Lane* m_owner;
        };

        struct Chooser
        {
            struct Input
            {
                HarmonicSheaf::SectionChoiceStrategy m_strategy;
                HarmonicSheaf::SectionChoiceStrategy m_baseStrategy;
                float m_choiceArg[x_channelsPerLane];

                Input()
                    : m_strategy(HarmonicSheaf::SectionChoiceStrategy::ClosestModOne)
                    , m_baseStrategy(HarmonicSheaf::SectionChoiceStrategy::None)
                    , m_choiceArg{}
                {
                }
            };

            Lane* m_owner;

            Chooser(Lane* owner)
                : m_owner(owner)
            {
            }            

            HarmonicSheaf::SectionWithValue Choose(Input& input, size_t voiceIx, HarmonicSheaf::BitVector defaultVector)
            {
                HarmonicSheaf::SectionChooser chooser;
                chooser.m_strategy = input.m_baseStrategy;
                chooser.m_evaluator = m_owner->GetEvaluator();
                chooser.m_choiceArg = input.m_choiceArg[voiceIx];
                HarmonicSheaf::SectionWithValue baseSection = chooser.Choose(m_owner->m_owner->m_sheaf, m_owner->m_coMuteState.GetLens(), defaultVector);
                
                chooser.m_strategy = input.m_strategy;
                chooser.m_choiceArg += baseSection.m_value;
                return chooser.Choose(m_owner->m_owner->m_sheaf, m_owner->m_coMuteState.GetLens(), defaultVector);
            }
        };

        struct Input
        {
            Input()
                : m_coMuteInput()
                , m_inputVector(nullptr)
                , m_updateChan{}
            {
            }

            CoMuteState::Input m_coMuteInput;
            Chooser::Input m_chooserInput;
            HarmonicSheaf::BitVector* m_inputVector;
            bool m_updateChan[x_channelsPerLane];
        };

        bool m_trigger[x_channelsPerLane]{};
        HarmonicSheaf::SectionWithValue m_pitch[x_channelsPerLane];
        CoMuteState m_coMuteState;        

        GridSheafView m_gridSheafView;

        LameJuisInternal* m_owner{nullptr};

        HarmonicSheaf::Evaluator GetEvaluator()
        {
            return m_owner->GetEvaluator();
        }

        void Process(Input& input)
        {
            m_coMuteState.Process(input.m_coMuteInput);
            for (size_t i = 0; i < x_channelsPerLane; ++i)
            {
                if (input.m_updateChan[i])
                {
                    Chooser chooser(this);
                    HarmonicSheaf::SectionWithValue section = chooser.Choose(input.m_chooserInput, i, *input.m_inputVector);
                    m_trigger[i] = section != m_pitch[i];
                    m_pitch[i] = section;
                }
                else
                {
                    m_trigger[i] = false;
                }
            }
        }

        GridSheafView::CellInfo GetCellInfo(uint8_t x, uint8_t y)
        {
            GridSheafView::CellInfo cellInfo;
            cellInfo.m_baseTimeSlice = m_gridSheafView.m_cellBaseTimeSlices[x][y];
            HarmonicSheaf::BitVector currentBaseSlice = m_owner->m_inputVector;
            cellInfo.m_isCurrentSlice = m_gridSheafView.LensEquivalent(currentBaseSlice, cellInfo.m_baseTimeSlice);
            cellInfo.m_localHarmonicPosition = m_owner->m_sheaf.Get(cellInfo.m_baseTimeSlice);
            cellInfo.m_isMuted = false;
            for (size_t i = 0; i < x_channelsPerLane; ++i)
            {
                cellInfo.m_isPlaying[i] = m_pitch[i].m_section == cellInfo.m_localHarmonicPosition;
            }

            return cellInfo;
        }
                
        HarmonicSheaf::TimeSliceClassIterator GetBitVectorIterator(HarmonicSheaf::BitVector defaultVector)
        {
            return HarmonicSheaf::TimeSliceClassIterator(m_coMuteState.GetLens(), defaultVector);
        }

        void Init(LameJuisInternal* owner)
        {
            m_owner = owner;
            m_coMuteState.Init(this);
        }

        void Reset()
        {
            for (size_t i = 0; i < x_channelsPerLane; ++i)
            {
                m_pitch[i] = HarmonicSheaf::SectionWithValue();
                for (size_t j = 0; j < x_numAccumulators; ++j)
                {
                    m_pitch[i].m_section.m_high[j] = 0xFF;
                }
            }
        }
    };

    GridSheafView::CellInfo GetCellInfo(size_t outputId, uint8_t x, uint8_t y)
    {
        return m_lanes[outputId].GetCellInfo(x, y);
    }   
    
    struct Input
    {        
        InputBit::Input m_inputBitInput[x_numInputs];
        LogicOperation::Input m_operationInput[x_numOperations];
        Accumulator::Input m_accumulatorInput[x_numAccumulators];
        Lane::Input m_laneInput[x_numLanes];

        HarmonicSheaf::BitVector m_inputVector;

        void SetInputVectors(LameJuisInternal* owner)
        {            
            for (size_t i = 0; i < x_numInputs; ++i)
            {
                m_inputVector.Set(i, owner->m_inputs[i].m_value);
            }
        }

        Input()
        {            
            for (size_t i = 0; i < x_numOperations; ++i)
            {
                m_operationInput[i].m_inputVector = &m_inputVector;
                m_operationInput[i].m_elements[i] = MatrixSwitch::Normal;
                for (size_t j = 0; j < x_numInputs + 1; ++j)
                {
                    m_operationInput[i].m_rhs[j] = j % 2 == 1;
                }
            }

            for (size_t i = 0; i < x_numLanes; ++i)
            {
                m_laneInput[i].m_inputVector = &m_inputVector;
            }

            m_accumulatorInput[0].m_interval = Accumulator::Interval::PerfectFifth;
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
        
        RebuildSheafIfNeeded();

        for (size_t i = 0; i < x_numLanes; ++i)
        {
            m_lanes[i].Process(input.m_laneInput[i]);
        }
    }

    HarmonicSheaf::BitVector m_inputVector;
    HarmonicSheaf::Sheaf m_sheaf;

    LameJuisInternal()
        : m_inputVector(0)
        , m_sheaf()
        , m_needsInvalidateCache(false)
        , m_inputs{}
        , m_operations{}
        , m_accumulators{}
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
        
        for (size_t i = 0; i < x_numLanes; ++i)
        {
            m_accumulators[i].Init(this);
            m_lanes[i].Init(this);
        }

        m_needsInvalidateCache = false;
        Reset();
    }

    void Reset()
    {
        for (size_t i = 0; i < x_numLanes; ++i)
        {
            m_lanes[i].Reset();
        }
    }

    bool m_needsInvalidateCache;

    InputBit m_inputs[x_numInputs];
    LogicOperation m_operations[x_numOperations];
    Accumulator m_accumulators[x_numAccumulators];
    Lane m_lanes[x_numLanes];
};
    
constexpr float LameJuisInternal::Accumulator::x_voltages[];    

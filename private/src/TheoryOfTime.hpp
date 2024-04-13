#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "Trig.hpp"
#include "NormGen.hpp"


inline float Circlize(float x)
{
    return x - floor(x);
}

inline float Interpolate(float x1, float x2, float y1, float y2, float xp)
{
    return y1 + (y2 - y1) * (xp - x1) / (x2 - x1);
}

inline float CircleDist(float x1, float x2)
{
    return std::min(std::abs(x2 - x1), 1 - std::abs(x2 - x1));
}

struct MusicalTime;

struct LinearPeice
{
    enum class BoundState : int
    {
        Under,
        In,
        Over
    };

    float m_x1;
    float m_x2;
    float m_y1;
    float m_y2;
    bool m_empty;

    LinearPeice()
        : LinearPeice(0, 0, 0, 0)
    {
        m_empty = true;
    }
    
    LinearPeice(float x1, float x2, float y1, float y2)
    {
        m_x1 = x1;
        m_x2 = x2;
        m_y1 = y1;
        m_y2 = y2;
        m_empty = false;
    }

    LinearPeice SetOver(float x2, float y2)
    {
        return LinearPeice(m_x2, x2, m_y2, y2);
    }

    LinearPeice SetUnder(float x1, float y1)
    {
        return LinearPeice(x1, m_x1, y1, m_y1);
    }

    static LinearPeice Empty()
    {
        return LinearPeice();
    }

    LinearPeice Compose(LinearPeice other)
    {
        float x1 = ::Interpolate(0, 1, other.m_x1, other.m_x2, m_x1);
        float x2 = ::Interpolate(0, 1, other.m_x1, other.m_x2, m_x2);
        return LinearPeice(
            x1,
            x2, 
            Interpolate(other.Interpolate(x1)),
            Interpolate(other.Interpolate(x2)));
    }

    BoundState Check(float x)
    {
        if (m_empty)
        {
            return BoundState::Under;
        }
        else if (m_x1 <= x && x <= m_x2)
        {
            return BoundState::In;
        }
        else if (CircleDist(m_x1, x) < CircleDist(m_x2, x))
        {
            return BoundState::Under;
        }
        else
        {
            return BoundState::Over;
        }
    }

    float Interpolate(float x)
    {
        return ::Interpolate(m_x1, m_x2, m_y1, m_y2, x);
    }

    std::string ToString()
    {
        return "<" + std::to_string(m_x1) + ", " +
            std::to_string(m_x2) + ", " +
            std::to_string(m_y1) + ", " +
            std::to_string(m_y2) + ">";
    }
};

struct TimeBit
{
    enum class State : int
    {
        x_init,
        x_waiting,
        x_running
    };
    
    LinearPeice m_lp;
    MusicalTime* m_owner;
    size_t m_ix;
    size_t m_parentIx;
    float m_swing;
    float m_swagger;
    float m_pos;
    size_t m_parentFloor;
    size_t m_mult;
    bool m_top;
    State m_state;
    bool m_pingPong;

    void Init(size_t ix, MusicalTime* owner)
    {
        m_ix = ix;        
        m_owner = owner;
        m_parentIx = ix - 1;
        m_pos = 0;
        m_parentFloor = 0;
        m_top = true;
        m_state = State::x_init;
        m_swing = 0;
        m_swagger = 0;
        m_mult = 1;
        m_pingPong = false;
    }

    void Stop()
    {
        m_pos = 0;
        m_parentFloor = 0;
        m_top = true;
        m_state = State::x_init;
        m_lp = LinearPeice::Empty();
    }
    
    TimeBit* GetParent();

    LinearPeice MakeLP(float parentPos, size_t* parentFloor)
    {
        *parentFloor = floor(parentPos * m_mult);
        float pfFloat = static_cast<float>(*parentFloor);
        LinearPeice result = LinearPeice(pfFloat / m_mult, (pfFloat + 1) / m_mult, 0, 1);
        if (m_pingPong)
        {
            result = MakePingPongLP(result.Interpolate(parentPos)).Compose(result);
        }
        
        result = MakeSwingLP(result.Interpolate(parentPos)).Compose(result);
        return result;
    }
    
    float ApplyMult(float t)
    {
        float preres = m_mult * t;
        return preres - floor(preres);
    }

    LinearPeice MakePingPongLP(float t)
    {
        if (!m_pingPong)
        {
            return LinearPeice(0, 1, 0, 1);
        }
        else if (t < 0.5)
        {
            return LinearPeice(0, 0.5, 0, 1);
        }
        else
        {
            return LinearPeice(0.5, 1, 1, 0);
        }
    }
    
    LinearPeice MakeSwingLP(float t)
    {
        if (t < m_swing / 2 + 0.5)
        {
            return LinearPeice(0, m_swing / 2 + 0.5, 0, m_swagger / 2 + 0.5);
        }
        else
        {
            return LinearPeice(m_swing / 2 + 0.5, 1, m_swagger / 2 + 0.5, 1);
        }
    }

    struct Input
    {
        size_t m_parentIx;
        float m_swing;
        float m_swagger;
        size_t m_mult;
        bool m_pingPong;
        float* m_rand;
        RGen m_gen;
        float* m_globalHomotopy;

        Input()
            : m_parentIx(0)
            , m_swing(0.0)
            , m_swagger(0.0)
            , m_mult(2)
            , m_pingPong(false)
            , m_rand(nullptr)
            , m_globalHomotopy(nullptr)
        {
        }
    };

    void Read(Input& input)
    {
        if (m_state == State::x_init)
        {
            m_state = State::x_running;
        }
        else if (m_parentIx != input.m_parentIx)
        {
            m_top = true;
            m_pos = 0;
            m_state = State::x_waiting;
        }

        m_parentIx = input.m_parentIx;
        if (GetParent()->m_top)
        {
            m_state = State::x_running;            
        }
        
        m_mult = input.m_mult;
        m_pingPong = input.m_pingPong;
        ReadSwing(input);
    }

    void ReadSwing(Input& input)
    {
        float rand = input.m_rand ? *input.m_rand : 0;
        float globalHomo = input.m_globalHomotopy ? *input.m_globalHomotopy : 1;
        m_swing = globalHomo * ((1 - rand) * input.m_swing + rand * input.m_swing * input.m_gen.UniGen());
        m_swagger = globalHomo * ((1 - rand) * input.m_swagger + rand * input.m_swagger * input.m_gen.UniGen());
    }

    void Process(Input& input)
    {
        if (m_state == State::x_init)
        {
            Read(input);
        }
        
        if (m_state == State::x_waiting)
        {
            Read(input);
            if (m_state == State::x_waiting)
            {
                return;
            }
        }
        else
        {
            TimeBit* parent = GetParent();
            if (parent->m_top)
            {
                Read(input);
                if (m_state == State::x_waiting)
                {
                    return;
                }
            }

            m_top = parent->m_top;
        }

        TimeBit* parent = GetParent();
        float inT = parent->m_pos;
        if (m_lp.Check(inT) != LinearPeice::BoundState::In)
        {
            size_t newParentFloor;
            m_lp = MakeLP(inT, &newParentFloor);
            if (newParentFloor != m_parentFloor)
            {
                m_top = true;
                m_parentFloor = newParentFloor;
            }
        }

        m_pos = m_lp.Interpolate(inT);
    }

    void SetDirectly(float t)
    {
        m_top = false;
        if (std::abs(t - m_pos) > 0.5)
        {
            m_top = true;
        }

        m_pos = t;
    }
};

struct MusicalTime
{
    static constexpr size_t x_numBits = 8;
    TimeBit m_bits[x_numBits];
    bool m_gate[x_numBits];
    bool m_change[x_numBits];
    bool m_anyChange;
    bool m_running;

    MusicalTime()
    {
        m_anyChange = false;
        m_running = false;
        for (size_t i = 0; i < x_numBits; ++i)
        {
            m_bits[i].Init(i, this);
            m_change[i] = false;
        }
    }
    
    struct Input
    {
        float m_t;
        TimeBit::Input m_input[x_numBits];
        float m_rand;
        float m_globalHomotopy;
        bool m_running;

        Input()
        {
            m_t = 0;
            m_rand = 0;
            m_globalHomotopy = 1;
            for (size_t i = 0; i < x_numBits; ++i)
            {
                if (i > 0)
                {
                    m_input[i].m_parentIx = i - 1;
                }

                m_input[i].m_rand = &m_rand;
                m_input[i].m_globalHomotopy = &m_globalHomotopy;
            }
        }
    };

    void Process(Input& in)
    {
        if (in.m_running)
        {
            m_running = true;
            m_bits[0].SetDirectly(in.m_t);
            for (size_t i = 1; i < x_numBits; ++i)
            {
                m_bits[i].Process(in.m_input[i]);
            }
            
            m_anyChange = false;
            
            for (size_t i = 0; i < x_numBits; ++i)
            {
                m_change[i] = false;
                bool gate = GetPos(i) < 0.5;
                if (m_gate[i] != gate)
                {
                    m_change[i] = true;
                    m_anyChange = true;
                }
                
                m_gate[i] = gate;
            }
        }
        else if (m_running)
        {
            // It was running, but a stop was requested.
            // Stop All bits, and set 'm_anyChange' so others can react.
            //
            m_running = false;
            m_anyChange = true;
            for (size_t i = 0; i < x_numBits; ++i)
            {
                m_bits[i].Stop();
                m_gate[i] = false;
                m_change[i] = true;
            }            
        }
        else
        {
            m_anyChange = false;
        }
    }

    float GetPos(size_t ix)
    {
        return m_bits[ix].m_pos;        
    }

    bool GetGate(size_t ix)
    {
        return m_gate[ix];
    }
};

inline TimeBit* TimeBit::GetParent()
{
    return &m_owner->m_bits[m_parentIx];
}

struct MusicalTimeWithClock
{
    MusicalTime m_musicalTime;
    float m_phasor;

    struct Input
    {
        Input() : m_freq(1.0/4)
        {
        }
        
        MusicalTime::Input m_input;
        float m_freq;        
    };

    void Process(float dt, Input& input)
    {
        if (input.m_input.m_running)
        {
            float dx = dt * input.m_freq;
            m_phasor += dx;
            m_phasor = m_phasor - floor(m_phasor);
        }
        else
        {
            m_phasor = 0;
        }

        input.m_input.m_t = m_phasor;
        m_musicalTime.Process(input.m_input);
    }
};

// struct TheoryOfTime : Module
// {
//     MusicalTime::Input m_state;
//     MusicalTime m_musicalTime;

//     static constexpr float x_timeToCheck = 0.05;    
//     float m_timeToCheck;

//     TheoryOfTime()
//     {
//         config(0, x_numInputs, x_numOutputs, 0);
        
//         m_timeToCheck = -1;

//         configInput(x_inputId, "Phasor Input");
//         configInput(x_multInId, "Mult Input");
//         configInput(x_swingInId, "Swing input");
//         configInput(x_pingPongInId, "Ping Pong Input");
//         configInput(x_rebaseInId, "Rebase Input");
//         configInput(x_zeroInId, "Zero Input");
//         configInput(x_randomInId, "Random Input");

//         configOutput(x_phasorOut, "Phasor Output");
//         configOutput(x_gateOut, "Gate Output");
//         configOutput(x_trigOut, "Trig Output");
//     }    

//     void CheckInputs()
//     {
//         for (size_t i = 1; i < MusicalTime::x_numBits; ++i)
//         {
//             if (i > 1)
//             {
//                 m_state.m_input[i].m_parentIx = inputs[x_rebaseInId].getVoltage(i - 1) > 0 ? i - 2 : i - 1;
//             }
//             else
//             {
//                 m_state.m_input[i].m_parentIx = i - 1;
//             }

//             m_state.m_input[i].m_mult = std::floor(inputs[x_multInId].getVoltage(i - 1) + 0.5);
//             m_state.m_input[i].m_swing = inputs[x_swingInId].getVoltage(i - 1) / 20 + 0.5;
//             m_state.m_input[i].m_swagger = 0.5;
//             m_state.m_input[i].m_pingPong = inputs[x_pingPongInId].getVoltage(i - 1) > 0;
//             //            m_state.m_input[i].m_rand = inputs[x_randomInId].getVoltage() / 10;
//             if (inputs[x_zeroInId].getVoltage(i - 1) > 0)
//             {
//                 m_state.m_input[i].m_mult = 0;
//             }
//         }

//         outputs[x_phasorOut].setChannels(MusicalTime::x_numBits);
//         outputs[x_gateOut].setChannels(MusicalTime::x_numBits);
//     }
    
//     void process(const ProcessArgs &args) override
//     {
//         m_state.m_t = inputs[x_inputId].getVoltage() / 10;
//         m_timeToCheck -= args.sampleTime;
//         if (m_timeToCheck < 0)
//         {
//             CheckInputs();
//             m_timeToCheck = x_timeToCheck;
//         }

//         m_musicalTime.Process(m_state);

//         for (size_t i = 0; i < MusicalTime::x_numBits; ++i)
//         {
//             outputs[x_phasorOut].setVoltage(m_musicalTime.GetPos(i) * 10, i);
//             outputs[x_gateOut].setVoltage(m_musicalTime.GetGate(i) ? 10 : 0, i);
//         }

//         outputs[x_trigOut].setVoltage(m_musicalTime.m_anyChange ? 10 : 0);
//     }

//     static constexpr size_t x_inputId = 0;
//     static constexpr size_t x_multInId = 1;
//     static constexpr size_t x_swingInId = 2;
//     static constexpr size_t x_pingPongInId = 3;
//     static constexpr size_t x_rebaseInId = 4;
//     static constexpr size_t x_zeroInId = 5;
//     static constexpr size_t x_randomInId = 6;
//     static constexpr size_t x_numInputs = 7;

//     static constexpr size_t x_phasorOut = 0;
//     static constexpr size_t x_gateOut = 1;
//     static constexpr size_t x_trigOut = 2;
//     static constexpr size_t x_numOutputs = 3;
// };

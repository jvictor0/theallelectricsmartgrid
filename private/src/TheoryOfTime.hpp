#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "BlinkLight.hpp"
#include "Trig.hpp"
#include "NormGen.hpp"

inline float Interpolate(float x1, float y1, float x2, float y2, float xp)
{
    return y1 + (y2 - y1) * (xp - x1) / (x2 - x1);
}

struct MusicalTime;

struct TimeBit
{
    enum class State : int
    {
        x_init,
        x_waiting,
        x_running
    };
    
    MusicalTime* m_owner;
    size_t m_ix;
    size_t m_parentIx;
    float m_swing;
    float m_swagger;
    float m_pos;
    float m_prePos;
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
        m_prePos = 0;
        m_top = true;
        m_state = State::x_init;
        m_swing = 0.5;
        m_swagger = 0.5;
        m_mult = 1;
        m_pingPong = false;
    }

    TimeBit* GetParent();

    float ApplyMult(float t)
    {
        float preres = m_mult * t;
        return preres - floor(preres);
    }

    float ApplyPingPong(float t)
    {
        if (!m_pingPong)
        {
            return t;
        }
        else if (t < 0.5)
        {
            return 2 * t;
        }
        else
        {
            return 1 - 2 * (t - 0.5);
        }
    }
    
    float ApplySwing(float t)
    {
        if (t < m_swing)
        {
            return Interpolate(0, 0, m_swing, m_swagger, t);
        }
        else
        {
            return Interpolate(m_swing, m_swagger, 1, 1, t);
        }
    }

    struct Input
    {
        size_t m_parentIx;
        float m_swing;
        float m_swagger;
        size_t m_mult;
        bool m_pingPong;
        float m_rand;
        RGen m_gen;

        Input()
            : m_parentIx(0)
            , m_swing(0.5)
            , m_swagger(0.5)
            , m_mult(2)
            , m_pingPong(false)
            , m_rand(0)
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
        m_swing = (1 - input.m_rand) * m_swing + input.m_rand * (0.5 + (m_swing - 0.5) * input.m_gen.UniGen());
        m_swagger = (1 - input.m_rand) * m_swagger + input.m_rand * (0.5 + (m_swagger - 0.5) * input.m_gen.UniGen());        
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
        }

        TimeBit* parent = GetParent();
        float inT = parent->m_pos;
        float newPrePos = ApplyMult(inT);
        m_top = false;
        if (std::abs(newPrePos - m_prePos) > 0.5 || newPrePos == 0)
        {
            m_top = true;
            ReadSwing(input);
        }

        m_prePos = newPrePos;
        m_pos = ApplySwing(ApplyPingPong(newPrePos));
    }

    void SetDirectly(float t)
    {
        m_top = false;
        if (std::abs(t - m_prePos) > 0.5)
        {
            m_top = true;
        }

        m_prePos = t;
        m_pos = t;
    }
};

struct MusicalTime
{
    static constexpr size_t x_numBits = 8;
    TimeBit m_bits[x_numBits];
    bool m_gate[x_numBits];
    bool m_change;

    MusicalTime()
    {
        m_change = false;
        for (size_t i = 0; i < x_numBits; ++i)
        {
            m_bits[i].Init(i, this);
        }
    }
    
    struct Input
    {
        float m_t;
        TimeBit::Input m_input[x_numBits];

        Input()
        {
            m_t = 0;
            for (size_t i = 1; i < x_numBits; ++i)
            {
                m_input[i].m_parentIx = i - 1;
            }
        }
    };

    void Process(Input& in)
    {
        m_bits[0].SetDirectly(in.m_t);
        for (size_t i = 1; i < x_numBits; ++i)
        {
            m_bits[i].Process(in.m_input[i]);
        }

        m_change = false;

        for (size_t i = 0; i < x_numBits; ++i)
        {
            bool gate = GetPos(i) < 0.5;
            if (m_gate[i] != gate)
            {
                m_change = true;
            }

            m_gate[i] = gate;
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
        float dx = dt * input.m_freq;
        m_phasor += dx;
        m_phasor = m_phasor - floor(m_phasor);
        input.m_input.m_t = m_phasor;

        m_musicalTime.Process(input.m_input);
    }
};

struct TheoryOfTime : Module
{
    MusicalTime::Input m_state;
    MusicalTime m_musicalTime;

    static constexpr float x_timeToCheck = 0.05;    
    float m_timeToCheck;

    TheoryOfTime()
    {
        config(0, x_numInputs, x_numOutputs, 0);
        
        m_timeToCheck = -1;

        configInput(x_inputId, "Phasor Input");
        configInput(x_multInId, "Mult Input");
        configInput(x_swingInId, "Swing input");
        configInput(x_pingPongInId, "Ping Pong Input");
        configInput(x_rebaseInId, "Rebase Input");
        configInput(x_zeroInId, "Zero Input");
        configInput(x_randomInId, "Random Input");

        configOutput(x_phasorOut, "Phasor Output");
        configOutput(x_gateOut, "Gate Output");
        configOutput(x_trigOut, "Trig Output");
    }    

    void CheckInputs()
    {
        for (size_t i = 1; i < MusicalTime::x_numBits; ++i)
        {
            if (i > 1)
            {
                m_state.m_input[i].m_parentIx = inputs[x_rebaseInId].getVoltage(i - 1) > 0 ? i - 2 : i - 1;
            }
            else
            {
                m_state.m_input[i].m_parentIx = i - 1;
            }

            m_state.m_input[i].m_mult = std::floor(inputs[x_multInId].getVoltage(i - 1) + 0.5);
            m_state.m_input[i].m_swing = inputs[x_swingInId].getVoltage(i - 1) / 20 + 0.5;
            m_state.m_input[i].m_swagger = 0.5;
            m_state.m_input[i].m_pingPong = inputs[x_pingPongInId].getVoltage(i - 1) > 0;
            m_state.m_input[i].m_rand = inputs[x_randomInId].getVoltage() / 10;
            if (inputs[x_zeroInId].getVoltage(i - 1) > 0)
            {
                m_state.m_input[i].m_mult = 0;
            }
        }

        outputs[x_phasorOut].setChannels(MusicalTime::x_numBits);
        outputs[x_gateOut].setChannels(MusicalTime::x_numBits);
    }
    
    void process(const ProcessArgs &args) override
    {
        m_state.m_t = inputs[x_inputId].getVoltage() / 10;
        m_timeToCheck -= args.sampleTime;
        if (m_timeToCheck < 0)
        {
            CheckInputs();
            m_timeToCheck = x_timeToCheck;
        }

        m_musicalTime.Process(m_state);

        for (size_t i = 0; i < MusicalTime::x_numBits; ++i)
        {
            outputs[x_phasorOut].setVoltage(m_musicalTime.GetPos(i) * 10, i);
            outputs[x_gateOut].setVoltage(m_musicalTime.GetGate(i) ? 10 : 0, i);
        }

        outputs[x_trigOut].setVoltage(m_musicalTime.m_change ? 10 : 0);
    }

    static constexpr size_t x_inputId = 0;
    static constexpr size_t x_multInId = 1;
    static constexpr size_t x_swingInId = 2;
    static constexpr size_t x_pingPongInId = 3;
    static constexpr size_t x_rebaseInId = 4;
    static constexpr size_t x_zeroInId = 5;
    static constexpr size_t x_randomInId = 6;
    static constexpr size_t x_numInputs = 7;

    static constexpr size_t x_phasorOut = 0;
    static constexpr size_t x_gateOut = 1;
    static constexpr size_t x_trigOut = 2;
    static constexpr size_t x_numOutputs = 3;
};

struct TheoryOfTimeWidget : ModuleWidget
{
    TheoryOfTimeWidget(TheoryOfTime* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/TheoryOfTime.svg")));
        
        float rowOff = 25;
        float rowStart = 50;
        
        float rowPos = 75;
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 0 * rowOff, rowPos), module, module->x_inputId));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 1 * rowOff, rowPos), module, module->x_multInId));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 2 * rowOff, rowPos), module, module->x_swingInId));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 3 * rowOff, rowPos), module, module->x_pingPongInId));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 4 * rowOff, rowPos), module, module->x_rebaseInId));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 5 * rowOff, rowPos), module, module->x_zeroInId));
        addInput(createInputCentered<PJ301MPort>(Vec(rowStart + 6 * rowOff, rowPos), module, module->x_randomInId));
        
        rowPos += 50;
        addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 0 * rowOff, rowPos), module, module->x_phasorOut));
        addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 1 * rowOff, rowPos), module, module->x_gateOut));
        addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart + 2 * rowOff, rowPos), module, module->x_trigOut));
    }
};

#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "Trig.hpp"
#include "NormGen.hpp"

struct ErfTable
{
    static constexpr size_t x_tableSize = 1024;
    float x_table[x_tableSize + 1];

    ErfTable()
    {
        x_table[0] = 0;
        x_table[x_tableSize] = 1;
        for (size_t i = 1; i < x_tableSize; ++i)
        {
            float timeFrac = static_cast<float>(i) / x_tableSize;
            x_table[i] = (std::erf(timeFrac * 4.0f - 2.0f) + 1.0) / 2;
        }
    }

    float Compute(float timeFrac)
    {
        timeFrac *= x_tableSize - 1;
        size_t flr = std::max<size_t>(0, std::min<size_t>(x_tableSize - 1, floor(timeFrac)));
        float lowErf = x_table[flr];
        float highErf = x_table[flr + 1];
        float frac = std::max<float>(0.0, timeFrac - static_cast<float>(flr));
        return lowErf + frac * (highErf - lowErf);
    }
};

struct GangedRandomLFOInternal
{
    enum class State : int
    {
        x_waiting,
        x_moving
    };
    
    static constexpr size_t x_maxSize = 16;
    static constexpr float x_waitFrac = 0.25;
    static constexpr float x_timeSigma = 0.25;

    ErfTable m_erf;
    
    float m_src[x_maxSize];
    float m_pos[x_maxSize];
    float m_trg[x_maxSize];
    float m_shape[x_maxSize];
    float m_time;
    float m_deadline;
    State m_state;

    bool m_didInit;

    GangedRandomLFOInternal()
        : m_state(State::x_moving)
        , m_didInit(false)
    {
    }

    struct Input
    {
        float m_time;
        float m_sigma;
        size_t m_gangSize;
        RGen m_gen;
    };

    void ProcessOne(size_t voice, float timeFrac, float sigmoid)
    {
        float spaceFrac = m_shape[voice] * sigmoid + (1 - m_shape[voice]) * timeFrac;
        m_pos[voice] = m_src[voice] + (m_trg[voice] - m_src[voice]) * spaceFrac;
    }

    void Process(float dt, Input& input)
    {
        if (!m_didInit)
        {
            Init(input);
            return;
        }
        
        m_time += dt;
        if (m_time > m_deadline * input.m_time)
        {
            AdvanceState(input);
            return;
        }

        if (m_state == State::x_waiting)
        {
            return;
        }
        else
        {
            float timeFrac = m_time / (m_deadline * input.m_time);
            float sigmoid = m_erf.Compute(timeFrac);
            for (size_t i = 0; i < input.m_gangSize; ++i)
            {
                ProcessOne(i, timeFrac, sigmoid);
            }
        }
    }

    void AdvanceState(Input& input)
    {
        if (m_state == State::x_waiting || input.m_gen.UniGen() < 0.25)
        {
            InitMove(input);
        }
        else
        {
            InitWait(input);
        }
    }

    void InitMove(Input& input)
    {
        m_time = 0;
        m_deadline = std::abs(input.m_gen.NormGen() * x_timeSigma) + 1;
        float trgCenter = input.m_gen.UniGen();
        for (size_t i = 0; i < input.m_gangSize; ++i)
        {
            m_src[i] = m_pos[i];
            m_trg[i] = trgCenter + input.m_sigma * input.m_gen.NormGen();
            m_shape[i] = input.m_gen.UniGen();
        }

        m_state = State::x_moving;
    }

    void InitWait(Input& input)
    {
        m_time = 0;
        m_deadline = x_waitFrac * (std::abs(input.m_gen.NormGen() * x_timeSigma) + 1);
        m_state = State::x_waiting;       
    }

    void Init(Input& input)
    {
        float posCenter = input.m_gen.UniGen();
        for (size_t i = 0; i < x_maxSize; ++i)
        {
            m_pos[i] = posCenter + input.m_sigma * input.m_gen.NormGen();
        }

        m_didInit = true;

        AdvanceState(input);
    }
};

struct ManyGangedRandomLFO
{
    static constexpr size_t x_maxPoly = 16;
    GangedRandomLFOInternal m_lfos[x_maxPoly];
    
    struct Input : public GangedRandomLFOInternal::Input
    {
        size_t m_numGangs;
    };

    void Process(float dt, Input& input)
    {
        for (size_t i = 0; i < input.m_numGangs; ++i)
        {
            m_lfos[i].Process(dt, input);
        }
    }
};


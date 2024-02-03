#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "BlinkLight.hpp"
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
        float trgCenter = 10 * input.m_gen.UniGen();
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
        float posCenter = 10 * input.m_gen.UniGen();
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

struct GangedRandomLFO : Module
{
    static constexpr size_t x_outs = 8;
    static constexpr size_t x_maxPoly = 16;
    ManyGangedRandomLFO::Input m_state;
    ManyGangedRandomLFO m_lfos[x_outs];

    size_t TimeParamId()
    {
        return 0;
    }

    size_t SigmaParamId()
    {
        return TimeParamId() + 1;
    }

    size_t GangSizeParamId()
    {
        return SigmaParamId() + 1;
    }

    size_t NumGangsParamId()
    {
        return GangSizeParamId() + 1;
    }

    size_t NumParams()
    {
        return NumGangsParamId() + 1;
    }

    float GetTime()
    {
        return params[TimeParamId()].getValue();
    }

    float GetSigma()
    {
        return params[SigmaParamId()].getValue();
    }

    size_t GetGangSize()
    {
        return static_cast<size_t>(params[GangSizeParamId()].getValue() + 0.5);
    }

    size_t GetNumGangs()
    {
        return static_cast<size_t>(params[NumGangsParamId()].getValue() + 0.5);
    }

    size_t OutputId(size_t out)
    {
        return out;
    }

    size_t NumOutputs()
    {
        return OutputId(x_outs);
    }

    bool IsOutputPlugged(size_t out)
    {
        return outputs[OutputId(out)].isConnected();
    }

    void SetChannels(size_t out, size_t chans)
    {
        outputs[OutputId(out)].setChannels(chans);
    }

    void SetOutput(size_t out, size_t chan, float val)
    {
        val = std::max<float>(0, std::min<float>(val, 10));
        outputs[OutputId(out)].setVoltage(val, chan);
    }

    void PopulateInput()
    {
        m_state.m_time = GetTime();
        m_state.m_sigma = GetSigma();
        m_state.m_gangSize = GetGangSize();
        m_state.m_numGangs = GetNumGangs();
    }

    void process(const ProcessArgs &args) override
    {
        PopulateInput();

        for (size_t i = 0; i < x_outs; ++i)
        {
            if (IsOutputPlugged(i))
            {
                SetChannels(i, std::min(x_maxPoly, m_state.m_numGangs * m_state.m_gangSize));
                m_lfos[i].Process(args.sampleTime, m_state);
                for (size_t j = 0; j < m_state.m_numGangs; ++j)
                {
                    for (size_t k = 0; k < m_state.m_gangSize; ++k)
                    {
                        size_t ix = j * m_state.m_gangSize + k;
                        if (ix >= x_maxPoly)
                        {
                            break;
                        }
                        
                        SetOutput(i, ix, m_lfos[i].m_lfos[j].m_pos[k]);
                    }
                }
            }
        }
    }

    GangedRandomLFO()
    {
        config(NumParams(), 0, NumOutputs(), 0);

        configParam(TimeParamId(), 0.1, 600, 45, "Time (secs)");
        configParam(SigmaParamId(), 0.1, 20, 2, "Sigma");
        configParam(GangSizeParamId(), 1, 16, 3, "Gang Size");
        configParam(NumGangsParamId(), 1, 16, 3, "Num Gangs");
        
        for (size_t i = 0; i < x_outs; ++i)
        {
            configOutput(OutputId(i), ("Output " + std::to_string(i)));
        }
    }
};

struct GangedRandomLFOWidget : ModuleWidget
{
    GangedRandomLFOWidget(GangedRandomLFO* module)
    {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/GangedRandomLFO.svg")));
        
        float dx = 25;
        
        addParam(createParamCentered<Trimpot>(Vec(100, 100), module, module->TimeParamId()));
        addParam(createParamCentered<Trimpot>(Vec(100 + dx, 100), module, module->SigmaParamId()));
        addParam(createParamCentered<Trimpot>(Vec(100 + 2 * dx, 100), module, module->GangSizeParamId()));
        addParam(createParamCentered<Trimpot>(Vec(100 + 3 * dx, 100), module, module->NumGangsParamId()));

        float rowStart = 50;
        
        for (size_t i = 0; i < module->x_outs; ++i)
        {
            float rowPos = 100 + i * 30;
            addOutput(createOutputCentered<PJ301MPort>(Vec(rowStart, rowPos), module, module->OutputId(i)));
        }
    }
};

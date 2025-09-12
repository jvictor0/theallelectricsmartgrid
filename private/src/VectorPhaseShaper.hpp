#pragma once
#include <cstddef>
#include <cmath>
#include "plugin.hpp"
#include "Slew.hpp"
#include "WaveTable.hpp"
#include "PhaseUtils.hpp"
#include "ModuleUtils.hpp"

struct VectorPhaseShaperInternal
{
    const WaveTable* m_waveTable;
    float m_phase;
    float m_voct;
    float m_freq;
    float m_v;
    float m_floorV;
    float m_d;
    float m_b;
    float m_c;
    float m_out;
    float m_dScale;
    float m_phaseMod;
    bool m_top;

    struct Input
    {
        bool m_useVoct;
        float m_voct;
        float m_freq;
        float m_v;
        float m_d;
        float m_phaseMod;

        Input()
            : m_useVoct(true)
            , m_voct(0)
            , m_freq(0)
            , m_v(0.5)
            , m_d(0.5)
            , m_phaseMod(0)
        {
        }
    };

    void Process(const Input& input, float deltaT)
    {
        if (input.m_useVoct)
        {
            SetFreq(input.m_voct, deltaT);
        }
        else
        {
            m_freq = input.m_freq;
            SetDScale();
        }

        SetDV(input.m_d, input.m_v);
        m_phaseMod = input.m_phaseMod;
        UpdatePhase();
        Evaluate();
    }
    
    VectorPhaseShaperInternal()
        : m_waveTable(nullptr)
        , m_phase(0)
        , m_voct(NAN)
        , m_freq(0)
        , m_v(0)
        , m_floorV(0)
        , m_d(0)
        , m_b(0)
        , m_c(0)
        , m_out(0)
        , m_dScale(1.0f)
        , m_phaseMod(0)
        , m_top(false)
    {
        m_waveTable = &WaveTable::GetCosine();
        SetDV(0.5, 0.5);
    }

    void SetFreq(float voct, float delta)
    {
        if (voct != m_voct)
        {
            m_voct = voct;
            m_freq = PhaseUtils::VOctToNatural(voct, delta);
        }

        SetDScale();
    }

    void SetDScale()
    {
        // 4 * freq / d < 0.5
        // 8 * freq < d
        // dScale = max(0, 1 - 16 * freq)
        //
        m_dScale = std::max(0.0f, 1 - 16 * m_freq);
    }
    
    void SetDV(float d, float v)
    {
        m_d = d;
        v = std::max(0.0f, v);
        if (v != m_v)
        {
            m_v = v;
            m_floorV = std::floor(m_v);
            m_b = m_v - m_floorV;
            m_c = m_waveTable->Evaluate(m_b);
        }
    }
    
    void UpdatePhase()
    {
        m_top = false;
        m_phase += m_freq;
        while (m_phase >= 1)
        {
            m_top = true;
            m_phase -= 1;
        }
    }

    bool NeedsAntiAlias(float phi_vps)
    {
        if (m_floorV == 0)
        {
            return false;
        }
        else if (m_b < 0.5)
        {
            return m_floorV < phi_vps;
        }
        else
        {
            return m_floorV + 0.5 < phi_vps;
        }
    };

    void Evaluate()
    {
        float d = (m_d - 0.5) * m_dScale + 0.5;
        float phi_vps;
        float phase = m_phase + m_phaseMod;
        phase = phase - std::floor(phase);
        if (phase < d)
        {
            phi_vps = phase * m_v / d;
        }
        else
        {
            phi_vps = m_v + (phase - d) * (1 - m_v) / (1 - d);
        }

        if (!NeedsAntiAlias(phi_vps))
        {
            phi_vps = fmod(phi_vps, 1);
            m_out = - m_waveTable->Evaluate(phi_vps);
        }
        else
        {
            if (m_b < 0.5)
            {
                float phi_as = fmod(phi_vps, 1) / (2 * m_b);
                float s = - m_waveTable->Evaluate(phi_as);
                m_out = ((1 - m_c) * s - 1 - m_c) / 2;
            }
            else
            {
                float phi_as = fmod(phi_vps - 0.5, 1) / (2 * (m_b - 0.5)) + 0.5;
                float s = - m_waveTable->Evaluate(phi_as);
                m_out = ((1 + m_c) * s + 1 - m_c) / 2;
            }
        }
    }                    
};

template<size_t N>
struct Detour
{
    VectorPhaseShaperInternal m_vps[N];
    float m_modPhase[N];
    float m_modFreq[N];
    size_t m_which;
    float m_phase;
    float m_voct;
    float m_freq;
    float m_out;
    const WaveTable* m_sinWaveTable;

    Detour()
        : m_which(0)
        , m_phase(0)
        , m_voct(NAN)
        , m_freq(0)
        , m_out(0)
    {
        m_sinWaveTable = &WaveTable::GetSine();

        for (size_t i = 0; i < N; ++i)
        {
            m_modPhase[i] = 0;
            m_modFreq[i] = 0;
        }
    }
    
    struct Input
    {
        float m_voct;

        float m_modBlend[N];
        float m_modAmount[N];
        float m_modDMult[N];
        float m_modVMult[N];
        float m_modFreqOffset[N];
        float m_modVPhaseOffset[N];
        
        VectorPhaseShaperInternal::Input m_vps[N];

        Input()
            : m_voct(0)
        {
            for (size_t i = 0; i < N; ++i)
            {
                m_modBlend[i] = 0;
                m_modAmount[i] = 0;
                m_modDMult[i] = 1;
                m_modVMult[i] = 1;
                m_modFreqOffset[i] = 0;
                m_modVPhaseOffset[i] = 0.25;
            }
        }
    };
    
    void SetFreq(const Input& input, float delta)
    {
        if (input.m_voct != m_voct)
        {
            m_voct = input.m_voct;
            m_freq = PhaseUtils::VOctToNatural(input.m_voct, delta);

            for (size_t which = 0; which < N; ++which)
            {
                m_vps[which].SetFreq(m_voct, delta);
            }
        }

        for (size_t which = 0; which < N; ++which)
        {
            m_modFreq[which] = PhaseUtils::VOctToNatural(m_voct + input.m_modFreqOffset[which], delta);
        }
    }
    
    void UpdatePhase()
    {
        m_phase += m_freq;
        while (m_phase >= 1)
        {
            m_phase -= 1;
            m_which = (m_which + 1) % N;
        }

        for (size_t i = 0; i < N; ++i)
        {
            m_modPhase[i] += m_modFreq[i];
            while (m_modPhase[i] >= 1)
            {
                m_modPhase[i] -= 1;
            }
        }

        m_vps[m_which].m_phase = m_phase;
    }

    void SetDV(const Input& input)
    {
        for (size_t which = 0; which < N; ++which)
        {
            if (input.m_modAmount[which] == 0)
            {
                m_vps[which].SetDV(input.m_vps[which].m_d, input.m_vps[which].m_v);
            }
            else
            {
                float dMod = PhaseUtils::SyncedEval(
                    m_sinWaveTable,
                    m_modPhase[which],
                    input.m_modDMult[which],
                    0) / 2 + 0.5;
                float vMod = PhaseUtils::SyncedEval(
                    m_sinWaveTable,
                    m_modPhase[which],
                    input.m_modVMult[which],
                    input.m_modVPhaseOffset[which]) * 2 + 2;
                float dBlend = input.m_modAmount[which] * std::min<float>(1, 2 * input.m_modBlend[which]);
                float vBlend = input.m_modAmount[which] * std::min<float>(1, 2 - 2 * input.m_modBlend[which]);
                float d = (1 - dBlend) * input.m_vps[which].m_d + dBlend * dMod;
                float v = (1 - vBlend) * input.m_vps[which].m_v + vBlend * vMod;
                m_vps[which].SetDV(d, v);
            }
        }
    }

    void Process(const Input& input, float deltaT)
    {
        SetFreq(input, deltaT);
        UpdatePhase();
        SetDV(input);
        m_vps[m_which].Evaluate();
        m_out = m_vps[m_which].m_out;
    }
};

#ifndef IOS_BUILD
template<size_t N = 2>
struct VectorPhaseShaper : Module
{
    IOMgr m_ioMgr;
    IOMgr::Output* m_out;
    IOMgr::Input* m_voct;
    IOMgr::Input* m_d[N];
    IOMgr::Input* m_v[N];
    IOMgr::Input* m_modBlend[N];
    IOMgr::Input* m_modAmount[N];
    IOMgr::Input* m_modDMult[N];
    IOMgr::Input* m_modVMult[N];
    IOMgr::Input* m_modFreqOffset[N];
    IOMgr::Input* m_modVPhaseOffset[N];

    Detour<N> m_detour[16];
    typename Detour<N>::Input m_state[16];

    IOMgr::Output* m_dOut[N];
    IOMgr::Output* m_vOut[N];
    
   VectorPhaseShaper()
        : m_ioMgr(this)
    {
        m_out = m_ioMgr.AddOutput("Out", true);
        m_out->m_scale = 5;
        m_voct = m_ioMgr.AddInput("VOct", true);
        m_voct->m_offset = 1;

        for (size_t i = 0; i < 16; ++i)
        {
            m_out->SetSource(i, &m_detour[i].m_out);
            m_voct->SetTarget(i, &m_state[i].m_voct);
        }
        
        for (size_t which = 0; which < N; ++which)
        {
            m_d[which] = m_ioMgr.AddInput("D" + std::to_string(which + 1), true);
            m_d[which]->m_scale = 0.1;
            
            m_v[which] = m_ioMgr.AddInput("V" + std::to_string(which + 1), true);
            m_v[which]->m_scale = 4.0 / 10;

            m_modBlend[which] = m_ioMgr.AddInput("Mod Blend" + std::to_string(which + 1), true);
            m_modBlend[which]->m_scale = 0.1;

            m_modAmount[which] = m_ioMgr.AddInput("Mod Amount" + std::to_string(which + 1), true);
            m_modAmount[which]->m_scale = 0.1;

            m_modDMult[which] = m_ioMgr.AddInput("Mod D Mult" + std::to_string(which + 1), true);
            m_modDMult[which]->m_offset = 1;

            m_modVMult[which] = m_ioMgr.AddInput("Mod V Mult" + std::to_string(which + 1), true);
            m_modVMult[which]->m_offset = 1;

            m_modFreqOffset[which] = m_ioMgr.AddInput("Mod Freq Offset" + std::to_string(which + 1), true);
            m_modFreqOffset[which]->m_scale = 0.1;
            m_modFreqOffset[which]->m_offset = 1 - N;
            
            m_modVPhaseOffset[which] = m_ioMgr.AddInput("Mod V Phase Offset" + std::to_string(which + 1), true);
            m_modVPhaseOffset[which]->m_scale = 0.1;

            m_dOut[which] = m_ioMgr.AddOutput("D" + std::to_string(which + 1) + " Out", true);
            m_dOut[which]->m_scale = 10;

            m_vOut[which] = m_ioMgr.AddOutput("V" + std::to_string(which + 1) + " Out", true);
            m_vOut[which]->m_scale = 10.0 / 4;
            
            for (size_t i = 0; i < 16; ++i)
            {        
                m_d[which]->SetTarget(i, &m_state[i].m_vps[which].m_d);
                m_v[which]->SetTarget(i, &m_state[i].m_vps[which].m_v);
                m_modBlend[which]->SetTarget(i, &m_state[i].m_modBlend[which]);
                m_modAmount[which]->SetTarget(i, &m_state[i].m_modAmount[which]);
                m_modDMult[which]->SetTarget(i, &m_state[i].m_modDMult[which]);
                m_modVMult[which]->SetTarget(i, &m_state[i].m_modVMult[which]);
                m_modFreqOffset[which]->SetTarget(i, &m_state[i].m_modFreqOffset[which]);
                m_modVPhaseOffset[which]->SetTarget(i, &m_state[i].m_modVPhaseOffset[which]);

                m_dOut[which]->SetSource(i, &m_detour[i].m_vps[which].m_d);
                m_vOut[which]->SetSource(i, &m_detour[i].m_vps[which].m_v);
            }
        }
            
        m_ioMgr.Config();
    }

    void process(const ProcessArgs &args) override
    {
        m_ioMgr.Process();
        for (int i = 0; i < m_voct->m_value.m_channels; ++i)
        {
            m_detour[i].Process(m_state[i], args.sampleTime);
        }

        if (m_ioMgr.IsControlFrame())
        {
            m_out->SetChannels(m_voct->m_value.m_channels);
        }
        
        m_ioMgr.SetOutputs();
    }
};

template<size_t N>
struct VectorPhaseShaperWidget : public ModuleWidget
{
    VectorPhaseShaperWidget(VectorPhaseShaper<N>* module)
    {
        setModule(module);

        if (N == 1)
        {
            setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VectorPhaseShaper.svg")));
        }
        else
        {
            setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DeDeTour.svg")));
        }

        if (module)
        {
            module->m_voct->Widget(this, 1, 10);
            for (size_t i = 0; i < N; ++i)
            {
                module->m_d[i]->Widget(this, 2 + 2 * i, 10);
                module->m_v[i]->Widget(this, 3 + 2 * i, 10);
                module->m_modBlend[i]->Widget(this, 2 + 2 * i, 11);
                module->m_modAmount[i]->Widget(this, 3 + 2 * i, 11);
                module->m_modDMult[i]->Widget(this, 2 + 2 * i, 12);
                module->m_modVMult[i]->Widget(this, 3 + 2 * i, 12);
                module->m_modFreqOffset[i]->Widget(this, 2 + 2 * i, 13);
                module->m_modVPhaseOffset[i]->Widget(this, 3 + 2 * i, 13);

                module->m_dOut[i]->Widget(this, 2 + 2 * i, 14);
                module->m_vOut[i]->Widget(this, 3 + 2 * i, 14);
            }
            
            module->m_out->Widget(this, 6, 10);
        }
    }
};
#endif

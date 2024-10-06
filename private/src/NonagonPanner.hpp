#pragma once
#include "LinAlg.hpp"
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "Trig.hpp"
#include "NormGen.hpp"

typedef linalg::aliases::float3 Vector;
typedef linalg::aliases::float4 Quat;

Quat ToQuat(Vector& v, float phase)
{
    float s = std::sin(phase * M_PI);
    float c = std::cos(phase * M_PI);
    return Quat(v * s, c);
}

Vector Rotate(Vector v, Vector& n, float phase)
{
    return linalg::qrot(ToQuat(n, phase), v);
}

float Reduce(float x)
{
    return x - floor(x);
}

Vector SphereToCart(float azimuth, float inclination)
{
    float si = std::sin(inclination * M_PI);
    float ci = std::cos(inclination * M_PI);
    float sa = std::sin(2 * azimuth * M_PI);
    float ca = std::cos(2 * azimuth * M_PI);
    return Vector(si * ca, si * sa, ci);
}

struct PhasorSwitcher
{
    int m_valIx;
    int m_desiredIx;
    float* m_options[6];

    float F(int ix)
    {
        if (ix == -1)
        {
            return 0;
        }
        if (ix == -2)
        {
            return 0;
        }
        else
        {
            return *m_options[ix];
        }
    }
    
    PhasorSwitcher()
        : m_valIx(-1)
        , m_desiredIx(-2)
    {
    }

    void Do()
    {
        if (m_valIx == -2 && m_desiredIx != -2)
        {
            m_valIx = m_desiredIx;
        }
        else if (m_desiredIx != -2)
        {        
            float v1 = F(m_valIx);
            float v2 = F(m_desiredIx);
            if (std::abs(v1 - v2) < 1.0 / 256)
            {
                m_valIx = m_desiredIx;
                m_desiredIx = -2;
            }
        }
    }

    float Get()
    {
        Do();
        return F(m_valIx);
    }

    void Desire(int ix)
    {
        if (ix == m_valIx)
        {
            m_desiredIx = -1;
        }
        else
        {
            m_desiredIx = ix;
        }
    }
};

struct OrbiterLFOs
{
    ManyGangedRandomLFO::Input m_state;
    ManyGangedRandomLFO m_lfo;

    OrbiterLFOs()
    {
        m_state.m_numGangs = 10;
        m_state.m_gangSize = 1;
        m_state.m_time = 10;
    }

    void Process(float dt)
    {
        m_lfo.Process(dt, m_state);
    }
};

struct Orbiter
{
    int m_phaseOffset[9];
    float m_mainPhase;
    OrbiterLFOs m_lfos;

    float MainPhase(int offset)
    {
        return Reduce(m_mainPhase + static_cast<float>(offset) / 9);
    }

    float Radius(float)
    {
        return 1;
    }
    
    struct Input
    {
        float m_radMod[9];

        float m_azimuth;
        float m_azimuthOffset[3];
        float m_azimuthSpread;

        float m_inclinationScale;
        float m_inclination[3];

        float m_mainFreq;

        Vector GetNormal(size_t ix)
        {
            float a = m_azimuth + m_azimuthSpread * m_azimuthOffset[ix];
            float i = m_inclinationScale * m_inclination[ix];
            return SphereToCart(a, i);
        }

        Vector GetStraight(size_t ix)
        {
            float a = m_azimuth + m_azimuthSpread * m_azimuthOffset[ix];
            float i = m_inclinationScale * m_inclination[ix] + 0.5;
            return SphereToCart(a, i);
        }
        
        void PopulateFromLFOs(OrbiterLFOs& lfos)
        {
            float* trgs[] = {
                &m_azimuth,
                &m_azimuthOffset[0],
                &m_azimuthOffset[1],
                &m_azimuthOffset[2],
                &m_azimuthSpread,
                &m_inclinationScale,
                &m_inclination[0],
                &m_inclination[1],
                &m_inclination[2],
            };

            for (size_t i = 0; i < 9; ++i)
            {
                *trgs[i] = lfos.m_lfo.m_lfos[i].m_pos[0] / 10;
            }

            m_azimuthSpread = std::max<float>(0, std::min<float>(m_azimuthSpread, 1));
            m_inclinationScale = std::max<float>(0, std::min<float>(m_inclinationScale, 1));
            float normFreq = lfos.m_lfo.m_lfos[9].m_pos[0] / 10;
            m_mainFreq = 1.0 / 60.0 * std::exp2f(normFreq * std::log2f(60.0 / 5.0));
        }
        
        Input()
            : m_azimuth(0)
            , m_azimuthSpread(0)
            , m_inclinationScale(0)
        {
            for (size_t i = 0; i < 9; ++i)
            {
                m_radMod[i] = 0;
            }
            
            for (size_t i = 0; i < 3; ++i)
            {
                m_azimuthOffset[i] = 0;
                m_inclination[i] = 0;
            }
        }
    };    

    struct Output
    {
        Vector m_pos[9];
    };

    Output m_output;

    void ReadInputs(Input& input)
    {
        input.PopulateFromLFOs(m_lfos);
    }

    void Process(float dt, Input& input)
    {
        m_lfos.Process(dt);        
        ReadInputs(input);
        m_mainPhase = Reduce(m_mainPhase + input.m_mainFreq * dt);
        for (size_t i = 0; i < 3; ++i)
        {
            Vector mainNormal = input.GetNormal(i);
            Vector straight = input.GetStraight(i);
            for (size_t j = 0; j < 3; ++j)
            {
                if (i == 2 && j == 0)
                {
                    m_output.m_pos[3 * i + j] = Vector(0, 0, 0);
                }
                else
                {
                    size_t ix = 3 * i + j;
                    int offset = m_phaseOffset[ix];
                    
                    float phase = MainPhase(offset);
                    float rad = Radius(input.m_radMod[ix]);
                    Vector mainPos = Rotate(rad * straight, mainNormal, phase);
                    m_output.m_pos[ix] = mainPos;
                }
            }
        }
    }
    
    Orbiter()
    {
        static constexpr int oa[] = {0, 4, 2};
        
        m_mainPhase = 0;
        for (size_t i = 0; i < 3; ++i)
        {           
            for (size_t j = 0; j < 3; ++j)
            {
                int o = oa[i];
                m_phaseOffset[3 * i + j] = o + 3 * j;
            }
        }        
    }
};

#pragma once

#include "AdaptiveWaveTable.hpp"
#include "MultiplicativeSpectra.hpp"
#include "FixedAllocator.hpp"
#include "VectorPhaseShaper.hpp"

struct RandomWaveTable
{
    static constexpr size_t x_maxWaveTables = 16;

    static std::random_device s_rd;
    static std::mt19937 s_gen;
    static std::uniform_real_distribution<float> s_uniform;

    float m_coefficients[DiscreteFourierTransform::x_maxComponents];

    bool m_coefficientsReady;
    bool m_isReady;

    AdaptiveWaveTable* m_waveTables[x_maxWaveTables];
    size_t m_waveTableCount;

    void AddWaveTable(AdaptiveWaveTable* waveTable)
    {
        m_waveTables[m_waveTableCount] = waveTable;
        ++m_waveTableCount;
    }

    AdaptiveWaveTable* PopWaveTable()
    {
        assert(m_waveTableCount > 0);
        --m_waveTableCount;
        return m_waveTables[m_waveTableCount];
    }

    struct Input
    {
        static constexpr size_t x_maxPrimes = 10;
        int m_primes[x_maxPrimes];
        size_t m_primeCount;

        void AddPrime(int prime)
        {
            m_primes[m_primeCount] = prime;
            ++m_primeCount;
        }

        void Clear()
        {
            m_primeCount = 3;
            m_primes[0] = 2;
            m_primes[1] = 3;
            m_primes[2] = 5;
        }

        Input()
            : m_primeCount(0)
        {
        }
    };

    RandomWaveTable()
    {
        Init();
    }

    void Init()
    {
        m_waveTableCount = 0;
        m_coefficientsReady = false;
        m_isReady = false;

        for (int i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_coefficients[i] = 0;
        }
    }

    void SetCoefficientsFromMultiplicativeSpectra(MultiplicativeSpectra& ms)
    {
        for (int i = 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            m_coefficients[i] = ms.GetCoefficient(i);
        }
    }

    void FuzzCoefficients()
    {
        float mu = 1.0f;
        float b = 0.1f;

        for (int i = 0; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            float u = s_uniform(s_gen) - 0.5f;
            float fuzzFactor = mu - b * static_cast<float>((u < 0 ? 1 : -1)) * std::log1p(-2.0f * std::abs(u));
            m_coefficients[i] = m_coefficients[i] * fuzzFactor;
        }
    }

    void ThinSpectrum()
    {
        while (s_uniform(s_gen) < 0.25)
        {
            int mod = 2;
            while (s_uniform(s_gen) < 0.5)
            {
                ++mod;
            }

            bool used[mod];
            for (size_t i = 0; i < mod; ++i)
            {
                used[i] = false;
            }
     
            while (true)
            {
                int shard = s_uniform(s_gen) * mod;
                if (used[shard])
                {
                    break;
                }

                used[shard] = true;

                if (shard != 0)
                {
                    ++shard;
                }

                bool foundNonZero = false;
                for (size_t i = 2; i < DiscreteFourierTransform::x_maxComponents; ++i)
                {
                    if (i % mod != shard && m_coefficients[i] != 0)
                    {
                        foundNonZero = true;
                        break;
                    }
                }

                if (!foundNonZero)
                {
                    return;
                }

                for (size_t i = 2; i < DiscreteFourierTransform::x_maxComponents; ++i)
                {
                    if (i % mod == shard)
                    {
                        m_coefficients[i] = 0;
                    }
                }
            }
        }
    }

    int HighPass()
    {
        int highPass = 2;
        if (s_uniform(s_gen) < 0.33)
        {
            ++highPass;
            while (s_uniform(s_gen) < 0.825 && highPass < 32)
            {
                ++highPass;
            }

            float u = s_uniform(s_gen);
            float slope = (std::powf(9.0, u) - 1) / (9.0 - 1);
            for (int i = 2; i < highPass; ++i)
            {
                float factor = std::powf(slope, std::log2f(static_cast<float>(highPass) / static_cast<float>(i)));
                m_coefficients[i] *= factor;
            }
        }

        return highPass;
    }

    int Bump(int minHarmonic)
    {
        if (s_uniform(s_gen) < 0.33)
        {
            float polarity = s_uniform(s_gen) < 0.5 ? 1 : -1;
            float logPick = std::log2f(minHarmonic) + s_uniform(s_gen) * (std::log2f(64) - std::log2f(minHarmonic));
            int pick = static_cast<int>(std::round(std::powf(2.0, logPick)));
            float u = s_uniform(s_gen);
            float newVal = (std::powf(1.0 / static_cast<float>(pick), u) - 1) / (1.0 / static_cast<float>(pick) - 1);
            m_coefficients[pick] = polarity * newVal;
            int below = pick;
            while (s_uniform(s_gen) < 0.5 && minHarmonic < below)
            {
                --below;
            }

            for (int i = below; i < pick; ++i)
            {
                float blend = (i + 1 - static_cast<float>(below)) / (static_cast<float>(pick) - static_cast<float>(below) + 2);
                m_coefficients[i] = polarity * (m_coefficients[i] * blend + newVal * (1 - blend));
            }

            int above = pick;
            while (s_uniform(s_gen) < 0.5 && above < 64)
            {
                ++above;
            }

            for (int i = pick + 1; i <= above; ++i)
            {
                float blend = (static_cast<float>(above) + 1 - static_cast<float>(i)) / (static_cast<float>(above) - static_cast<float>(pick) + 2);
                m_coefficients[i] = polarity * (m_coefficients[i] * blend + newVal * (1 - blend));
            }

            return above;
        }

        return minHarmonic;
    }

    void LowPass(int minHarmonic)
    {
        float logPick = std::log2f(minHarmonic) + s_uniform(s_gen) * (std::log2f(DiscreteFourierTransform::x_maxComponents) - std::log2f(minHarmonic));
        int pick = static_cast<int>(std::round(std::powf(2.0, logPick)));
        float u = s_uniform(s_gen);
        float slope = (std::powf(9.0, u) - 1) / (9.0 - 1);
        for (int i = pick + 1; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            float factor = std::powf(slope, std::log2f(static_cast<float>(i) / static_cast<float>(pick)));
            m_coefficients[i] *= factor;
        }
    }

    void EQ()
    {
        int highPass = HighPass();
        int bump = Bump(highPass);
        LowPass(bump);
    }

    static void GenerateMSPrime(int prime, MultiplicativeSpectra& ms)
    {
        if (prime == 2)
        {
            ms.m_coefficients[prime] = s_uniform(s_gen);
        }
        else
        {
            // If you're reading this fuck you.  There's very good reasons for these numbers.  
            //
            float a = static_cast<float>((prime - 1) * (prime - 1));
            float u = s_uniform(s_gen);
            ms.m_coefficients[prime] = (std::powf(a, u) - 1) / (a - 1);
        }
    }

    static void WriteDistortedHarmonic(int harmonic, float value, DiscreteFourierTransform& dft)
    {
        float factors[] = {-0.347, -0.166, 0.041, 0.044, 0.02, 0.008, 0.004, 0.003};
        for (int i = 0; i < 8; ++i)
        {
            if (harmonic * (i + 1) < DiscreteFourierTransform::x_maxComponents)
            {
                dft.m_components[harmonic * (i + 1)] += std::complex<float>(0, value * factors[i]);
            }
            else
            {
                break;
            }
        }
    }

    void GenerateWaveTable(BasicWaveTable& waveTable)
    {
        DiscreteFourierTransform dft;
        
        dft.m_components[1] = std::complex<float>(0.5, 0);
        
        for (int i = 2; i < DiscreteFourierTransform::x_maxComponents; ++i)
        {
            WriteDistortedHarmonic(i, m_coefficients[i], dft);            
        }

        dft.InverseTransform(waveTable, DiscreteFourierTransform::x_maxComponents);
        waveTable.NormalizeAmplitude();
    }

    void GenerateWaveTable(AdaptiveWaveTable& waveTable)
    {
        GenerateWaveTable(waveTable.GetWaveTable());
        waveTable.m_waveTableReady = true;
    }

    void GenerateCoefficients(Input& input)
    {
        MultiplicativeSpectra ms;
        for (size_t i = 0; i < input.m_primeCount; ++i)
        {            
            GenerateMSPrime(input.m_primes[i], ms);
        }

        SetCoefficientsFromMultiplicativeSpectra(ms);
        
        EQ();

        ThinSpectrum();
        
        m_coefficientsReady = true;
    }

    void GenerateIncrementally(Input& input)
    {
        if (!m_coefficientsReady)
        {
            GenerateCoefficients(input);
            return;
        }

        for (size_t i = 0; i < m_waveTableCount; ++i)
        {
            if (!m_waveTables[i]->m_waveTableReady)
            {
                FuzzCoefficients();
                GenerateWaveTable(*m_waveTables[i]);
                return;
            }

            if (!m_waveTables[i]->IsReady())
            {
                m_waveTables[i]->GenerateIncrementally();
                return;
            }
        }

        m_isReady = true;
    }

    void GenerateCompletely(Input& input)
    {
        while (!m_isReady)
        {
            GenerateIncrementally(input);
        }
    }
};

inline std::random_device RandomWaveTable::s_rd;
inline std::mt19937 RandomWaveTable::s_gen(s_rd());
inline std::uniform_real_distribution<float> RandomWaveTable::s_uniform(0.0f, 1.0f);

struct SquiggleBoyWaveTableGenerator
{
    static constexpr size_t x_gangSize = 3;
    static constexpr size_t x_numGangs = 3;
    RandomWaveTable m_randomWaveTable;
    FixedAllocator<AdaptiveWaveTable, (2 * x_numGangs + 1) * x_gangSize> m_waveTableAllocator;
    RandomWaveTable::Input m_state;

    bool m_leftVisible[x_numGangs];
    bool m_rightVisible[x_numGangs];

    SquiggleBoyWaveTableGenerator()
    {
        for (size_t i = 0; i < x_numGangs; ++i)
        {
            m_leftVisible[i] = true;
            m_rightVisible[i] = true;
        }
    }

    void ProcessFrame()
    {
        if (!m_randomWaveTable.m_coefficientsReady)   
        {
            m_randomWaveTable.GenerateCoefficients(m_state);
            return;
        }

        while (m_randomWaveTable.m_waveTableCount < x_gangSize)
        {
            AdaptiveWaveTable* waveTable = m_waveTableAllocator.Allocate();
            assert(waveTable);
            waveTable->Init();
            if (!waveTable)
            {
                return;
            }

            m_randomWaveTable.AddWaveTable(waveTable);
        }

        if (!m_randomWaveTable.m_isReady)
        {
            m_randomWaveTable.GenerateIncrementally(m_state);
            return;
        }
    }

    void GenerateCompletely()
    {
        while (m_randomWaveTable.m_waveTableCount < x_gangSize)
        {
            AdaptiveWaveTable* waveTable = m_waveTableAllocator.Allocate();
            assert(waveTable);
            waveTable->Init();
            m_randomWaveTable.AddWaveTable(waveTable);
        }

        m_randomWaveTable.GenerateCompletely(m_state);
    }

    void SetLeft(VectorPhaseShaperInternal* vps)
    {
        AdaptiveWaveTable* waveTable = vps->SetLeft(m_randomWaveTable.PopWaveTable());
        if (waveTable)
        {
            FreeWaveTable(waveTable);
        }
    }
    
    void SetRight(VectorPhaseShaperInternal* vps)
    {
        AdaptiveWaveTable* waveTable = vps->SetRight(m_randomWaveTable.PopWaveTable());
        if (waveTable)
        {
            FreeWaveTable(waveTable);
        }
    }

    void FreeWaveTable(AdaptiveWaveTable* waveTable)
    {
        m_waveTableAllocator.Free(waveTable);
    }

    bool IsReady() const
    {
        return m_randomWaveTable.m_isReady;
    }

    void Clear()
    {
        m_randomWaveTable.Init();
        m_state.Clear();
    }

    void AddPrime(int prime)
    {
        m_state.AddPrime(prime);
    }

    AdaptiveWaveTable* GetWaveTable(size_t index)
    {
        return m_randomWaveTable.m_waveTables[index];
    }
};
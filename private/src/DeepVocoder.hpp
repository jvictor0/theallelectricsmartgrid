#pragma once

#include "Resynthesis.hpp"
#include "PhaseUtils.hpp"

struct DeepVocoder
{
    struct Input
    {
        PhaseUtils::ExpParam m_initDeadline;
        PhaseUtils::ExpParam m_tolerance;
        PhaseUtils::ExpParam m_minMagnitude;
        float m_ratio;

        Input()
            : m_initDeadline(10 * 48000.0 / Resynthesizer::x_H, 10 * 60 * 48000.0 / Resynthesizer::x_H)
            , m_tolerance(32.0 / 31.0, 5.0 / 4.0)
            , m_minMagnitude(0.001f, 0.2f)
            , m_ratio(1.0f)
        {
        }
    };

    struct Frequency
    {
        float m_omega;
        float m_magnitude;
        int m_deadline;

        Frequency()
            : m_omega(0.0f)
            , m_magnitude(0.0f)
            , m_deadline(0)
        {
        }

        Frequency(float omega, float magnitude, int deadline)
            : m_omega(omega)
            , m_magnitude(magnitude)
            , m_deadline(deadline)
        {
        }

        bool operator<(const Frequency& other) const
        {
            return m_omega < other.m_omega;
        }

        bool IsBetter(const Frequency& other) const
        {
            return m_deadline > other.m_deadline ||
                   (m_deadline == other.m_deadline && m_magnitude > other.m_magnitude);
        }

        bool Within(const Frequency& other, float tolerance) const
        {
            float ratio = m_omega / other.m_omega;
            if (ratio < tolerance && 1.0f / tolerance < ratio)
            {
                return true;
            }

            if (std::abs(m_omega - other.m_omega) < Resynthesizer::OmegaBin(1))
            {
                return true;
            }

            return false;
        }
    };

    struct FrequencyList
    {
        static constexpr size_t x_maxFrequencies = 4 * Resynthesizer::x_maxComponents;
        Frequency m_frequencies[x_maxFrequencies];
        size_t m_size;

        FrequencyList()
            : m_size(0)
        {
        }

        void Add(const Frequency& frequency, float tolerance)
        {
            if (m_size >= x_maxFrequencies)
            {
                return;
            }

            while (m_size > 0)
            {
                if (m_frequencies[m_size - 1].Within(frequency, tolerance))
                {
                    if (m_frequencies[m_size - 1].IsBetter(frequency))
                    {
                        return;
                    }

                    --m_size;
                }
                else
                {
                    break;
                }
            }

            m_frequencies[m_size] = frequency;
            ++m_size;
        }

        void AdvanceDeadline()
        {
            for (size_t i = 0; i < m_size; ++i)
            {
                --m_frequencies[i].m_deadline;
            }
        }

        void AdvanceFrequencyIndex(size_t& index) const
        {
            while (index < m_size && m_frequencies[index].m_deadline <= 0)
            {
                ++index;
            }
        }

        void Merge(const FrequencyList& other, DeepVocoder& owner, Input& input)
        {
            size_t otherIndex = 0;
            size_t bin = 1;
            owner.AdvanceBinFrequency(bin, input);
            other.AdvanceFrequencyIndex(otherIndex);            
            while (bin < Resynthesizer::x_maxComponents && otherIndex < other.m_size)
            {
                Frequency newFrequency = owner.MakeFrequency(bin, input);
                Frequency otherFrequency = other.m_frequencies[otherIndex];
                if (newFrequency < otherFrequency)
                {
                    Add(newFrequency, input.m_tolerance.m_expParam);
                    ++bin;
                    owner.AdvanceBinFrequency(bin, input);
                }
                else
                {
                    Add(otherFrequency, input.m_tolerance.m_expParam);                    
                    ++otherIndex;
                    other.AdvanceFrequencyIndex(otherIndex);
                }
            }

            while (bin < Resynthesizer::x_maxComponents)
            {
                Frequency newFrequency = owner.MakeFrequency(bin, input);
                Add(newFrequency, input.m_tolerance.m_expParam);
                ++bin;
                owner.AdvanceBinFrequency(bin, input);
            }

            while (otherIndex < other.m_size)
            {
                Frequency otherFrequency = other.m_frequencies[otherIndex];
                Add(otherFrequency, input.m_tolerance.m_expParam);
                ++otherIndex;
                other.AdvanceFrequencyIndex(otherIndex);
            }
        }

        void Swap(FrequencyList& other)
        {
            FrequencyList temp = *this;
            *this = other;
            other = temp;
        }

        FrequencyList& operator=(const FrequencyList& other)
        {
            m_size = other.m_size;
            for (size_t i = 0; i < m_size; ++i)
            {
                m_frequencies[i] = other.m_frequencies[i];
            }

            return *this;
        }

        float Search(float omega)
        {
            auto itr = std::lower_bound(m_frequencies, m_frequencies + m_size, Frequency(omega, 0.0f, 0));
            if (itr == m_frequencies + m_size)
            {
                if (0 < m_size)
                {
                    return m_frequencies[m_size - 1].m_omega;
                }
                else
                {
                    return omega;
                }
            }

            if (itr == m_frequencies)
            {
                return itr->m_omega;
            }

            auto prev_itr = itr - 1;
            if (std::abs(prev_itr->m_omega - omega) <= std::abs(itr->m_omega - omega))
            {
                return prev_itr->m_omega;
            }
            else
            {
                return itr->m_omega;
            }
        }
    };

    DeepVocoder()
      : m_pvdr(&m_resynthesizer)
      , m_index(0)
      , m_minMagnitude(0.0f)
    {
        for (size_t i = 0; i < Resynthesizer::x_tableSize; ++i)
        {
            m_buffer[i] = 0.0f;
        }
    }

    void Process(float inputSample, Input& input)
    {
        m_buffer[m_index % Resynthesizer::x_tableSize] = inputSample;
        ++m_index;
        if (m_index % Resynthesizer::x_H == 0)
        {
            m_ratio = std::powf(2.0f, std::round(input.m_ratio * 5 - 2.5));
            m_minMagnitude = input.m_minMagnitude.m_expParam;
            Resynthesizer::Buffer buffer;
            for (size_t i = 0; i < Resynthesizer::x_tableSize; ++i)
            {
                buffer.m_table[i] = m_buffer[(m_index + i) % Resynthesizer::x_tableSize] * Math4096::Hann(i);
            }
         
            m_resynthesizer.PrimeAndAnalyze(&m_pvdr, buffer);
            FrequencyList newFrequencies;
            m_frequencies.AdvanceDeadline();
            newFrequencies.Merge(m_frequencies, *this, input);
            m_frequencies.Swap(newFrequencies);
        }
    }

    Frequency MakeFrequency(size_t bin, Input& input)
    {
        float omega = m_resynthesizer.m_omegaInstantaneous[bin] / (2.0f * M_PI);
        float magnitude = m_resynthesizer.m_analysisMagnitudes[bin];
        int deadline = std::max<int>(1, static_cast<int>(input.m_initDeadline.m_expParam) * magnitude);
        return Frequency(omega, magnitude, deadline);
    }

    void AdvanceBinFrequency(size_t& bin, Input& input)
    {     
        while (bin < Resynthesizer::x_maxComponents && 
               (!m_pvdr.IsLeader(bin) ||
                m_resynthesizer.m_analysisMagnitudes[bin] < input.m_minMagnitude.m_expParam ||
                std::abs(m_resynthesizer.m_omegaInstantaneous[bin]) < Resynthesizer::OmegaBin(1)))
        {
            ++bin;
        }
    }

    float Search(float omega, size_t index)
    {
        float result = m_frequencies.Search(omega * m_ratio);
        m_usedOmegas[index] = result;
        return result / m_ratio;
    }

    struct UIState
    {
        static constexpr size_t x_uiBufferSize = 16;
        std::atomic<float> m_threshold;
        std::atomic<size_t> m_numFrequencies[x_uiBufferSize];
        std::atomic<size_t> m_which;
        std::atomic<float> m_frequencies[x_uiBufferSize][FrequencyList::x_maxFrequencies];
        std::atomic<float> m_usedOmegas[9];

        UIState()
            : m_threshold(0.0f)
            , m_which(0)
        {
            for (size_t i = 0; i < x_uiBufferSize; ++i)
            {
                m_numFrequencies[i].store(0);
                for (size_t j = 0; j < FrequencyList::x_maxFrequencies; ++j)
                {
                    m_frequencies[i][j].store(0.0f);
                }
            }

            for (size_t i = 0; i < 9; ++i)
            {
                m_usedOmegas[i].store(0.0f);
            }
        }

        size_t Which()
        {
            return m_which.load();
        }

        float GetThreshold()
        {
            return m_threshold.load();
        }

        size_t GetNumFrequencies(size_t which)
        {
            return m_numFrequencies[which].load();
        }

        float GetFrequency(size_t which, size_t index)
        {
            return m_frequencies[which][index].load();
        }

        float GetUsedOmega(size_t index)
        {
            return m_usedOmegas[index].load();
        }
    };

    void PopulateUIState(UIState* uiState)
    {
        uiState->m_threshold.store(m_minMagnitude);

        size_t which = (uiState->m_which.load() + 1) % UIState::x_uiBufferSize;
        uiState->m_numFrequencies[which].store(0);
        for (size_t i = 0; i < m_frequencies.m_size; ++i)
        {
            uiState->m_frequencies[which][i].store(m_frequencies.m_frequencies[i].m_omega);
        }

        uiState->m_numFrequencies[which].store(m_frequencies.m_size);
        uiState->m_which.store(which);

        for (size_t i = 0; i < 9; ++i)
        {
            uiState->m_usedOmegas[i].store(m_usedOmegas[i]);
        }
    }

    Resynthesizer m_resynthesizer;
    Resynthesizer::PVDR m_pvdr;
    float m_buffer[Resynthesizer::x_tableSize];
    size_t m_index;
    FrequencyList m_frequencies;
    float m_usedOmegas[9];
    float m_ratio;
    float m_minMagnitude;
};
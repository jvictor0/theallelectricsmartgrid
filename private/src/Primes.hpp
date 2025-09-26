#pragma once

struct Primes
{
    static constexpr int x_max = 1025;

    int m_highestFactor[x_max];
    int m_quotient[x_max];

    Primes()
    {
        for (int i = 1; i < x_max; ++i)
        {
            m_highestFactor[i] = 1;
            m_quotient[i] = i;
            for (int j = 1; j * j <= i; ++j)
            {
                if (i % j == 0)
                {
                    m_highestFactor[i] = j;
                    m_quotient[i] = i / j;
                }
            }

            if (m_highestFactor[i] == 1)
            {
                m_highestFactor[i] = i;
                m_quotient[i] = 1;
            }
        }
    }

    struct Iterator
    {
        int m_cur;
        Primes* m_primes;

        Iterator(Primes* primes, int cur)
            : m_cur(cur)
            , m_primes(primes)
        {
        }
        
        int Get()
        {
            return m_primes->m_highestFactor[m_cur];
        }

        void Next()
        {
            m_cur = m_primes->m_quotient[m_cur];
        }

        bool Done()
        {
            return m_cur == 1;
        }
    };

    Iterator Begin(int cur)
    {
        return Iterator(this, cur);
    }

    Iterator End()
    {
        return Iterator(this, 1);
    }

    static Primes s_primes;
};

inline Primes Primes::s_primes;
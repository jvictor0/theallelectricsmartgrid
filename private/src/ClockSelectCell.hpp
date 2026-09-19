#include "PercentileSequencer.hpp"
#include "IndexArp.hpp"

struct ClockSelectCell : public SmartGrid::Cell
{
    virtual ~ClockSelectCell()
    {
    }

    int* m_clockSelect;
    int* m_resetSelect;
    State* m_clockSelectState;
    State* m_resetSelectState;
    bool* m_externalReset;
    size_t m_trio;
    size_t* m_numHeld;
    size_t* m_maxHeld;
    int m_myClock;
    SmartGrid::Color m_offColor;
    SmartGrid::Color m_clockColor;
    SmartGrid::Color m_resetColor;
    bool m_unsetOnRelease;

    ClockSelectCell(
        PercentileSequencerInternal::Input* state,
        size_t trio,
        size_t* numHeld,
        size_t* maxHeld,
        int myClock,
        SmartGrid::Color offColor,
        SmartGrid::Color clockColor,
        SmartGrid::Color resetColor)
        : m_clockSelect(state->m_clockSelect)
        , m_resetSelect(state->m_resetSelect)
        , m_clockSelectState(nullptr)
        , m_resetSelectState(nullptr)
        , m_externalReset(state->m_externalReset)
        , m_trio(trio)
        , m_numHeld(numHeld)
        , m_maxHeld(maxHeld)
        , m_myClock(myClock)
        , m_offColor(offColor)
        , m_clockColor(clockColor)
        , m_resetColor(resetColor)
        , m_unsetOnRelease(false)
    {
    }

    ClockSelectCell(
        State* clockSelectState,
        State* resetSelectState,
        size_t trio,
        size_t* numHeld,
        size_t* maxHeld,
        int myClock,
        SmartGrid::Color offColor,
        SmartGrid::Color clockColor,
        SmartGrid::Color resetColor)
        : m_clockSelect(nullptr)
        , m_resetSelect(nullptr)
        , m_clockSelectState(clockSelectState)
        , m_resetSelectState(resetSelectState)
        , m_externalReset(nullptr)
        , m_trio(trio)
        , m_numHeld(numHeld)
        , m_maxHeld(maxHeld)
        , m_myClock(myClock)
        , m_offColor(offColor)
        , m_clockColor(clockColor)
        , m_resetColor(resetColor)
        , m_unsetOnRelease(false)
    {
    }

    int ClockSelect()
    {
        if (m_clockSelectState)
        {
            return m_clockSelectState->Get<int>();
        }

        return m_clockSelect[m_trio];
    }

    void SetClockSelect(int clock)
    {
        if (m_clockSelectState)
        {
            m_clockSelectState->Set(clock);
        }
        else
        {
            m_clockSelect[m_trio] = clock;
        }
    }

    int ResetSelect()
    {
        if (m_resetSelectState)
        {
            return m_resetSelectState->Get<int>();
        }

        return m_resetSelect[m_trio];
    }

    void SetResetSelect(int clock)
    {
        if (m_resetSelectState)
        {
            m_resetSelectState->Set(clock);
        }
        else
        {
            m_resetSelect[m_trio] = clock;
        }
    }

    virtual void OnPress(uint8_t) override
    {
        ++(*m_numHeld);
        
        if (*m_numHeld == 1)
        {
            if (ClockSelect() == m_myClock)
            {
                m_unsetOnRelease = true;
            }
            else
            {
                SetClockSelect(m_myClock);
            }

            SetResetSelect(-1);
        }
        else if (*m_numHeld == 2)
        {
            SetResetSelect(m_myClock);
        }

        *m_maxHeld = std::max<size_t>(*m_maxHeld, *m_numHeld);
    }

    virtual void OnRelease() override
    {
        --(*m_numHeld);
        
        if (*m_numHeld == 0)
        {
            if (m_externalReset)
            {
                m_externalReset[m_trio] = true;
            }
                
            if (*m_maxHeld == 1 && m_unsetOnRelease)
            {
                SetClockSelect(-1);
            }
            
            *m_maxHeld = 0;
        }

        m_unsetOnRelease = false;
    }

    virtual SmartGrid::Color GetColor() override
    {
        if (ClockSelect() == m_myClock)
        {
            return m_clockColor;
        }
        else if (ResetSelect() == m_myClock)
        {
            return m_resetColor;
        }
        else
        {
            return m_offColor;
        }
    }
};

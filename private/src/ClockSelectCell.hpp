#include "PercentileSequencer.hpp"
#include "IndexArp.hpp"

struct ClockSelectCell : public SmartGrid::Cell
{
    virtual ~ClockSelectCell()
    {
    }

    int* m_clockSelect;
    int* m_resetSelect;
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
        NonagonIndexArp::Input* state,
        size_t trio,
        size_t* numHeld,
        size_t* maxHeld,
        int myClock,
        SmartGrid::Color offColor,
        SmartGrid::Color clockColor,
        SmartGrid::Color resetColor)
        : m_clockSelect(state->m_clockSelect)
        , m_resetSelect(state->m_resetSelect)
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

    virtual void OnPress(uint8_t) override
    {
        ++(*m_numHeld);
        
        if (*m_numHeld == 1)
        {
            if (m_clockSelect[m_trio] == m_myClock)
            {
                m_unsetOnRelease = true;
            }
            else
            {
                m_clockSelect[m_trio] = m_myClock;
            }
            
            m_resetSelect[m_trio] = -1;
        }
        else if (*m_numHeld == 2)
        {
            m_resetSelect[m_trio] = m_myClock;
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
                m_clockSelect[m_trio] = -1;
            }
            
            *m_maxHeld = 0;
        }

        m_unsetOnRelease = false;
    }

    virtual SmartGrid::Color GetColor() override
    {
        if (m_clockSelect[m_trio] == m_myClock)
        {
            return m_clockColor;
        }
        else if (m_resetSelect[m_trio] == m_myClock)
        {
            return m_resetColor;
        }
        else
        {
            return m_offColor;
        }
    }
};

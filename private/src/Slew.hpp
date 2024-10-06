

struct Slew
{
    float m_val;

    Slew() : m_val(0)
    {
    }

    Slew(float val) : m_val(val)
    {
    }

    float SlewDown(float dt, float time, float trg)
    {
        if (m_val <= trg || time < dt)
        {
            m_val = trg;
        }
        else
        {
            m_val = std::max<float>(m_val - dt / time, trg);
        }

        return m_val;
    }             
};

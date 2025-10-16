#pragma once
#include "plugin.hpp"
#include <cstddef>
#include <cmath>
#include "Trig.hpp"
#include "NormGen.hpp"
#include "Tick2Phasor.hpp"
#include <iomanip>
#include <sstream>
#include <numeric>

inline uint64_t gcd(uint64_t a, uint64_t b)
{
   if (b == 0)
   return a;
   return gcd(b, a % b);
}

struct FixedPointNumber
{
    static constexpr uint64_t x_base = 72055526400000000;
    uint64_t m_val;

    FixedPointNumber() : m_val(0)
    {
    }

    explicit constexpr FixedPointNumber(uint64_t num, uint64_t den)
        : m_val(num * x_base / den)
    {
    }

    explicit constexpr FixedPointNumber(uint64_t x)
        : m_val(x)
    {
    }

    static FixedPointNumber FromInt(uint64_t x)
    {
        return FixedPointNumber(x);
    }

    static FixedPointNumber FromDouble(double x)
    {
        return FixedPointNumber(x * x_base);
    }
    
    float Float()
    {
        return static_cast<double>(m_val) / x_base;
    }

    FixedPointNumber operator*(uint64_t x)
    {
        unsigned __int128 val = m_val;
        return FromInt(val * x);
    }

    // Fixed operator/(uint64_t x)
    // {
    //     return Fixed(m_val / x);
    // }

    FixedPointNumber operator+(FixedPointNumber x)
    {
        unsigned __int128 val = m_val;
        return FromInt(val + x.m_val);
    }

    FixedPointNumber Negate()
    {
        return FixedPointNumber(x_base - m_val);
    }

    FixedPointNumber operator-(FixedPointNumber x)
    {
        assert(x <= (*this));
        FixedPointNumber result = FromInt(m_val - x.m_val);
        return result;
    }
    
    bool operator==(FixedPointNumber x)
    {
        return m_val == x.m_val;
    }

    bool operator!=(FixedPointNumber x)
    {
        return m_val != x.m_val;
    }

    bool operator<(FixedPointNumber x)
    {
        return m_val < x.m_val;
    }

    bool operator<=(FixedPointNumber x)
    {
        return m_val <= x.m_val;
    }

    bool operator>=(FixedPointNumber x)
    {
        return m_val >= x.m_val;
    }

    bool operator>(FixedPointNumber x)
    {
        return m_val > x.m_val;
    }

    FixedPointNumber Min(FixedPointNumber x)
    {
        return (*this) <= x ? (*this) : x;
    }

    FixedPointNumber Max(FixedPointNumber x)
    {
        return (*this) >= x ? (*this) : x;
    }

    FixedPointNumber TimesRat(FixedPointNumber num, FixedPointNumber den)
    {
        unsigned __int128 val = m_val;
        val = val * num.m_val;
        val = val / den.m_val;
        FixedPointNumber result = FromInt(val);
        return result;
    }

    FixedPointNumber Round(uint64_t x)
    {
        return FixedPointNumber((m_val / x) * x);
    }

    FixedPointNumber Reduce(uint64_t* integer)
    {
        *integer = m_val / x_base;
        return FixedPointNumber(m_val % x_base);
    }

    FixedPointNumber Reduce()
    {
        uint64_t i;
        return Reduce(&i);
    }

    static constexpr FixedPointNumber One()
    {
        return FixedPointNumber(x_base);
    }

    static constexpr FixedPointNumber Half()
    {
        return FixedPointNumber(1, 2);
    }

    bool Gate()
    {
        return (*this) < Half();
    }

    std::string Frac()
    {
        uint64_t g = gcd(x_base, m_val);
        return std::to_string(m_val) + " (" + std::to_string(m_val / g) + "/" + std::to_string(x_base / g) + ")";
    }

    std::string Str()
    {
        return std::to_string(Float());
    }
};

inline FixedPointNumber Interpolate(FixedPointNumber x1, FixedPointNumber x2, FixedPointNumber y1, FixedPointNumber y2, FixedPointNumber xp)
{
    if (x1.Float() > 1 ||
        x2.Float() > 1 ||
        y1.Float() > 1 ||
        y2.Float() > 1 ||
        xp.Float() > 1)
    {
        INFO("Interp %f %f %f %f %f",
             x1.Float() ,
             x2.Float() ,
              y1.Float() ,
             y2.Float() ,
             xp.Float());
        assert(false);
    }

    if (y2 < y1)
    {
        return Interpolate(x2, x1, y2, y1, xp);
    }
    
    if ((x2 >= x1) != (xp >= x1) && xp != x1)
    {
        INFO("Interp %f %f %f %f %f",
             x1.Float() ,
             x2.Float() ,
              y1.Float() ,
             y2.Float() ,
             xp.Float());
        INFO("Interp %s %s %s %s %s",
             x1.Frac().c_str() ,
             x2.Frac().c_str() ,
             y1.Frac().c_str() ,
             y2.Frac().c_str() ,
             xp.Frac().c_str());
        assert(false);
    }
    
    FixedPointNumber ratNum = x2 >= x1 ? xp - x1 : x1 - xp;
    FixedPointNumber ratDen = x2 >= x1 ? x2 - x1 : x1 - x2;
    
    FixedPointNumber result = y1 + (y2 - y1).TimesRat(ratNum, ratDen);

    if (FixedPointNumber::One() < result)
    {
        INFO("Interp %s %s %s %s %s -> %s",
             x1.Frac().c_str() ,
             x2.Frac().c_str() ,
             y1.Frac().c_str() ,
             y2.Frac().c_str() ,
             xp.Frac().c_str(),
             result.Frac().c_str());
        assert(false);
    }
        
    return result;
}

inline FixedPointNumber CircleDist(FixedPointNumber x1, FixedPointNumber x2)
{
    FixedPointNumber diff = x1 < x2 ? x2 - x1 : x1 - x2;
    return FixedPointNumber(std::min(diff.m_val, FixedPointNumber::x_base - diff.m_val));
}

struct MusicalTime;

struct LinearPeice
{
    enum class BoundState : int
    {
        Under,
        In,
        Over
    };

    FixedPointNumber m_x1;
    FixedPointNumber m_x2;
    FixedPointNumber m_y1;
    FixedPointNumber m_y2;
    bool m_empty;

    LinearPeice()
        : LinearPeice(FixedPointNumber(0), FixedPointNumber(0), FixedPointNumber(0), FixedPointNumber(0))
    {   
        m_empty = true;
    }
    
    LinearPeice(FixedPointNumber x1, FixedPointNumber x2, FixedPointNumber y1, FixedPointNumber y2)
        : m_x1(x1)
        , m_x2(x2)
        , m_y1(y1)
        , m_y2(y2)
        , m_empty(false)
    {
    }

    LinearPeice SetOver(FixedPointNumber x2, FixedPointNumber y2)
    {
        return LinearPeice(m_x2, x2, m_y2, y2);
    }

    LinearPeice SetUnder(FixedPointNumber x1, FixedPointNumber y1)
    {
        return LinearPeice(x1, m_x1, y1, m_y1);
    }

    static LinearPeice Empty()
    {
        return LinearPeice();
    }

    LinearPeice Compose(LinearPeice other)
    {
        FixedPointNumber y1 = other.m_y1 <= other.m_y2 ? m_x1.Max(other.m_y1) : m_x2.Max(other.m_y2);
        FixedPointNumber y2 = other.m_y1 <= other.m_y2 ? m_x2.Min(other.m_y2) : m_x1.Min(other.m_y1);
        if (y1 < other.m_y1.Min(other.m_y2) ||
            y2 < other.m_y1.Min(other.m_y2) ||
            y1 > other.m_y1.Max(other.m_y2) ||
            y2 > other.m_y1.Max(other.m_y2))
        {
            INFO("COMPOSE %f %s, %f %s <- %s o %s", y1.Float(), y1.Frac().c_str(), y2.Float(), y2.Frac().c_str(), ToString().c_str(), other.ToString().c_str());
            INFO("C %d %d %d %d",
                 y1 < other.m_y1.Min(other.m_y2) ,
                 y2 < other.m_y1.Min(other.m_y2) ,
                 y1 > other.m_y1.Max(other.m_y2) ,
                 y2 > other.m_y1.Max(other.m_y2));
            assert(false);
        }
        
        FixedPointNumber x1 = other.Invert(y1);
        FixedPointNumber x2 = other.Invert(y2);
        if (x2 < x1 ||
            x1 < other.m_x1 ||
            other.m_x2 < x1 ||
            x2 < other.m_x1 ||
            other.m_x2 < x2)
        {
            INFO("COMPOSE %f -> %f, %f -> %f %s o %s", y1.Float(), x1.Float(), y2.Float(), x2.Float(),ToString().c_str(), other.ToString().c_str());
            INFO("C %d %d %d %d %d",
                 x2 < x1 ,
                 x1 < other.m_x1 ,
                 other.m_x2 < x1 ,
                 x2 < other.m_x1 ,
                 other.m_x2 < x2);
            assert(false);
        }

        FixedPointNumber z1 = other.Interpolate(x1);
        FixedPointNumber z2 = other.Interpolate(x2);
        
        if (z1 < m_x1 ||
            m_x2 < z1 ||
            z2 < m_x1 ||
            m_x2 < z2 ||
            y1 != z1 ||
            y2 != z2)
        {
            INFO("COMPOSE %f -> %f -> %f, %f -> %f -> %f %s o %s", y1.Float(), x1.Float(), z1.Float(), y2.Float(), x2.Float(), z2.Float(), ToString().c_str(), other.ToString().c_str());
            INFO("C %d %d %d %d %d %d",
                 z1 < m_x1 ,
                 m_x2 < z1 ,
                 z2 < m_x1 ,
                 m_x2 < z2,
                 y1 != z1,
                 y2 != z2);
            assert(false);
        }
        
        return LinearPeice(x1, x2, Interpolate(y1), Interpolate(y2));
    }

    BoundState Check(FixedPointNumber x)
    {
        if (m_empty)
        {
            return BoundState::Under;
        }
        else if (m_x1 <= x && x < m_x2)
        {
            return BoundState::In;
        }
        else if (CircleDist(m_x1, x) < CircleDist(m_x2, x))
        {
            return BoundState::Under;
        }
        else
        {
            return BoundState::Over;
        }
    }

    FixedPointNumber Interpolate(FixedPointNumber x)
    {
        if (x == FixedPointNumber(0) && m_x1 != FixedPointNumber(0) && m_x2 == FixedPointNumber::One())
        {
            return m_y2;
        }
        else if (x == FixedPointNumber::One() && m_x2 != FixedPointNumber::One() && m_x1 == FixedPointNumber(0))
        {
            return m_y1;
        }
        
        return ::Interpolate(m_x1, m_x2, m_y1, m_y2, x);
    }

    FixedPointNumber Invert(FixedPointNumber y)
    {
        if (y == m_y1)
        {
            return m_x1;
        }
        else if (y == m_y2)
        {
            return m_x2;
        }
        else if (m_y1 <= m_y2)
        {
            return LinearPeice(m_y1, m_y2, m_x1, m_x2).Interpolate(y);
        }
        else
        {
            return LinearPeice(m_y2, m_y1, m_x2, m_x1).Interpolate(y);
        }
    }

    std::string ToString()
    {
        return "<" + m_x1.Str() + ", " +
            m_x2.Str() + ", " +
            m_y1.Str() + ", " +
            m_y2.Str() + ">";
    }
};

struct TimeBit
{
    enum class State : int
    {
        x_init,
        x_waiting,
        x_running
    };
    
    LinearPeice m_lp;
    MusicalTime* m_owner;
    size_t m_ix;
    int m_parentIx;
    FixedPointNumber m_swing;
    FixedPointNumber m_swagger;
    FixedPointNumber m_pos;
    uint64_t m_parentFloor;
    size_t m_mult;
    bool m_top;
    State m_state;
    bool m_pingPong;
    size_t m_topLevelWinding;

    void Init(size_t ix, MusicalTime* owner)
    {
        m_ix = ix;        
        m_owner = owner;
        m_parentIx = ix - 1;
        m_pos = FixedPointNumber(0);
        m_parentFloor = 0;
        m_top = true;
        m_state = State::x_init;
        m_swing = FixedPointNumber::Half();
        m_swagger = FixedPointNumber::Half();
        m_mult = 1;
        m_pingPong = false;
        m_topLevelWinding = 0;
    }

    void Stop()
    {
        m_pos = FixedPointNumber(0);
        m_parentFloor = 0;
        m_top = true;
        m_state = State::x_init;
        m_lp = LinearPeice::Empty();
        m_topLevelWinding = 0;
    }

    int MonodromyNumber(int resetIx)
    {
        if (static_cast<int>(m_ix) == resetIx)
        {
            return 0;
        }
        else if (m_pingPong)
        {
            bool thisGate = m_pos < FixedPointNumber::Half();
            return thisGate ? 0 : 1;
        }
        else if (m_parentIx < 0)
        {
            bool thisGate = m_pos < FixedPointNumber::Half();
            return 2 * m_topLevelWinding + (thisGate ? 0 : 1);
        }
        else
        {
            uint64_t preResult;
            bool thisGate = m_pos < FixedPointNumber::Half();
            if (m_mult % 2 == 0)
            {
                preResult = m_parentFloor % (m_mult / 2);
            }
            else
            {
                if (GetParent()->m_pos <= FixedPointNumber::Half())
                {
                    preResult = m_parentFloor;
                }
                else
                {
                    ((GetParent()->m_pos - FixedPointNumber::Half()) * m_mult).Reduce(&preResult);
                    thisGate = !thisGate;
                }
            }   

            return 2 * preResult + (thisGate ? 0 : 1) + m_mult * GetParent()->MonodromyNumber(resetIx);
        }
    }
    
    TimeBit* GetParent();

    // std::string Rep()
    // {
    //     std::stringstream stream;
    //     stream << std::fixed << std::setprecision(10) << m_pos;
    //     std::string s = stream.str();
    //     return "(" + std::to_string(m_ix) + ", " + std::to_string(m_pos < 0.5) + ", " + s + ", " + std::to_string(m_mult) + ")";
    // }

    LinearPeice MakeLP(FixedPointNumber  parentPos, uint64_t* parentFloor)
    {
        (parentPos * m_mult).Reduce(parentFloor);
        assert(*parentFloor < m_mult);
        LinearPeice result = LinearPeice(
            FixedPointNumber(*parentFloor, m_mult), 
            FixedPointNumber(*parentFloor + 1, m_mult), 
            FixedPointNumber(0),
            FixedPointNumber::One());
        //INFO("Make LP %lu %s (%llu)", m_ix, result.ToString().c_str(), *parentFloor);
        if (m_pingPong)
        {
            //INFO("PP %f -> %f", parentPos.Float(), result.Interpolate(parentPos).Float());
            result = MakePingPongLP(result.Interpolate(parentPos)).Compose(result);
            //INFO("PP %s", result.ToString().c_str());
        }

        //INFO("Swing %f -> %f", parentPos.Float(), result.Interpolate(parentPos).Float());
        if (m_mult % 2 == 0 || *parentFloor != m_mult / 2)
        {
            result = MakeSwingLP(result.Interpolate(parentPos)).Compose(result);
        }
        //INFO("Swung %s", result.ToString().c_str());
        return result;
    }
    
    LinearPeice MakePingPongLP(FixedPointNumber t)
    {
        if (!m_pingPong)
        {   
            return LinearPeice(FixedPointNumber(0), FixedPointNumber::One(), FixedPointNumber(0), FixedPointNumber::One());
        }
        else if (t < FixedPointNumber::Half())
        {
            return LinearPeice(FixedPointNumber(0), FixedPointNumber::Half(), FixedPointNumber(0), FixedPointNumber::One());
        }
        else
        {
            return LinearPeice(FixedPointNumber::Half(), FixedPointNumber::One(), FixedPointNumber::One(), FixedPointNumber(0));
        }
    }
    
    LinearPeice MakeSwingLP(FixedPointNumber t)
    {
        if (t < m_swing)
        {
            return LinearPeice(FixedPointNumber(0), m_swing, FixedPointNumber(0), m_swagger);
        }
        else
        {
            return LinearPeice(m_swing, FixedPointNumber::One(), m_swagger, FixedPointNumber::One());
        }
    }

    struct Input
    {
        int m_parentIx;
        float m_swing;
        float m_swagger;
        size_t m_mult;
        bool m_pingPong;
        float* m_rand;
        RGen m_gen;
        float* m_globalHomotopy;

        Input()
            : m_parentIx(0)
            , m_swing(0.0)
            , m_swagger(0.0)
            , m_mult(2)
            , m_pingPong(false)
            , m_rand(nullptr)
            , m_globalHomotopy(nullptr)
        {
        }
    };

    void Read(Input& input)
    {
        if (m_state == State::x_init)
        {
            m_state = State::x_running;
        }
        else if (m_parentIx != input.m_parentIx)
        {
            m_top = true;
            m_pos = FixedPointNumber(0);
            m_state = State::x_waiting;
        }

        m_parentIx = input.m_parentIx;
        if (GetParent()->m_top)
        {
            m_state = State::x_running;            
        }
        
        m_mult = input.m_mult;
        m_pingPong = input.m_pingPong;
        ReadSwing(input);
    }

    void ReadSwing(Input& input)
    {
        float rand = input.m_rand ? *input.m_rand : 0;
        float globalHomo = input.m_globalHomotopy ? *input.m_globalHomotopy : 1;
        float swingFloat = globalHomo * ((1 - rand) * input.m_swing + rand * input.m_swing * input.m_gen.UniGen());
        float swaggerFloat = globalHomo * ((1 - rand) * input.m_swagger + rand * input.m_swagger * input.m_gen.UniGen());
        m_swing = FixedPointNumber::FromDouble(swingFloat / 2.5 + 0.5);
        m_swagger = FixedPointNumber::FromDouble(swaggerFloat / 2.5 + 0.5);

        size_t effectiveMult = m_pingPong ? 2 * m_mult : m_mult;
        m_swing = m_swing.Round(effectiveMult);
    }

    void Process(Input& input)
    {
        if (m_state == State::x_init)
        {
            Read(input);
        }
        
        if (m_state == State::x_waiting)
        {
            Read(input);
            if (m_state == State::x_waiting)
            {
                return;
            }
        }
        else
        {
            TimeBit* parent = GetParent();
            if (parent->m_top)
            {
                Read(input);
                if (m_state == State::x_waiting)
                {
                    return;
                }
            }

            m_top = parent->m_top;
        }

        TimeBit* parent = GetParent();
        FixedPointNumber inT = parent->m_pos;
        if (parent->m_top || m_lp.Check(inT) != LinearPeice::BoundState::In)
        {
            uint64_t newParentFloor;
            m_lp = MakeLP(inT, &newParentFloor);
            if (newParentFloor != m_parentFloor)
            {
                m_top = true;
                m_parentFloor = newParentFloor;
            }
        }

        m_pos = m_lp.Interpolate(inT);
        if (FixedPointNumber::One() < m_pos)
        {
            INFO("Big pos %f, %lu %f", m_pos.Float(), m_ix, inT.Float());
            assert(false);
        }

        if (m_pos == FixedPointNumber::One())
        {
            m_pos = FixedPointNumber(0);
        }

        if (m_top)
        {
            m_topLevelWinding = m_pingPong ? 0 : (m_mult * parent->m_topLevelWinding);
        }
    }

    void SetDirectly(float t)
    {
        m_top = false;
        if (std::abs(t - m_pos.Float()) > 0.5)
        {
            m_top = true;
            ++m_topLevelWinding;
        }

        m_pos = FixedPointNumber::FromDouble(t);
    }
};

struct MusicalTime
{
    static constexpr size_t x_numBits = 8;
    TimeBit m_bits[x_numBits];
    bool m_gate[x_numBits];
    bool m_change[x_numBits];
    bool m_anyChange;
    bool m_running;

    MusicalTime()
    {
        m_anyChange = false;
        m_running = false;
        for (size_t i = 0; i < x_numBits; ++i)
        {
            m_bits[i].Init(i, this);
            m_change[i] = false;
        }
    }

    // std::string Rep()
    // {
    //     std::string result = "<";
    //     for (size_t i = 0; i < x_numBits; ++i)
    //     {
    //         result += m_bits[i].Rep() + " ";
    //     }

    //     return result + ">";
    // }
    
    struct Input
    {
        float m_t;
        TimeBit::Input m_input[x_numBits];
        float m_rand;
        float m_globalHomotopy;
        bool m_running;

        Input()
        {
            m_t = 0;
            m_rand = 0;
            m_globalHomotopy = 1;
            for (size_t i = 0; i < x_numBits; ++i)
            {
                if (i > 0)
                {
                    m_input[i].m_parentIx = i - 1;
                }

                m_input[i].m_rand = &m_rand;
                m_input[i].m_globalHomotopy = &m_globalHomotopy;
            }
        }
    };

    void Process(Input& in)
    {
        if (in.m_running)
        {
            m_running = true;
            m_bits[0].SetDirectly(in.m_t);
            for (size_t i = 1; i < x_numBits; ++i)
            {
                m_bits[i].Process(in.m_input[i]);
            }
            
            m_anyChange = false;
            
            for (size_t i = 0; i < x_numBits; ++i)
            {
                m_change[i] = false;
                bool gate = GetPos(i) < 0.5;
                if (m_gate[i] != gate)
                {
                    m_change[i] = true;
                    m_anyChange = true;
                }
                
                m_gate[i] = gate;
            }
        }
        else if (m_running)
        {
            // It was running, but a stop was requested.
            // Stop All bits, and set 'm_anyChange' so others can react.
            //
            m_running = false;
            m_anyChange = true;
            for (size_t i = 0; i < x_numBits; ++i)
            {
                m_bits[i].Stop();
                m_gate[i] = false;
                m_change[i] = true;
            }            
        }
        else
        {
            m_anyChange = false;
        }
    }

    float GetPos(size_t ix)
    {
        return m_bits[ix].m_pos.Float();
    }

    bool GetGate(size_t ix)
    {
        return m_gate[ix];
    }

    int MonodromyNumber(int clockIx, int resetIx)
    {
        return m_bits[clockIx].MonodromyNumber(resetIx);
    }
};

inline TimeBit* TimeBit::GetParent()
{
    if (m_parentIx < 0)
    {
        return nullptr;
    }

    return &m_owner->m_bits[m_parentIx];
}

struct MusicalTimeWithClock
{
    MusicalTime m_musicalTime;
    float m_phasor;
    std::string m_lastChange;
    float m_lastChangeTime;
    Tick2Phasor m_tick2Phasor;  

    enum class ClockMode : int 
    {
        Internal,
        External,
        Tick2Phasor
    };

    struct Input
    {
        Input() 
          : m_clockMode(ClockMode::Internal)
          , m_freq(1.0/4)
          , m_timeIn(0)
        {
        }
        
        ClockMode m_clockMode;
        MusicalTime::Input m_input;
        float m_freq;        
        float m_timeIn;
        Tick2Phasor::Input m_tick2PhasorInput;
    };

    MusicalTimeWithClock()
        : m_phasor(0)
        , m_lastChangeTime(0)
    {
    }

    int MonodromyNumber(int clockIx, int resetIx)
    {
        return m_musicalTime.MonodromyNumber(clockIx, resetIx);
    }

    void Process(Input& input)
    {
        m_lastChangeTime += 1.0 / 48000.0;
        if (input.m_input.m_running)
        {
            if (input.m_clockMode == ClockMode::Internal)
            {
                m_phasor += input.m_freq;
                m_phasor = m_phasor - floor(m_phasor);
            }
            else if (input.m_clockMode == ClockMode::External)
            {
                m_phasor = input.m_timeIn;
            }
        }
        else
        {
            m_phasor = 0;
            m_tick2Phasor.m_reset = true;
        }
        
        if (input.m_clockMode == ClockMode::Tick2Phasor)
        {
            m_tick2Phasor.Process(input.m_tick2PhasorInput);
            m_phasor = m_tick2Phasor.m_output;
        }

        input.m_input.m_t = m_phasor;
        m_musicalTime.Process(input.m_input);

        if (m_musicalTime.m_anyChange)
        {
            if (m_lastChangeTime < 0.001)
            {
                INFO("Change time %f", m_lastChangeTime);
                // INFO("BEFORE: %s", m_lastChange.c_str());
                //                INFO("AFTER:  %s", m_musicalTime.Rep().c_str());
            }

            m_lastChangeTime = 0;
            // m_lastChange = m_musicalTime.Rep();
        }
    }
};


#pragma once

#include <cassert>
#include <cmath>

// Continuous master-cycle coordinate. Transport rebases its musical origin;
// tempo changes only subsequent travel. See formal/PhaseTime/Clock.lean.
//
struct PhaseClock
{
    double m_phase = 0.0;
    double m_musicalPhase = 0.0;
    double m_offset = 0.0;
    double m_increment = 0.0;
    bool m_running = false;

    void Advance(double musicalPhase)
    {
        m_phase = musicalPhase + m_offset;
        m_musicalPhase = musicalPhase;
        m_running = true;
    }

    void Resume(double musicalPhase)
    {
        m_offset = m_phase - musicalPhase;
        m_musicalPhase = musicalPhase;
        m_running = true;
    }

    void FreeRun(double increment)
    {
        assert(increment > 0.0);
        m_phase += increment;
        m_musicalPhase = 0.0;
        m_offset = m_phase;
        m_running = false;
    }

    void Update(double musicalPhase, bool running, double nominalIncrement)
    {
        assert(std::isfinite(musicalPhase) && std::isfinite(nominalIncrement));
        double previous = m_phase;
        if (!running)
        {
            FreeRun(nominalIncrement);
        }
        else if (!m_running)
        {
            Resume(musicalPhase);
        }
        else
        {
            Advance(musicalPhase);
        }

        m_increment = m_phase - previous;
    }
};

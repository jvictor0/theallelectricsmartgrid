#pragma once

// Delay tape positions. Update runs every engine sample.
//
struct TapeHead
{
    double m_relativePosition;
    double m_actualPosition;

    TapeHead()
        : m_relativePosition(0.0)
        , m_actualPosition(0.0)
    {
    }
};

// Positions and periods in engine samples.
//
struct WriteTapeHead : TapeHead
{
    struct Input
    {
        // Modulated global phase of the theory of time.
        //
        double m_theoryPhase;
        bool m_running;

        // Global loop period in engine samples.
        //
        double m_globalPeriodSamples;

        Input()
            : m_theoryPhase(0.0)
            , m_running(false)
            , m_globalPeriodSamples(1.0)
        {
        }
    };

    double m_glue;
    double m_globalPeriodSamples;
    double m_displayPhaseOffset;
    bool m_running;

    WriteTapeHead()
        : m_glue(0.0)
        , m_globalPeriodSamples(1.0)
        , m_displayPhaseOffset(0.0)
        , m_running(false)
    {
    }

    void Update(const Input& input)
    {
        double theoryPhase = input.m_theoryPhase;
        double theoryPosition = theoryPhase * input.m_globalPeriodSamples;

        bool runningChanged = input.m_running != m_running;
        bool globalPeriodSamplesChanged = input.m_globalPeriodSamples != m_globalPeriodSamples;
        if (runningChanged || globalPeriodSamplesChanged)
        {
            m_glue = m_actualPosition - theoryPosition;
        }

        if (runningChanged && input.m_running)
        {
            // Playback rejoins musical phase on start. Keep the display origin
            // continuous so the waveform recorder does not interpolate a reset.
            //
            m_displayPhaseOffset += m_relativePosition - theoryPhase;
        }

        m_running = input.m_running;
        m_globalPeriodSamples = input.m_globalPeriodSamples;

        if (!m_running)
        {
            m_actualPosition += 1.0;
            if (m_globalPeriodSamples > 0.0)
            {
                m_relativePosition += 1.0 / m_globalPeriodSamples;
            }

            m_glue = m_actualPosition - theoryPosition;
        }
        else
        {
            m_relativePosition = theoryPhase;
            m_actualPosition = theoryPosition + m_glue;
        }
    }

    double GetDisplayPosition() const
    {
        return m_relativePosition + m_displayPhaseOffset;
    }
};

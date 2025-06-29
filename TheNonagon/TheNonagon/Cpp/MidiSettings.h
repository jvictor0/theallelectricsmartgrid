#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MidiSettings
{
    int32_t m_sourceIndex;
    int32_t m_destinationIndex;
    bool m_sendClock;
    bool m_receiveClock;
    bool m_sendTransport;
    bool m_receiveTransport;
    bool m_sendProgramChange;
    bool m_receiveProgramChange;

#ifdef __cplusplus
    MidiSettings()
        : m_sourceIndex(-1)
        , m_destinationIndex(-1)
        , m_sendClock(false)
        , m_receiveClock(false)
        , m_sendTransport(false)
        , m_receiveTransport(false)
        , m_sendProgramChange(false)
        , m_receiveProgramChange(false)
    {
    }
#endif
};

#ifdef __cplusplus
}
#endif 
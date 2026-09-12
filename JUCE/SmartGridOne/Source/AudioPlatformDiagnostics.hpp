#pragma once

struct AudioPlatformState
{
    int m_thermalState = -1;
    bool m_lowPower = false;
};

AudioPlatformState ReadAudioPlatformState();
void LogAudioSessionDiagnostics();

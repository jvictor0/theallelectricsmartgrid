#pragma once

#include "ParamEventLogger.hpp"
#include "SceneManager.hpp"
#include "StreamingRecorder.hpp"

struct SmartGridOneContext
{
    SmartGrid::SceneManager m_sceneManager;
    StreamingRecorder m_recorder;
    ParamEventLogger m_paramEventLogger;

    SmartGridOneContext()
        : m_paramEventLogger(&m_recorder)
    {
    }
};
#pragma once

#include "State.hpp"

struct StateManager
{
    void RecordStateChange(State* state)
    {
        std::ignore = state;

        // Not yet implemented
        //
    }
};

inline void RecordStateChange(StateManager* stateManager, State* state)
{
    stateManager->RecordStateChange(state);
}
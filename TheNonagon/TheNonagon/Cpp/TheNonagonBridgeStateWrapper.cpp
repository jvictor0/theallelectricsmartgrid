#include "TheNonagonBridgeState.h"
#include "TheNonagonBridgeStateWrapper.h"

extern "C" 
{
    void* createTheNonagonBridgeState() 
    {
        return new TheNonagonBridgeState();
    }
    
    void destroyTheNonagonBridgeState(void* instance) 
    {
        delete static_cast<TheNonagonBridgeState*>(instance);
    }

    void handleTheNonagonBridgeStatePress(void* instance, GridHandle gridHandle, int x, int y)
    {
        static_cast<TheNonagonBridgeState*>(instance)->HandlePress(gridHandle, x, y);
    }

    void handleTheNonagonBridgeStateRelease(void* instance, GridHandle gridHandle, int x, int y)
    {
        static_cast<TheNonagonBridgeState*>(instance)->HandleRelease(gridHandle, x, y);
    }

    void getTheNonagonBridgeStateColor(void* instance, GridHandle gridHandle, int x, int y, struct RGBColor* outColor)
    {
        if (outColor)
        {
            *outColor = static_cast<TheNonagonBridgeState*>(instance)->GetColor(gridHandle, x, y);
        }
    }
    
    void handleTheNonagonBridgeStateRightMenuPress(void* instance, int index)
    {
        static_cast<TheNonagonBridgeState*>(instance)->HandleRightMenuPress(index);
    }
    
    void getTheNonagonBridgeStateRightMenuColor(void* instance, int index, struct RGBColor* outColor)
    {
        if (outColor)
        {
            *outColor = static_cast<TheNonagonBridgeState*>(instance)->GetRightMenuColor(index);
        }
    }

    void processTheNonagonBridgeState(void* instance, float** audioBuffer, int32_t numChannels, int32_t numFrames, AudioTimeStamp timestamp)
    {
        static_cast<TheNonagonBridgeState*>(instance)->Process(audioBuffer, numChannels, numFrames, timestamp);
    }
    
    void setTheNonagonBridgeStateMidiInput(void* instance, int32_t index)
    {
        static_cast<TheNonagonBridgeState*>(instance)->SetMidiInput(index);
    }
    
    void setTheNonagonBridgeStateMidiOutput(void* instance, int32_t index)
    {
        static_cast<TheNonagonBridgeState*>(instance)->SetMidiOutput(index);
    }
} 
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

    void handleTheNonagonBridgeStatePress(void* instance, int x, int y)
    {
        static_cast<TheNonagonBridgeState*>(instance)->HandlePress(x, y);
    }

    void handleTheNonagonBridgeStateRelease(void* instance, int x, int y)
    {
        static_cast<TheNonagonBridgeState*>(instance)->HandleRelease(x, y);
    }

    void getTheNonagonBridgeStateColor(void* instance, int x, int y, struct RGBColor* outColor)
    {
        if (outColor)
        {
            *outColor = static_cast<TheNonagonBridgeState*>(instance)->GetColor(x, y);
        }
    }

    void processTheNonagonBridgeState(void* instance)
    {
        static_cast<TheNonagonBridgeState*>(instance)->Process();
    }
} 
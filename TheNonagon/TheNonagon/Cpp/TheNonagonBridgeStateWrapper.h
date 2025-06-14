#pragma once
#include <stdint.h>
#include "RGBColor.h"

#ifdef __cplusplus
extern "C" {
#endif

void* createTheNonagonBridgeState();
void destroyTheNonagonBridgeState(void* instance);
void handleTheNonagonBridgeStatePress(void* instance, int x, int y);
void handleTheNonagonBridgeStateRelease(void* instance, int x, int y);
void getTheNonagonBridgeStateColor(void* instance, int x, int y, struct RGBColor* outColor);
void handleTheNonagonBridgeStateRightMenuPress(void* instance, int index);
void getTheNonagonBridgeStateRightMenuColor(void* instance, int index, struct RGBColor* outColor);
void processTheNonagonBridgeState(void* instance, float** audioBuffer, int32_t numChannels, int32_t numFrames);

#ifdef __cplusplus
}
#endif 
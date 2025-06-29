#pragma once
#include <stdint.h>
#include <CoreAudio/CoreAudioTypes.h>
#include "RGBColor.h"
#include "GridHandle.h"
#include "MidiSettings.h"

#ifdef __cplusplus
extern "C" {
#endif

void* createTheNonagonBridgeState();
void destroyTheNonagonBridgeState(void* instance);
void handleTheNonagonBridgeStatePress(void* instance, GridHandle gridHandle, int x, int y);
void handleTheNonagonBridgeStateRelease(void* instance, GridHandle gridHandle, int x, int y);
void getTheNonagonBridgeStateColor(void* instance, GridHandle gridHandle, int x, int y, struct RGBColor* outColor);
void handleTheNonagonBridgeStateRightMenuPress(void* instance, int index);
void getTheNonagonBridgeStateRightMenuColor(void* instance, int index, struct RGBColor* outColor);
void processTheNonagonBridgeState(void* instance, float** audioBuffer, int32_t numChannels, int32_t numFrames, AudioTimeStamp timestamp);
struct MidiSettings* getTheNonagonBridgeStateMidiSettings(void* instance);

#ifdef __cplusplus
}
#endif 
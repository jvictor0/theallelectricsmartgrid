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
void processTheNonagonBridgeState(void* instance);

#ifdef __cplusplus
}
#endif 
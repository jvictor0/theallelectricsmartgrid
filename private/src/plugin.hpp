#pragma once

#ifndef IOS_BUILD
#include <rack.hpp>
#include <osdialog.h>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelGangedRandomLFO;
extern Model* modelTheNonagon;
extern Model* modelGridJnctLPP3;
extern Model* modelPhasorEnvelope;
extern Model* modelGridCnct;
extern Model* modelSmartBoxCnct;
extern Model* modelFaderBank;
extern Model* modelVectorPhaseShaper;
extern Model* modelDeDeTour;
extern Model* modelMxDJ;
extern Model* modelSaturationBlock;
extern Model* modelColorHelper;
extern Model* modelButtonBank;
extern Model* modelEncoderBank;
extern Model* modelPolyXFader;
extern Model* modelLissajousLFO;
extern Model* modelQuadDelay;
extern Model* modelQuadReverb;
extern Model* modelQuadMixer;
extern Model* modelHarmonicFlipflop;
extern Model* modelMasterSwitch;
extern Model* modelSmartCanvas;
extern Model* modelPolyCC;
extern Model* modelSampleRateDivider;
extern Model* modelSquiggleBoy;
extern Model* modelTheNonagonSquiggleBoyQuadLaunchpadTwister;

#else
// iOS build stubs
struct Plugin {};
struct Model {};
#endif

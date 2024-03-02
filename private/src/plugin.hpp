#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelGangedRandomLFO;
extern Model* modelTheNonagon;
extern Model* modelGridJnctLPP3;
extern Model* modelPhasorEnvelope;
extern Model* modelGridCnct;
extern Model* modelFaderBank;

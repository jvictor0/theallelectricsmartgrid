#include "plugin.hpp"


Plugin* pluginInstance;


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelMultiFunctionKnob);
	p->addModel(modelMatrixThing);
    p->addModel(modelPolyCV_MIDICC);
    p->addModel(modelPercentileSequencer);
    p->addModel(modelMultiPhasorGate);
    p->addModel(modelGangedRandomLFO);
	p->addModel(modelTheoryOfTime);
	p->addModel(modelTheNonagon);
	p->addModel(modelPhasorEnvelope);
	p->addModel(modelGridJnctLPP3);
	p->addModel(modelGridCnct);
	p->addModel(modelFaderBank);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}

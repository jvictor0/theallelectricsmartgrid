#include "plugin.hpp"
#include "GangedRandomLFO.hpp"
#include "TheNonagon.hpp"
#include "PhasorEnvelope.hpp"
#include "GridJnct.hpp"
#include "FaderBank.hpp"

Plugin* pluginInstance;

Model* modelGangedRandomLFO = createModel<GangedRandomLFO, GangedRandomLFOWidget>("GangedRandomLFO");
Model* modelTheNonagon = createModel<TheNonagon, TheNonagonWidget>("TheNonagon");
Model* modelPhasorEnvelope = createModel<PhasorEnvelope, PhasorEnvelopeWidget>("PhasorEnvelope");
Model* modelGridJnctLPP3 = createModel<SmartGrid::GridJnctLPP3, SmartGrid::GridJnctLPP3Widget>("GridJnctLPP3");
Model* modelGridCnct = createModel<SmartGrid::GridCnct, SmartGrid::GridCnctWidget>("GridCnct");
Model* modelFaderBank = createModel<SmartGrid::FaderBank, SmartGrid::FaderBankWidget>("FaderBank");


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
    p->addModel(modelGangedRandomLFO);
	p->addModel(modelTheNonagon);
	p->addModel(modelPhasorEnvelope);
	p->addModel(modelGridJnctLPP3);
	p->addModel(modelGridCnct);
	p->addModel(modelFaderBank);
}

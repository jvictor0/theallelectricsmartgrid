#include "plugin.hpp"
#include "GangedRandomLFO.hpp"
#include "TheNonagon.hpp"
#include "PhasorEnvelope.hpp"
#include "GridJnct.hpp"
#include "FaderBank.hpp"
#include "MxDJ.hpp"
#include "SampleRateDivider.hpp"
#include "ColorHelper.hpp"
#include "ButtonBank.hpp"
#include "MasterSwitch.hpp"
#include "PolyCC.hpp"
#include "SmartCanvas.hpp"

Plugin* pluginInstance;

Model* modelGangedRandomLFO = createModel<GangedRandomLFO, GangedRandomLFOWidget>("GangedRandomLFO");
Model* modelTheNonagon = createModel<TheNonagon, TheNonagonWidget>("TheNonagon");
Model* modelPhasorEnvelope = createModel<PhasorEnvelope, PhasorEnvelopeWidget>("PhasorEnvelope");
Model* modelGridJnctLPP3 = createModel<SmartGrid::GridJnctLPP3, SmartGrid::GridJnctLPP3Widget>("GridJnctLPP3");
Model* modelGridCnct = createModel<SmartGrid::GridCnct, SmartGrid::GridCnctWidget>("GridCnct");
Model* modelFaderBank = createModel<SmartGrid::FaderBank, SmartGrid::FaderBankWidget>("FaderBank");
Model* modelMxDJ = createModel<MxDJ, MxDJWidget>("MxDJ");
Model* modelSampleRateDivider = createModel<SampleRateDivider, SampleRateDividerWidget>("SampleRateDivider");
Model* modelColorHelper = createModel<SmartGrid::ColorHelper, SmartGrid::ColorHelperWidget>("ColorHelper");
Model* modelButtonBank = createModel<SmartGrid::ButtonBank, SmartGrid::ButtonBankWidget>("ButtonBank");
Model* modelMasterSwitch = createModel<MasterSwitch, MasterSwitchWidget>("MasterSwitch");
Model* modelSmartCanvas = createModel<SmartGrid::SmartCanvas, SmartGrid::SmartCanvasWidget>("SmartCanvas");
Model* modelPolyCC = createModel<PolyCC, PolyCCWidget>("PolyCC");


void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
    p->addModel(modelGangedRandomLFO);
	p->addModel(modelTheNonagon);
	p->addModel(modelPhasorEnvelope);
	p->addModel(modelGridJnctLPP3);
	p->addModel(modelGridCnct);
	p->addModel(modelFaderBank);
	p->addModel(modelMxDJ);
    p->addModel(modelSampleRateDivider);
	p->addModel(modelColorHelper);
	p->addModel(modelButtonBank);
    p->addModel(modelMasterSwitch);
	p->addModel(modelPolyCC);
	p->addModel(modelSmartCanvas);
}

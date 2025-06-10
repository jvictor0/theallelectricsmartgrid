#include "plugin.hpp"
#include "GangedRandomLFO.hpp"
#include "PhasorEnvelope.hpp"
#include "PolyXFader.hpp"
#include "Lissajous.hpp"
#include "QuadDelay.hpp"
#include "QuadMixer.hpp"
#include "HarmonicFlipflop.hpp"
#include "GridJnct.hpp"
#include "FaderBank.hpp"
#include "VectorPhaseShaper.hpp"

#ifndef SMART_BOX
#include "MxDJ.hpp"
#include "SaturationBlock.hpp"
#include "PolyCC.hpp"
#include "TheNonagon.hpp"
#else
#include "SmartBox.hpp"
#endif

#include "SampleRateDivider.hpp"
#include "ColorHelper.hpp"
#include "ButtonBank.hpp"
#include "EncoderBank.hpp"
#include "MasterSwitch.hpp"
#include "SmartCanvas.hpp"

Model* modelGangedRandomLFO = createModel<GangedRandomLFO, GangedRandomLFOWidget>("GangedRandomLFO");
Model* modelPhasorEnvelope = createModel<PhasorEnvelope, PhasorEnvelopeWidget>("PhasorEnvelope");
Model* modelGridJnctLPP3 = createModel<SmartGrid::GridJnctLPP3, SmartGrid::GridJnctLPP3Widget>("GridJnctLPP3");
Model* modelFaderBank = createModel<SmartGrid::FaderBank, SmartGrid::FaderBankWidget>("FaderBank");
Model* modelVectorPhaseShaper = createModel<VectorPhaseShaper<1>, VectorPhaseShaperWidget<1>>("VectorPhaseShaperOne");
Model* modelDeDeTour = createModel<VectorPhaseShaper<2>, VectorPhaseShaperWidget<2>>("VectorPhaseShaper");
Model* modelSampleRateDivider = createModel<SampleRateDivider, SampleRateDividerWidget>("SampleRateDivider");
Model* modelColorHelper = createModel<SmartGrid::ColorHelper, SmartGrid::ColorHelperWidget>("ColorHelper");
Model* modelButtonBank = createModel<SmartGrid::ButtonBank, SmartGrid::ButtonBankWidget>("ButtonBank");
Model* modelEncoderBank = createModel<SmartGrid::EncoderBank, SmartGrid::EncoderBankWidget>("EncoderBank");
Model* modelPolyXFader = createModel<PolyXFader, PolyXFaderWidget>("PolyXFader");
Model* modelLissajousLFO = createModel<LissajousLFO, LissajousLFOWidget>("LissajousLFO");
Model* modelQuadDelay = createModel<QuadDelay<false>, QuadDelayWidget<false>>("QuadDelay");
Model* modelQuadReverb = createModel<QuadDelay<true>, QuadDelayWidget<true>>("QuadReverb");
Model* modelQuadMixer = createModel<QuadMixer, QuadMixerWidget>("QuadMixer");
Model* modelHarmonicFlipflop = createModel<HarmonicFlipflop, HarmonicFlipflopWidget>("HarmonicFlipflop");
Model* modelMasterSwitch = createModel<MasterSwitch, MasterSwitchWidget>("MasterSwitch");
Model* modelSmartCanvas = createModel<SmartGrid::SmartCanvas, SmartGrid::SmartCanvasWidget>("SmartCanvas");
Model* modelSaturationBlock = createModel<SaturationBlock, SaturationBlockWidget>("SaturationBlock");

#ifdef SMART_BOX
Model* modelSmartBoxCnct = createModel<SmartGrid::SmartBoxCnct, SmartGrid::SmartBoxCnctWidget>("SmartBoxCnct");
#endif

#ifndef SMART_BOX
Model* modelTheNonagon = createModel<TheNonagon, TheNonagonWidget>("TheNonagon");
Model* modelMxDJ = createModel<MxDJ, MxDJWidget>("MxDJ");
Model* modelPolyCC = createModel<PolyCC, PolyCCWidget>("PolyCC");
Model* modelGridCnct = createModel<SmartGrid::GridCnct, SmartGrid::GridCnctWidget>("GridCnct");

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
    p->addModel(modelGangedRandomLFO);
	p->addModel(modelTheNonagon);
	p->addModel(modelPhasorEnvelope);
	p->addModel(modelGridJnctLPP3);
	p->addModel(modelGridCnct);
	p->addModel(modelFaderBank);
	p->addModel(modelVectorPhaseShaper);
	p->addModel(modelDeDeTour);
	p->addModel(modelMxDJ);
	p->addModel(modelSaturationBlock);
    p->addModel(modelSampleRateDivider);
	p->addModel(modelColorHelper);
	p->addModel(modelButtonBank);
	p->addModel(modelPolyXFader);
	p->addModel(modelLissajousLFO);
	p->addModel(modelQuadDelay);
	p->addModel(modelQuadReverb);
	p->addModel(modelQuadMixer);
	p->addModel(modelHarmonicFlipflop);
	p->addModel(modelEncoderBank);
    p->addModel(modelMasterSwitch);
	p->addModel(modelPolyCC);
	p->addModel(modelSmartCanvas);
}
#endif

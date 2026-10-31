// F(ParamName, VisualizerName)
//
// Each parameter from ForEachSmartGridOneParam.hpp appears exactly once,
// including parameters shared by multiple pages. Visualizer names come from
// ForEachSmartGridOneVisualizer.hpp or the named G entries in
// ForEachModulationVisualizer.hpp. Define F before including this file.
//

// Dual VCO: oscillator-specific shaping, then the combined waveform.
//
F(Harmonics1, VCO1Scope)
F(Harmonics2, VCO2Scope)
F(VPSV1, VCO1Scope)
F(VPSV2, VCO2Scope)
F(VPSD1, VCO1Scope)
F(VPSD2, VCO2Scope)
F(PhaseMod1, VCO1Scope)
F(PhaseMod2, VCO2Scope)
F(OscillatorMix, PostFilterScope)
F(BitReduction, PostFilterScope)
F(PitchOffset, PostFilterScope)
F(OscillatorDetune, PostFilterScope)

// Physical modeling: resonator response and sample-rate-reduced spectrum.
//
F(PMMainSVFCutoff, PhysModFreqResponse)
F(PMMainSVFResonance, PhysModFreqResponse)
F(PMMainSVFMorph, PhysModFreqResponse)
F(PMSampleRateReduction, SourceAnalyzer)
F(PMCombFeedback, PhysModFreqResponse)
F(PMCombDampenCutoff, PhysModFreqResponse)

// The excitation envelope has no dedicated visualizer; show its audible result.
//
F(PMAHDAttack, PostFilterScope)
F(PMAHDHold, PostFilterScope)
F(PMAHDDecay, PostFilterScope)
F(PMAmplitude, PostFilterScope)

// Sample playback.
//
F(SampleLoopIndex, SampleTrioWaveform)
F(SampleStart, SampleTrioWaveform)
F(SampleReadSpeed, SampleTrioWaveform)
F(SampleLength, SampleTrioWaveform)
F(SampleBankPosition, SampleTrioWaveform)

// Shape: filtering and distortion spectrum, then the two envelopes.
//
F(HPCutoff, FilterAnalyzer)
F(HPResonance, FilterAnalyzer)
F(LPCutoff, FilterAnalyzer)
F(LPResonance, FilterAnalyzer)
F(FilterDrive, FilterAnalyzer)
F(SampleRateReduction, FilterAnalyzer)
F(ModAHDAttack, ShapeModulationEnvelopeScope)
F(ModAHDHold, ShapeModulationEnvelopeScope)
F(ModAHDDecay, ShapeModulationEnvelopeScope)
F(ModAmplitude, ShapeModulationEnvelopeScope)
F(AmpAHDAttack, ShapeAmpEnvelopeScope)
F(AmpAHDHold, ShapeAmpEnvelopeScope)
F(AmpAHDDecay, ShapeAmpEnvelopeScope)
F(AmpAmplitude, ShapeAmpEnvelopeScope)

// FX matrix: shared sends, returns, feedback, and decay keep this association
// on every page where the same parameter appears.
//
F(AmpGain, FXMatrixQuadAnalyzer)
F(DelaySend, FXMatrixQuadAnalyzer)
F(ReverbSend, FXMatrixQuadAnalyzer)
F(PartialMachineSend, FXMatrixQuadAnalyzer)
F(DelayReturn, FXMatrixQuadAnalyzer)
F(DelayFeedback, FXMatrixQuadAnalyzer)
F(DelayReverbSend, FXMatrixQuadAnalyzer)
F(DelayPartialMachineSend, FXMatrixQuadAnalyzer)
F(ReverbReturn, FXMatrixQuadAnalyzer)
F(ReverbDelaySend, FXMatrixQuadAnalyzer)
F(ReverbFeedback, FXMatrixQuadAnalyzer)
F(ReverbPartialMachineSend, FXMatrixQuadAnalyzer)
F(PartialMachineVolume, FXMatrixQuadAnalyzer)
F(PartialMachineDelaySend, FXMatrixQuadAnalyzer)
F(PartialMachineReverbSend, FXMatrixQuadAnalyzer)
F(PartialMachineDecay, FXMatrixQuadAnalyzer)

// Sub.
//
F(SubOscillator, SubAnalyzer)
F(SubTanhGain, SubAnalyzer)
F(SubSlewRise, SubAnalyzer)
F(SubSlewFall, SubAnalyzer)

// Panning and melody. Portamento follows the pitch display.
//
F(PanRadius, SoundStage)
F(PanPhaseShift, SoundStage)
F(PanMultX, SoundStage)
F(PanMultY, SoundStage)
F(PanCenterX, SoundStage)
F(PanCenterY, SoundStage)
F(Portamento, MelodyRoll)
F(SeqZoneHeight, MelodyRoll)
F(SeqZoneOverlap, MelodyRoll)
F(SeqOffset, MelodyRoll)
F(SeqInterval, MelodyRoll)
F(SeqPageInterval, MelodyRoll)

// Voice LFOs.
//
F(LFO1Skew, LFO1Scope)
F(LFO1Mult, LFO1Scope)
F(LFO1Shape, LFO1Scope)
F(LFO1Amplitude, LFO1Scope)
F(LFO1Center, LFO1Scope)
F(LFO1Slope, LFO1Scope)
F(LFO1PhaseShift, LFO1Scope)
F(LFO1SampleAndHold, LFO1Scope)
F(LFO2Skew, LFO2Scope)
F(LFO2Mult, LFO2Scope)
F(LFO2Shape, LFO2Scope)
F(LFO2Amplitude, LFO2Scope)
F(LFO2Center, LFO2Scope)
F(LFO2Slope, LFO2Scope)
F(LFO2PhaseShift, LFO2Scope)
F(LFO2SampleAndHold, LFO2Scope)

// Delay: buffer timing and envelope, then the resulting spectrum.
//
F(DelayTime, QuadDelayEnvelope)
F(DelayTimeFactor, QuadDelayEnvelope)
F(DelayReadSpeed, QuadDelayEnvelope)
F(DelayDampBase, DelayAnalyzer)
F(DelayDampWidth, DelayAnalyzer)
F(ResynthSlewUp, QuadDelayEnvelope)
F(DelayWiden, DelayAnalyzer)
F(DelayRotate, DelayAnalyzer)
F(ResynthUnison, DelayAnalyzer)
F(ResynthShiftFade, DelayAnalyzer)
F(ResynthShiftInterval, DelayAnalyzer)

// Reverb and the dedicated delay/reverb modulation scopes.
//
F(ReverbTime, ReverbAnalyzer)
F(ReverbDampBase, ReverbAnalyzer)
F(ReverbDampWidth, ReverbAnalyzer)
F(DelayModSpeed, DelayLFO)
F(DelayModPhaseShift, DelayLFO)
F(ReverbModDepth, ReverbAnalyzer)
F(ReverbModSpeed, ReverbLFO)
F(ReverbModPhaseShift, ReverbLFO)

// Partial machine: tracked partials, spectral filtering, and spatial placement.
//
F(PartialMachineAttack, PartialMachineInputSpectrum)
F(PartialMachineDensity, PartialMachineInputSpectrum)
F(PartialMachinePortamento, PartialMachineInputSpectrum)
F(PartialMachineBWBase, PartialMachineInputSpectrum)
F(PartialMachineBWWidth, PartialMachineInputSpectrum)
F(PartialMachineReductionFeedback, PartialMachineInputSpectrum)
F(PartialMachineBassCutoff, PartialMachineInputSpectrum)
F(PartialMachineAzimuthFactor, PartialMachineSpatial)
F(PartialMachineUnison, PartialMachineAnalyzer)
F(PartialMachineLinearFrequency, PartialMachineAnalyzer)
F(PartialMachinePitchShiftDepth, PartialMachineAnalyzer)
F(PartialMachinePitchShift, PartialMachineAnalyzer)

// Quad LFOs.
//
F(QuadLFO1Skew, QuadLFO1Scope)
F(QuadLFO1Mult, QuadLFO1Scope)
F(QuadLFO1Shape, QuadLFO1Scope)
F(QuadLFO1Amplitude, QuadLFO1Scope)
F(QuadLFO1Center, QuadLFO1Scope)
F(QuadLFO1Slope, QuadLFO1Scope)
F(QuadLFO1PhaseShift, QuadLFO1Scope)
F(QuadLFO1SampleAndHold, QuadLFO1Scope)
F(QuadLFO2Skew, QuadLFO2Scope)
F(QuadLFO2Mult, QuadLFO2Scope)
F(QuadLFO2Shape, QuadLFO2Scope)
F(QuadLFO2Amplitude, QuadLFO2Scope)
F(QuadLFO2Center, QuadLFO2Scope)
F(QuadLFO2Slope, QuadLFO2Scope)
F(QuadLFO2PhaseShift, QuadLFO2Scope)
F(QuadLFO2SampleAndHold, QuadLFO2Scope)

// Tempo and time warping use the Theory of Time phase scope.
//
F(Tempo, TheoryOfTimeViz)
F(ExternalClockLoop, TheoryOfTimeViz)
F(TempoLFOSkew, TheoryOfTimeViz)
F(TempoLFOMult, TheoryOfTimeViz)
F(TempoLFOShape, TheoryOfTimeViz)
F(TempoLFOCenter, TheoryOfTimeViz)
F(TempoLFOSlope, TheoryOfTimeViz)
F(TempoLFOIndex, TheoryOfTimeViz)

// Mastering: output spectrum, EQ response, and gain reduction.
//
F(MasterVolume, MultibandEQ)
F(LowEq, MultibandEQ)
F(LowMidEq, MultibandEQ)
F(HighMidEq, MultibandEQ)
F(HighEq, MultibandEQ)
F(LowBandCutoff, MultibandEQ)
F(MidBandCutoff, MultibandEQ)
F(HighBandCutoff, MultibandEQ)
F(MasterGain, MultibandGainRed)

// Inputs: filter response and source gain meters.
//
F(Source1HP, InputsSourceMixerFreq)
F(Source1LP, InputsSourceMixerFreq)
F(Source1Gain, InputsSourceMixerRed)
F(Source2HP, InputsSourceMixerFreq)
F(Source2LP, InputsSourceMixerFreq)
F(Source2Gain, InputsSourceMixerRed)
F(Source3HP, InputsSourceMixerFreq)
F(Source3LP, InputsSourceMixerFreq)
F(Source3Gain, InputsSourceMixerRed)
F(Source4HP, InputsSourceMixerFreq)
F(Source4LP, InputsSourceMixerFreq)
F(Source4Gain, InputsSourceMixerRed)

// Deep vocoder: tracked frequencies and threshold curves.
//
F(DeepVocoderSlewUp, DeepVocoderSourceMixerFreq)
F(DeepVocoderSlewDown, DeepVocoderSourceMixerFreq)
F(DVGainThresh1, DeepVocoderSourceMixerFreq)
F(DVRatio1, DeepVocoderSourceMixerFreq)
F(DVSlopeDown1, DeepVocoderSourceMixerFreq)
F(DVSlopeUp1, DeepVocoderSourceMixerFreq)
F(DVGainThresh2, DeepVocoderSourceMixerFreq)
F(DVRatio2, DeepVocoderSourceMixerFreq)
F(DVSlopeDown2, DeepVocoderSourceMixerFreq)
F(DVSlopeUp2, DeepVocoderSourceMixerFreq)
F(DVGainThresh3, DeepVocoderSourceMixerFreq)
F(DVRatio3, DeepVocoderSourceMixerFreq)
F(DVSlopeDown3, DeepVocoderSourceMixerFreq)
F(DVSlopeUp3, DeepVocoderSourceMixerFreq)

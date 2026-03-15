// F(VisualizerName, Bank, Block, ComponentConstructor, SourceMachineFlags)
//
// Bank: from SmartGridOneEncoders::Bank enum
// Block: 0 = (0,0), 1 = (0,8), 2 = (8,8), 3 = (16,8)
// Block -1 = special/full-width
//
// Source bank
//
// VCO scopes - DualVCO only
//
F(
    VCO1Scope,
    Source,
    0,
    std::make_unique<ScopeComponent>(
        0,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState),
    VoiceMachine::SourceMachineFlags::DualVCOOnly())
F(
    VCO2Scope,
    Source,
    1,
    std::make_unique<ScopeComponent>(
        1,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState),
    VoiceMachine::SourceMachineFlags::DualVCOOnly())

// Physical modeling frequency response - PhysicalModeling only
//
F(
    PhysModFreqResponse,
    Source,
    0,
    std::make_unique<PhysicalModelingFrequencyResponseComponent>(
        uiState,
        &m_scopeVoiceOffset),
    VoiceMachine::SourceMachineFlags::PhysicalModelingOnly())

F(
    PostFilterScope,
    Source,
    2,
    std::make_unique<ScopeComponent>(
        2,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    SourceAnalyzer,
    Source,
    3,
    std::make_unique<AnalyserComponent>(
        WindowedFFT(m_nonagon->GetAudioScopeWriter(), static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp)),
        &m_scopeVoiceOffset,
        uiState),
    VoiceMachine::SourceMachineFlags::All())

// FilterAndAmp bank
//
F(
    VoiceMeter,
    FilterAndAmp,
    0,
    std::make_unique<VoiceMeterComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    PostAmpScope,
    FilterAndAmp,
    1,
    std::make_unique<ScopeComponent>(
        3,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    FilterScope,
    FilterAndAmp,
    2,
    std::make_unique<ScopeComponent>(
        2,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    FilterAnalyzer,
    FilterAndAmp,
    3,
    std::make_unique<AnalyserComponent>(
        WindowedFFT(m_nonagon->GetAudioScopeWriter(), static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp)),
        &m_scopeVoiceOffset,
        uiState),
    VoiceMachine::SourceMachineFlags::All())

// PanningAndSequencing bank
//
F(
    SoundStage,
    PanningAndSequencing,
    0,
    std::make_unique<SoundStageComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    MelodyRoll,
    PanningAndSequencing,
    -1,
    std::make_unique<MelodyRollComponent>(m_nonagon->GetNoteWriter()),
    VoiceMachine::SourceMachineFlags::All())

// VoiceLFOs bank
//
F(
    ControlScope0,
    VoiceLFOs,
    0,
    std::make_unique<ScopeComponent>(
        0,
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    ControlScope1,
    VoiceLFOs,
    1,
    std::make_unique<ScopeComponent>(
        1,
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    ControlScope2,
    VoiceLFOs,
    2,
    std::make_unique<ScopeComponent>(
        2,
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    ControlScope3,
    VoiceLFOs,
    3,
    std::make_unique<ScopeComponent>(
        3,
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    VoiceMachine::SourceMachineFlags::All())

// Delay bank
//
F(
    DelayAnalyzer,
    Delay,
    0,
    std::make_unique<QuadAnalyserComponent>(
        uiState,
        QuadAnalyserComponent::Type::Delay),
    VoiceMachine::SourceMachineFlags::All())
F(
    QuadDelayEnvelope,
    Delay,
    1,
    std::make_unique<QuadDelayEnvelopeVisualizerComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())

// Reverb bank
//
F(
    ReverbAnalyzer,
    Reverb,
    0,
    std::make_unique<QuadAnalyserComponent>(
        uiState,
        QuadAnalyserComponent::Type::Reverb),
    VoiceMachine::SourceMachineFlags::All())

// TheoryOfTime bank
//
F(
    QuadAnalyzer,
    TheoryOfTime,
    0,
    std::make_unique<QuadAnalyserComponent>(
        uiState,
        QuadAnalyserComponent::Type::Master),
    VoiceMachine::SourceMachineFlags::All())
F(
    QuadDelayEnvelopeTot,
    TheoryOfTime,
    1,
    std::make_unique<QuadDelayEnvelopeVisualizerComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    TheoryOfTimeViz,
    TheoryOfTime,
    2,
    std::make_unique<TheoryOfTimeScopeComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())

// Mastering bank
//
F(
    SourceMixerRed,
    Mastering,
    0,
    std::make_unique<SourceMixerReductionComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    SourceMixerFreq,
    Mastering,
    1,
    std::make_unique<SourceMixerFrequencyComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    MultibandEQ,
    Mastering,
    2,
    std::make_unique<MultibandEQComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    MultibandGainRed,
    Mastering,
    3,
    std::make_unique<MultibandGainReductionComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())

// Inputs bank
//
F(
    InputsSourceMixerRed,
    Inputs,
    0,
    std::make_unique<SourceMixerReductionComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    InputsSourceMixerFreq,
    Inputs,
    1,
    std::make_unique<SourceMixerFrequencyComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    InputsMultibandEQ,
    Inputs,
    2,
    std::make_unique<MultibandEQComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    InputsMultibandGainRed,
    Inputs,
    3,
    std::make_unique<MultibandGainReductionComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())

// DeepVocoder bank
//
F(
    DeepVocoderSourceMixerRed,
    DeepVocoder,
    0,
    std::make_unique<SourceMixerReductionComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    DeepVocoderSourceMixerFreq,
    DeepVocoder,
    1,
    std::make_unique<SourceMixerFrequencyComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    DeepVocoderMultibandEQ,
    DeepVocoder,
    2,
    std::make_unique<MultibandEQComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())
F(
    DeepVocoderMultibandGainRed,
    DeepVocoder,
    3,
    std::make_unique<MultibandGainReductionComponent>(uiState),
    VoiceMachine::SourceMachineFlags::All())

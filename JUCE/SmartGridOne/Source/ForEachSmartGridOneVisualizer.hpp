// F(VisualizerName, Bank, Block, ComponentConstructor)
//
// Bank: from SmartGridOneEncoders::Bank enum
// Block: 0 = (0,0), 1 = (0,8), 2 = (8,8), 3 = (16,8)
// Block -1 = special/full-width
//
// Source bank
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
        uiState))
F(
    VCO2Scope,
    Source,
    1,
    std::make_unique<ScopeComponent>(
        1,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState))
F(
    PostFilterScope,
    Source,
    2,
    std::make_unique<ScopeComponent>(
        2,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState))
F(
    SourceAnalyzer,
    Source,
    3,
    std::make_unique<AnalyserComponent>(
        WindowedFFT(m_nonagon->GetAudioScopeWriter(), static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp)),
        &m_scopeVoiceOffset,
        uiState))

// FilterAndAmp bank
//
F(
    VoiceMeter,
    FilterAndAmp,
    0,
    std::make_unique<VoiceMeterComponent>(uiState))
F(
    PostAmpScope,
    FilterAndAmp,
    1,
    std::make_unique<ScopeComponent>(
        3,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState))
F(
    FilterScope,
    FilterAndAmp,
    2,
    std::make_unique<ScopeComponent>(
        2,
        m_nonagon->GetAudioScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Audio,
        uiState))
F(
    FilterAnalyzer,
    FilterAndAmp,
    3,
    std::make_unique<AnalyserComponent>(
        WindowedFFT(m_nonagon->GetAudioScopeWriter(), static_cast<size_t>(SmartGridOne::AudioScopes::PostAmp)),
        &m_scopeVoiceOffset,
        uiState))

// PanningAndSequencing bank
//
F(
    SoundStage,
    PanningAndSequencing,
    0,
    std::make_unique<SoundStageComponent>(uiState))
F(
    MelodyRoll,
    PanningAndSequencing,
    -1,
    std::make_unique<MelodyRollComponent>(m_nonagon->GetNoteWriter()))

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
        uiState))
F(
    ControlScope1,
    VoiceLFOs,
    1,
    std::make_unique<ScopeComponent>(
        1,
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState))
F(
    ControlScope2,
    VoiceLFOs,
    2,
    std::make_unique<ScopeComponent>(
        2,
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState))
F(
    ControlScope3,
    VoiceLFOs,
    3,
    std::make_unique<ScopeComponent>(
        3,
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState))

// Delay bank
//
F(
    DelayAnalyzer,
    Delay,
    0,
    std::make_unique<QuadAnalyserComponent>(
        uiState,
        QuadAnalyserComponent::Type::Delay))

// Reverb bank
//
F(
    ReverbAnalyzer,
    Reverb,
    0,
    std::make_unique<QuadAnalyserComponent>(
        uiState,
        QuadAnalyserComponent::Type::Reverb))

// TheoryOfTime bank
//
F(
    QuadAnalyzer,
    TheoryOfTime,
    0,
    std::make_unique<QuadAnalyserComponent>(
        uiState,
        QuadAnalyserComponent::Type::Master))
F(
    TheoryOfTimeViz,
    TheoryOfTime,
    2,
    std::make_unique<TheoryOfTimeScopeComponent>(uiState))

// Mastering bank
//
F(
    SourceMixerRed,
    Mastering,
    0,
    std::make_unique<SourceMixerReductionComponent>(uiState))
F(
    SourceMixerFreq,
    Mastering,
    1,
    std::make_unique<SourceMixerFrequencyComponent>(uiState))
F(
    MultibandEQ,
    Mastering,
    2,
    std::make_unique<MultibandEQComponent>(uiState))
F(
    MultibandGainRed,
    Mastering,
    3,
    std::make_unique<MultibandGainReductionComponent>(uiState))

// Inputs bank
//
F(
    InputsSourceMixerRed,
    Inputs,
    0,
    std::make_unique<SourceMixerReductionComponent>(uiState))
F(
    InputsSourceMixerFreq,
    Inputs,
    1,
    std::make_unique<SourceMixerFrequencyComponent>(uiState))
F(
    InputsMultibandEQ,
    Inputs,
    2,
    std::make_unique<MultibandEQComponent>(uiState))
F(
    InputsMultibandGainRed,
    Inputs,
    3,
    std::make_unique<MultibandGainReductionComponent>(uiState))

// DeepVocoder bank
//
F(
    DeepVocoderSourceMixerRed,
    DeepVocoder,
    0,
    std::make_unique<SourceMixerReductionComponent>(uiState))
F(
    DeepVocoderSourceMixerFreq,
    DeepVocoder,
    1,
    std::make_unique<SourceMixerFrequencyComponent>(uiState))
F(
    DeepVocoderMultibandEQ,
    DeepVocoder,
    2,
    std::make_unique<MultibandEQComponent>(uiState))
F(
    DeepVocoderMultibandGainRed,
    DeepVocoder,
    3,
    std::make_unique<MultibandGainReductionComponent>(uiState))

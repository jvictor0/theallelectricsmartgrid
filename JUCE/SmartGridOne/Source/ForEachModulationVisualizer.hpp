// G(VisualizerName, Slot, ComponentConstructor, Mode, Color)
//
G(
    GangedRandom1Scope,
    0,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[0],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        SmartGridOneEncoders::GetModulatorSkin(0, SmartGridOneEncoders::BankMode::Voice).m_color),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(0, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    GangedRandom2Scope,
    1,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[1],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        SmartGridOneEncoders::GetModulatorSkin(1, SmartGridOneEncoders::BankMode::Voice).m_color),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(1, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    GangedRandom3Scope,
    2,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[2],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Voice).m_color),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    GangedRandom4Scope,
    3,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[3],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Voice).m_color),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    ModulationEnvelopeScope,
    4,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::ModulationEnvelope),
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(4, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    AmpEnvelopeScope,
    5,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::AmpEnvelope),
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(5, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    LFO1Scope,
    6,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::LFO1),
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(6, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    LFO2Scope,
    7,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::LFO2),
        m_nonagon->GetControlScopeWriter(),
        &m_scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(7, SmartGridOneEncoders::BankMode::Voice).m_color)
G(
    SheafyModulator,
    8,
    std::make_unique<SheafyModulatorComponent>(uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGrid::Color::White)
G(
    QuadGangedRandom1Scope,
    2,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[0],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Quad).m_color),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Quad).m_color)
G(
    QuadGangedRandom2Scope,
    3,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[1],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Quad).m_color),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Quad).m_color)
G(
    DelayLFO,
    4,
    std::make_unique<PolyphonicScopeComponent>(
        m_nonagon->GetQuadControlScopeWriter(),
        static_cast<size_t>(SmartGridOne::QuadControlScopes::DelayLFO),
        4,
        SmartGridOneEncoders::QuadratureColor(0),
        0.5f,
        0.5f),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::QuadratureColor(0))
G(
    ReverbLFO,
    5,
    std::make_unique<PolyphonicScopeComponent>(
        m_nonagon->GetQuadControlScopeWriter(),
        static_cast<size_t>(SmartGridOne::QuadControlScopes::ReverbLFO),
        4,
        SmartGridOneEncoders::QuadratureColor(1),
        0.5f,
        0.5f),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::QuadratureColor(1))
G(
    QuadLFO1Scope,
    6,
    std::make_unique<PolyphonicScopeComponent>(
        m_nonagon->GetQuadControlScopeWriter(),
        static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadLFO1),
        4,
        SmartGrid::Color::Pink,
        0.9f,
        0.05f),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::QuadLFOColor(0))
G(
    QuadLFO2Scope,
    7,
    std::make_unique<PolyphonicScopeComponent>(
        m_nonagon->GetQuadControlScopeWriter(),
        static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadLFO2),
        4,
        SmartGrid::Color::Pink,
        0.9f,
        0.05f),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::QuadLFOColor(1))
G(
    GlobalGangedRandom1Scope,
    2,
    std::make_unique<GangedRandomLFOComponent<1>>(
        &uiState->m_squiggleBoyUIState.m_globalGangedRandomLFOUIState[0],
        1,
        nullptr,
        GangedRandomLFOComponent<1>::ColorMode::Fixed,
        SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Global).m_color),
    SmartGridOneEncoders::BankMode::Global,
    SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Global).m_color)
G(
    GlobalGangedRandom2Scope,
    3,
    std::make_unique<GangedRandomLFOComponent<1>>(
        &uiState->m_squiggleBoyUIState.m_globalGangedRandomLFOUIState[1],
        1,
        nullptr,
        GangedRandomLFOComponent<1>::ColorMode::Fixed,
        SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Global).m_color),
    SmartGridOneEncoders::BankMode::Global,
    SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Global).m_color)

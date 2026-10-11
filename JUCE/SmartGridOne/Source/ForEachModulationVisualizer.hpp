// G(VisualizerName, Slot, ComponentConstructor, Mode, Color, Modulator): panel and optional encoder view
// H(Modulator, ComponentConstructor): encoder view only
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
    SmartGridOneEncoders::GetModulatorSkin(0, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::Random1)
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
    SmartGridOneEncoders::GetModulatorSkin(1, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::Random2)
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
    SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::Random3)
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
    SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::Random4)
G(
    ModulationEnvelopeScope,
    4,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::ModulationEnvelope),
        &uiState->m_squiggleBoyUIState.m_controlScopeWriter,
        scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(4, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::ModulationEnvelope)
G(
    AmpEnvelopeScope,
    5,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::AmpEnvelope),
        &uiState->m_squiggleBoyUIState.m_controlScopeWriter,
        scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(5, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::AmpEnvelope)
G(
    LFO1Scope,
    6,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::LFO1),
        &uiState->m_squiggleBoyUIState.m_controlScopeWriter,
        scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(6, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::LFO1)
G(
    LFO2Scope,
    7,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::ControlScopes::LFO2),
        &uiState->m_squiggleBoyUIState.m_controlScopeWriter,
        scopeVoiceOffset,
        ScopeComponent::ScopeType::Control,
        uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGridOneEncoders::GetModulatorSkin(7, SmartGridOneEncoders::BankMode::Voice).m_color,
    VoiceModulator::LFO2)
G(
    SheafyModulator,
    8,
    std::make_unique<SheafyModulatorComponent>(uiState),
    SmartGridOneEncoders::BankMode::Voice,
    SmartGrid::Color::White,
    std::nullopt)
G(
    QuadGangedRandom1Scope,
    0,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[0],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        SmartGridOneEncoders::GetModulatorSkin(0, SmartGridOneEncoders::BankMode::Quad).m_color),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::GetModulatorSkin(0, SmartGridOneEncoders::BankMode::Quad).m_color,
    QuadModulator::Random1)
G(
    QuadGangedRandom2Scope,
    1,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[1],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        SmartGridOneEncoders::GetModulatorSkin(1, SmartGridOneEncoders::BankMode::Quad).m_color),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::GetModulatorSkin(1, SmartGridOneEncoders::BankMode::Quad).m_color,
    QuadModulator::Random2)
G(
    QuadGangedRandom3Scope,
    2,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[2],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Quad).m_color),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Quad).m_color,
    QuadModulator::Random3)
G(
    QuadGangedRandom4Scope,
    3,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[3],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Quad).m_color),
    SmartGridOneEncoders::BankMode::Quad,
    SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Quad).m_color,
    QuadModulator::Random4)
G(
    DelayLFO,
    4,
    std::make_unique<PolyphonicScopeComponent>(
        &uiState->m_squiggleBoyUIState.m_quadControlScopeWriter,
        static_cast<size_t>(SmartGridOne::QuadControlScopes::DelayLFO),
        4,
        GetModulatorColor(QuadModulator::DelayLFO),
        0.5f,
        0.5f),
    SmartGridOneEncoders::BankMode::Quad,
    GetModulatorColor(QuadModulator::DelayLFO),
    QuadModulator::DelayLFO)
G(
    ReverbLFO,
    5,
    std::make_unique<PolyphonicScopeComponent>(
        &uiState->m_squiggleBoyUIState.m_quadControlScopeWriter,
        static_cast<size_t>(SmartGridOne::QuadControlScopes::ReverbLFO),
        4,
        GetModulatorColor(QuadModulator::ReverbLFO),
        0.5f,
        0.5f),
    SmartGridOneEncoders::BankMode::Quad,
    GetModulatorColor(QuadModulator::ReverbLFO),
    QuadModulator::ReverbLFO)
G(
    QuadLFO1Scope,
    6,
    std::make_unique<PolyphonicScopeComponent>(
        &uiState->m_squiggleBoyUIState.m_quadControlScopeWriter,
        static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadLFO1),
        4,
        GetModulatorColor(QuadModulator::LFO1),
        0.9f,
        0.05f),
    SmartGridOneEncoders::BankMode::Quad,
    GetModulatorColor(QuadModulator::LFO1),
    QuadModulator::LFO1)
G(
    QuadLFO2Scope,
    7,
    std::make_unique<PolyphonicScopeComponent>(
        &uiState->m_squiggleBoyUIState.m_quadControlScopeWriter,
        static_cast<size_t>(SmartGridOne::QuadControlScopes::QuadLFO2),
        4,
        GetModulatorColor(QuadModulator::LFO2),
        0.9f,
        0.05f),
    SmartGridOneEncoders::BankMode::Quad,
    GetModulatorColor(QuadModulator::LFO2),
    QuadModulator::LFO2)
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
    SmartGridOneEncoders::GetModulatorSkin(2, SmartGridOneEncoders::BankMode::Global).m_color,
    GlobalModulator::Random1)
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
    SmartGridOneEncoders::GetModulatorSkin(3, SmartGridOneEncoders::BankMode::Global).m_color,
    GlobalModulator::Random2)

H(VoiceModulator::Sheaf1, std::make_unique<SheafyModulatorDialComponent>(uiState, VoiceModulator::Sheaf1))
H(VoiceModulator::Sheaf2, std::make_unique<SheafyModulatorDialComponent>(uiState, VoiceModulator::Sheaf2))
H(VoiceModulator::Sheaf3, std::make_unique<SheafyModulatorDialComponent>(uiState, VoiceModulator::Sheaf3))
H(VoiceModulator::Spread, std::make_unique<SpreadModulatorComponent>(SquiggleBoy::x_voicesPerTrack, &uiState->m_squiggleBoyUIState.m_activeTrack))
H(QuadModulator::Spread, std::make_unique<SpreadModulatorComponent>(4, nullptr))
H(VoiceModulator::Noise, std::make_unique<NoiseModulatorComponent>(GetModulatorColor(VoiceModulator::Noise)))
H(QuadModulator::Noise, std::make_unique<NoiseModulatorComponent>(GetModulatorColor(QuadModulator::Noise)))
H(GlobalModulator::Noise, std::make_unique<NoiseModulatorComponent>(GetModulatorColor(GlobalModulator::Noise)))

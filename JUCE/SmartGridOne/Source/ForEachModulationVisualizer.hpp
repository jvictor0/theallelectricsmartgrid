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
        static_cast<size_t>(SmartGridOne::VoiceScopes::ModulationEnvelope),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
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
        static_cast<size_t>(SmartGridOne::VoiceScopes::AmpEnvelope),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
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
        static_cast<size_t>(SmartGridOne::VoiceScopes::LFO1),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
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
        static_cast<size_t>(SmartGridOne::VoiceScopes::LFO2),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
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
    QuadLFO1Scope,
    6,
    std::make_unique<PolyphonicScopeComponent>(uiState, QuadModulator::LFO1),
    SmartGridOneEncoders::BankMode::Quad,
    GetModulatorColor(QuadModulator::LFO1),
    QuadModulator::LFO1)
G(
    QuadLFO2Scope,
    7,
    std::make_unique<PolyphonicScopeComponent>(uiState, QuadModulator::LFO2),
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

H(VoiceModulator::Sub,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::VoiceScopes::Sub),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
        scopeVoiceOffset, ScopeComponent::ScopeType::Control, uiState))
H(VoiceModulator::Voice,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::VoiceScopes::PostAmp),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
        scopeVoiceOffset, ScopeComponent::ScopeType::Control, uiState))

H(
    VoiceModulator::Random5,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[4],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        GetModulatorColor(VoiceModulator::Random5)))

H(
    VoiceModulator::Random6,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[5],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        GetModulatorColor(VoiceModulator::Random6)))

H(
    VoiceModulator::Random7,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[6],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        GetModulatorColor(VoiceModulator::Random7)))

H(
    VoiceModulator::Random8,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[7],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        GetModulatorColor(VoiceModulator::Random8)))

H(
    VoiceModulator::Random9,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[8],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        GetModulatorColor(VoiceModulator::Random9)))

H(
    VoiceModulator::Random10,
    std::make_unique<GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>>(
        uiState->m_squiggleBoyUIState.m_gangedRandomLFOUIState[9],
        SquiggleBoy::x_numTracks,
        &uiState->m_squiggleBoyUIState.m_activeTrack,
        GangedRandomLFOComponent<SquiggleBoy::x_voicesPerTrack>::ColorMode::TrackVoices,
        GetModulatorColor(VoiceModulator::Random10)))

H(
    QuadModulator::Random5,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[4],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        GetModulatorColor(QuadModulator::Random5)))

H(
    QuadModulator::Random6,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[5],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        GetModulatorColor(QuadModulator::Random6)))

H(
    QuadModulator::Random7,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[6],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        GetModulatorColor(QuadModulator::Random7)))

H(
    QuadModulator::Random8,
    std::make_unique<GangedRandomLFOComponent<4>>(
        &uiState->m_squiggleBoyUIState.m_quadGangedRandomLFOUIState[7],
        1,
        nullptr,
        GangedRandomLFOComponent<4>::ColorMode::Voices,
        GetModulatorColor(QuadModulator::Random8)))

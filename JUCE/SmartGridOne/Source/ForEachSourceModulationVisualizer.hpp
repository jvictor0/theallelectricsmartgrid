// F(SourceMachine, Modulator, ComponentConstructor): source-pair encoder views.
// Sample and Thru have no source modulation view.
//
F(DualWaveShapingVCO, SourceMod1,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::VoiceScopes::VCO1),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
        scopeVoiceOffset, ScopeComponent::ScopeType::Control, uiState))
F(DualWaveShapingVCO, SourceMod2,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::VoiceScopes::VCO2),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
        scopeVoiceOffset, ScopeComponent::ScopeType::Control, uiState))
F(PhysicalModeling, SourceMod1,
    std::make_unique<NoiseModulatorComponent>(GetModulatorColor(VoiceModulator::SourceMod1)))
F(PhysicalModeling, SourceMod2,
    std::make_unique<ScopeComponent>(
        static_cast<size_t>(SmartGridOne::VoiceScopes::PhysicalModelingEnvelope),
        &uiState->m_squiggleBoyUIState.m_voiceScopeWriter,
        scopeVoiceOffset, ScopeComponent::ScopeType::Control, uiState))

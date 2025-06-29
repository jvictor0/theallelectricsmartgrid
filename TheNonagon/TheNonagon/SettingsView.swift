import SwiftUI
import CoreMIDI

class SettingsViewModel: ObservableObject 
{
    @Published var m_selectedMidiInput: Int = 0
    @Published var m_selectedMidiOutput: Int = 0
    @Published var m_midiDestinations: [(id: Int, name: String)] = []
    @Published var m_midiSources: [(id: Int, name: String)] = []
    
    @Published var m_sendClock: Bool = false
    @Published var m_receiveClock: Bool = false
    @Published var m_sendTransport: Bool = false
    @Published var m_receiveTransport: Bool = false
    @Published var m_sendProgramChange: Bool = false
    @Published var m_receiveProgramChange: Bool = false

    private let m_bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) 
    {
        self.m_bridge = bridge
        RefreshMidiDestinations()
        if let settings = m_bridge.GetMidiSettings() 
        {
            m_selectedMidiInput = Int(settings.pointee.m_sourceIndex)
            m_selectedMidiOutput = Int(settings.pointee.m_destinationIndex)
            
            m_sendClock = settings.pointee.m_sendClock
            m_receiveClock = settings.pointee.m_receiveClock
            m_sendTransport = settings.pointee.m_sendTransport
            m_receiveTransport = settings.pointee.m_receiveTransport
            m_sendProgramChange = settings.pointee.m_sendProgramChange
            m_receiveProgramChange = settings.pointee.m_receiveProgramChange

            if m_midiDestinations.count - 1 <= m_selectedMidiOutput
            {
                m_selectedMidiOutput = -1
                settings.pointee.m_destinationIndex = -1
            }

            if m_midiSources.count - 1 <= m_selectedMidiInput
            {
                m_selectedMidiInput = -1
                settings.pointee.m_sourceIndex = -1
            }
        }
    }
    
    func RefreshMidiDestinations() 
    {
        m_midiDestinations = []
        m_midiSources = []
        // Add "None" option
        m_midiDestinations.append((id: -1, name: "None"))
        m_midiSources.append((id: -1, name: "None"))
        
        // Get available MIDI destinations
        let numDests = MIDIGetNumberOfDestinations()
        for i in 0..<numDests 
        {
            let dest = MIDIGetDestination(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(dest, kMIDIPropertyName, &name)
            if let name = name?.takeRetainedValue() as String? 
            {
                m_midiDestinations.append((id: i, name: name))
            }
        }

        // Get available MIDI sources
        let numSources = MIDIGetNumberOfSources()
        for i in 0..<numSources 
        {
            let source = MIDIGetSource(i)
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name)
            if let name = name?.takeRetainedValue() as String? 
            {
                m_midiSources.append((id: i, name: name))
            }
        }
    }
    
    func SetMidiInput(_ index: Int) 
    {
        if let settings = m_bridge.GetMidiSettings() 
        {
            settings.pointee.m_sourceIndex = Int32(index)
        }
    }
    
    func SetMidiOutput(_ index: Int) 
    {
        if let settings = m_bridge.GetMidiSettings() 
        {
            settings.pointee.m_destinationIndex = Int32(index)
        }
    }
    
    func UpdateMidiSettings() 
    {
        if let settings = m_bridge.GetMidiSettings() 
        {
            settings.pointee.m_sendClock = m_sendClock
            settings.pointee.m_receiveClock = m_receiveClock
            settings.pointee.m_sendTransport = m_sendTransport
            settings.pointee.m_receiveTransport = m_receiveTransport
            settings.pointee.m_sendProgramChange = m_sendProgramChange
            settings.pointee.m_receiveProgramChange = m_receiveProgramChange
        }
    }
}

struct SettingsView: View 
{
    @StateObject private var m_viewModel: SettingsViewModel
    @Environment(\.dismiss) private var m_dismiss
    
    init(bridge: TheNonagonBridge) 
    {
        _m_viewModel = StateObject(wrappedValue: SettingsViewModel(bridge: bridge))
    }
    
    var body: some View 
    {
        NavigationView 
        {
            Form 
            {
                Section(header: Text("MIDI Input")) 
                {
                    Picker("Select MIDI Input", selection: $m_viewModel.m_selectedMidiInput) 
                    {
                        ForEach(m_viewModel.m_midiSources, id: \.id) { dest in
                            Text(dest.name).tag(dest.id)
                        }
                    }
                    .onChange(of: m_viewModel.m_selectedMidiInput) { oldValue, newValue in
                        m_viewModel.SetMidiInput(newValue)
                    }
                }
                
                Section(header: Text("MIDI Output")) 
                {
                    Picker("Select MIDI Output", selection: $m_viewModel.m_selectedMidiOutput) 
                    {
                        ForEach(m_viewModel.m_midiDestinations, id: \.id) { dest in
                            Text(dest.name).tag(dest.id)
                        }
                    }
                    .onChange(of: m_viewModel.m_selectedMidiOutput) { oldValue, newValue in
                        m_viewModel.SetMidiOutput(newValue)
                    }
                }
                
                Section(header: Text("MIDI Clock")) 
                {
                    VStack(spacing: 8) 
                    {
                        Toggle("Send Clock", isOn: $m_viewModel.m_sendClock)
                            .onChange(of: m_viewModel.m_sendClock) { oldValue, newValue in
                                m_viewModel.UpdateMidiSettings()
                            }
                        
                        Toggle("Receive Clock", isOn: $m_viewModel.m_receiveClock)
                            .onChange(of: m_viewModel.m_receiveClock) { oldValue, newValue in
                                m_viewModel.UpdateMidiSettings()
                            }
                    }
                }
                
                Section(header: Text("MIDI Transport")) 
                {
                    VStack(spacing: 8) 
                    {
                        Toggle("Send Transport", isOn: $m_viewModel.m_sendTransport)
                            .onChange(of: m_viewModel.m_sendTransport) { oldValue, newValue in
                                m_viewModel.UpdateMidiSettings()
                            }
                        
                        Toggle("Receive Transport", isOn: $m_viewModel.m_receiveTransport)
                            .onChange(of: m_viewModel.m_receiveTransport) { oldValue, newValue in
                                m_viewModel.UpdateMidiSettings()
                            }
                    }
                }
                
                Section(header: Text("MIDI Program Change")) 
                {
                    VStack(spacing: 8) 
                    {
                        Toggle("Send Program Change", isOn: $m_viewModel.m_sendProgramChange)
                            .onChange(of: m_viewModel.m_sendProgramChange) { oldValue, newValue in
                                m_viewModel.UpdateMidiSettings()
                            }
                        
                        Toggle("Receive Program Change", isOn: $m_viewModel.m_receiveProgramChange)
                            .onChange(of: m_viewModel.m_receiveProgramChange) { oldValue, newValue in
                                m_viewModel.UpdateMidiSettings()
                            }
                    }
                }
            }
            .navigationTitle("Settings")
            .navigationBarItems(trailing: Button("Done") 
            {
                m_dismiss()
            })
        }
    }
}

#Preview 
{
    SettingsView(bridge: TheNonagonBridge())
} 
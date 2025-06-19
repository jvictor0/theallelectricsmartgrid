import SwiftUI
import CoreMIDI

class SettingsViewModel: ObservableObject 
{
    @Published var m_selectedMidiInput: Int = 0
    @Published var m_selectedMidiOutput: Int = 0
    @Published var m_midiDestinations: [(id: Int, name: String)] = []
    private let m_bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) 
    {
        self.m_bridge = bridge
        RefreshMidiDestinations()
    }
    
    func RefreshMidiDestinations() 
    {
        m_midiDestinations = []
        
        // Add "None" option
        m_midiDestinations.append((id: -1, name: "None"))
        
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
    }
    
    func SetMidiInput(_ index: Int) 
    {
        m_selectedMidiInput = index
        m_bridge.SetMidiInput(index)
    }
    
    func SetMidiOutput(_ index: Int) 
    {
        m_selectedMidiOutput = index
        m_bridge.SetMidiOutput(index)
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
                        ForEach(m_viewModel.m_midiDestinations, id: \.id) { dest in
                            Text(dest.name).tag(dest.id)
                        }
                    }
                    .onChange(of: m_viewModel.m_selectedMidiInput) { newValue in
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
                    .onChange(of: m_viewModel.m_selectedMidiOutput) { newValue in
                        m_viewModel.SetMidiOutput(newValue)
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
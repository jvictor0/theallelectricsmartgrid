import SwiftUI

struct MainView: View 
{
    private static let x_sharedBridge = TheNonagonBridge()
    private let m_bridge = MainView.x_sharedBridge
    @StateObject private var m_viewModel: ButtonGridViewModel
    @State private var m_displayLoopController: DisplayLoopController?
    @State private var m_audioEngineController: AudioEngineController?
    
    init() 
    {
        _m_viewModel = StateObject(wrappedValue: ButtonGridViewModel(bridge: MainView.x_sharedBridge))
    }
    
    var body: some View 
    {
        HStack(spacing: 0) 
        {
            // Left Menu
            VStack 
            {
                Spacer()
            }
            .frame(width: 100)
            .background(Color.gray.opacity(0.2))
            
            // Center Content with Button Grid
            VStack(spacing: 0) 
            {
                ButtonGridView(viewModel: m_viewModel)
                
                // Bottom Menu
                HStack 
                {
                    Spacer()
                }
                .frame(height: 60)
                .background(Color.gray.opacity(0.2))
            }
            
            // Right Menu
            RightMenuView(bridge: m_bridge)
                .frame(width: 100)
                .background(Color.gray.opacity(0.2))
        }
        .onAppear 
        {
            m_displayLoopController = DisplayLoopController(bridge: m_bridge, buttonView: m_viewModel)
            m_audioEngineController = AudioEngineController(bridge: m_bridge)
        }
    }
}

#Preview 
{
    MainView()
} 
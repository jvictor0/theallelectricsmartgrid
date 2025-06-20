import SwiftUI

struct MainView: View 
{
    private static let x_sharedBridge = TheNonagonBridge()
    private let m_bridge = MainView.x_sharedBridge
    @StateObject private var m_rightMenuViewModel: RightMenuViewModel
    @StateObject private var m_leftMenuViewModel: LeftMenuViewModel
    @StateObject private var m_bottomMenuViewModel: BottomMenuViewModel
    @State private var m_displayLoopController: DisplayLoopController?
    @State private var m_audioEngineController: AudioEngineController?
    
    init() 
    {
        _m_rightMenuViewModel = StateObject(wrappedValue: RightMenuViewModel(bridge: MainView.x_sharedBridge))
        _m_leftMenuViewModel = StateObject(wrappedValue: LeftMenuViewModel(bridge: MainView.x_sharedBridge))
        _m_bottomMenuViewModel = StateObject(wrappedValue: BottomMenuViewModel(bridge: MainView.x_sharedBridge))
    }
    
    var body: some View 
    {
        HStack(spacing: 0) 
        {
            // Left Menu
            LeftMenuView(viewModel: m_leftMenuViewModel)
            
            // Center Content with Button Grid
            VStack(spacing: 0) 
            {
                HStack(spacing: 2)
                {
                    CoMuteView(bridge: m_bridge)
                    TheoryOfTimeView(bridge: m_bridge)
                }
                
                // Bottom Menu
                BottomMenuView(viewModel: m_bottomMenuViewModel)
            }
        
            // Right Menu
            RightMenuView(viewModel: m_rightMenuViewModel)
                .frame(width: 100)
                .background(Color.gray.opacity(0.2))
        }
        .onAppear 
        {
            m_displayLoopController = DisplayLoopController(
                bridge: m_bridge, 
                rightMenuView: m_rightMenuViewModel
            )
            m_audioEngineController = AudioEngineController(bridge: m_bridge)
        }
    }
}

#Preview 
{
    MainView()
} 
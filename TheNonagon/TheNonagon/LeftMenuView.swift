import SwiftUI

class LeftMenuViewModel: ObservableObject
{
    @Published var m_isSettingsPresented = false
    @ObservedObject var m_bridge: TheNonagonBridge
        
    init(bridge: TheNonagonBridge) 
    {
        self.m_bridge = bridge
    }
    
    func ToggleSettings() 
    {
        m_isSettingsPresented.toggle()
    }
}

// Custom skin for play button that includes the triangle overlay
struct PlayButtonSkin: ButtonSkinView {
    let color: Color
    let isPressed: Bool
    
    private var padColor: Color {
        return color.opacity(0.8)
    }
    
    private var ledGlowColor: Color {
        return color.opacity(0.3)
    }
    
    var body: some View {
        ZStack {
            // LED glow background
            Circle()
                .fill(ledGlowColor)
                .blur(radius: 2)
            
            // Main pad
            Circle()
                .fill(padColor)
                .overlay(
                    Circle()
                        .stroke(Color.white.opacity(0.3), lineWidth: 1)
                )
                .overlay(
                    // Play triangle
                    Path { path in
                        path.move(to: CGPoint(x: 0.35, y: 0.25))
                        path.addLine(to: CGPoint(x: 0.35, y: 0.75))
                        path.addLine(to: CGPoint(x: 0.75, y: 0.5))
                        path.closeSubpath()
                    }
                    .fill(Color.white.opacity(0.8))
                )
                .shadow(color: Color.black.opacity(0.3), radius: 2, x: 0, y: 1)
        }
        .aspectRatio(1, contentMode: .fit)
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

struct LeftMenuView: View 
{
    @ObservedObject var viewModel: LeftMenuViewModel
    
    var body: some View 
    {
        VStack 
        {
            Button(action: 
            {
                viewModel.ToggleSettings()
            }) 
            {
                Image(systemName: "gear")
                    .font(.system(size: 24))
                    .padding()
            }
            .sheet(isPresented: $viewModel.m_isSettingsPresented) 
            {
                SettingsView(bridge: viewModel.m_bridge)
            }
            
            // Play button using ButtonView with custom PlayButtonSkin
            ButtonView<PlayButtonSkin>(
                logic: ButtonCellLogic(
                    bridge: viewModel.m_bridge,
                    x: 7,
                    y: 7,
                    gridHandle: GridHandle_LameJuisCoMute
                )
            )
            
            Spacer()
        }
        .frame(width: 100)
        .background(Color.gray.opacity(0.2))
    }
}

#Preview 
{
    LeftMenuView(viewModel: LeftMenuViewModel(bridge: TheNonagonBridge()))
} 
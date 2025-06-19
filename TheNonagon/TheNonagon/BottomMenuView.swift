import SwiftUI

class BottomMenuViewModel: ObservableObject 
{
    @ObservedObject var m_bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) 
    {
        self.m_bridge = bridge
    }
}

// Custom skin for mute buttons
struct MuteButtonSkin: ButtonSkinView {
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
                    // Mute symbol (circle with line through it)
                    ZStack {
                        Circle()
                            .stroke(Color.white.opacity(0.8), lineWidth: 2)
                            .frame(width: 12, height: 12)
                        
                        Path { path in
                            path.move(to: CGPoint(x: 0.3, y: 0.3))
                            path.addLine(to: CGPoint(x: 0.7, y: 0.7))
                        }
                        .stroke(Color.white.opacity(0.8), lineWidth: 2)
                    }
                )
                .shadow(color: Color.black.opacity(0.3), radius: 2, x: 0, y: 1)
        }
        .aspectRatio(1, contentMode: .fit)
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

struct BottomMenuView: View 
{
    @ObservedObject var viewModel: BottomMenuViewModel
    
    // Convert C++ style math to Swift
    private func getX(for i: Int) -> Int {
        return 7 - (i % 3 == 0 ? 1 : 0)
    }
    
    private func getY(for i: Int) -> Int {
        return 2 * (i / 3) + (i % 3 == 2 ? 1 : 0)
    }
    
    var body: some View 
    {
        HStack 
        {
            Spacer()
            
            // Nine mute buttons horizontally
            HStack(spacing: 8) {
                ForEach(0..<9, id: \.self) { i in
                    ButtonView<MuteButtonSkin>(
                        logic: ButtonCellLogic(
                            bridge: viewModel.m_bridge,
                            x: getX(for: i),
                            y: getY(for: i),
                            gridHandle: GridHandle_LameJuisCoMute
                        )
                    )
                    .frame(width: 40, height: 40)
                }
            }
            
            Spacer()
        }
        .frame(height: 60)
        .background(Color.gray.opacity(0.2))
    }
}

#Preview 
{
    BottomMenuView(viewModel: BottomMenuViewModel(bridge: TheNonagonBridge()))
} 
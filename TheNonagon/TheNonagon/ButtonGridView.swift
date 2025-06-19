import SwiftUI
import Combine

// Protocol for button visual implementations
protocol ButtonSkinView: View {
    init(color: Color, isPressed: Bool)
}

// Reusable button logic class - independent of visual appearance
class ButtonCellLogic: ObservableObject {
    @ObservedObject var bridge: TheNonagonBridge
    let x: Int
    let y: Int
    let gridHandle: GridHandle
    
    @Published var isPressed = false
    @Published var color: Color = Color.gray
    
    init(bridge: TheNonagonBridge, x: Int, y: Int, gridHandle: GridHandle) {
        self.bridge = bridge
        self.x = x
        self.y = y
        self.gridHandle = gridHandle
        
        // Set initial color
        updateColor()
        
        // Observe bridge changes
        bridge.objectWillChange.sink { [weak self] _ in
            DispatchQueue.main.async {
                self?.updateColor()
            }
        }.store(in: &cancellables)
    }
    
    private var cancellables = Set<AnyCancellable>()
    
    private func updateColor() {
        color = bridge.buttonColors[Int(gridHandle.rawValue)][x % 8][y % 8]
    }
    
    func HandlePress() {
        if !isPressed {
            isPressed = true
            bridge.HandlePress(gridHandle: gridHandle, x: x % 8, y: y % 8)
        }
    }
    
    func HandleRelease() {
        if isPressed {
            isPressed = false
            bridge.HandleRelease(gridHandle: gridHandle, x: x % 8, y: y)
        }
    }
}

// Generic button view that works with any skin
struct ButtonView<Skin: ButtonSkinView>: View {
    @ObservedObject var logic: ButtonCellLogic
    
    @GestureState private var gesturePressed = false
    
    var body: some View {
        Skin(color: logic.color, isPressed: logic.isPressed || gesturePressed)
            .gesture(
                DragGesture(minimumDistance: 0)
                    .updating($gesturePressed) { _, state, _ in
                        if !state {
                            logic.HandlePress()
                            state = true
                        }
                    }
                    .onEnded { _ in
                        logic.HandleRelease()
                    }
            )
    }
}

// Default circular pad skin implementation
struct CircularPadSkin: ButtonSkinView {
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
                .shadow(color: Color.black.opacity(0.3), radius: 2, x: 0, y: 1)
        }
        .aspectRatio(1, contentMode: .fit)
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

struct ButtonGridView: View {
    @ObservedObject var bridge: TheNonagonBridge
    let gridHandle: GridHandle
    
    var body: some View {
        VStack(spacing: 2) {
            ForEach(0..<8, id: \.self) { row in
                HStack(spacing: 2) {
                    ForEach(0..<16, id: \.self) { col in
                        ButtonView<CircularPadSkin>(
                            logic: ButtonCellLogic(
                                bridge: bridge,
                                x: col,
                                y: row,
                                gridHandle: gridHandle
                            )
                        )
                    }
                }
            }
        }
    }
} 
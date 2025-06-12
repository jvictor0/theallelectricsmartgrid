import SwiftUI

class ButtonGridViewModel: ObservableObject {
    @Published var buttonColors: [[Color]] = Array(repeating: Array(repeating: Color.gray, count: 16), count: 8)
    @Published var buttonPressed: [[Bool]] = Array(repeating: Array(repeating: false, count: 16), count: 8)
    private let m_bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) {
        self.m_bridge = bridge
    }
    
    func SetButtonColor(x: Int, y: Int, color: Color) {
        buttonColors[y][x] = color
    }
    
    func HandlePress(x: Int, y: Int) {
        if !buttonPressed[y][x] {
            buttonPressed[y][x] = true
            m_bridge.HandlePress(x: x, y: y)
        }
    }
    
    func HandleRelease(x: Int, y: Int) {
        if buttonPressed[y][x] {
            buttonPressed[y][x] = false
            m_bridge.HandleRelease(x: x, y: y)
        }
    }
}

struct ButtonGridView: View {
    @ObservedObject var viewModel: ButtonGridViewModel
    
    var body: some View {
        VStack(spacing: 2) {
            ForEach(0..<8, id: \.self) { row in
                HStack(spacing: 2) {
                    ForEach(0..<16, id: \.self) { col in
                        ButtonCell(viewModel: viewModel, x: col, y: row)
                    }
                }
            }
        }
    }
}

struct ButtonCell: View {
    let viewModel: ButtonGridViewModel
    let x: Int
    let y: Int
    
    @GestureState private var isPressed = false
    
    var body: some View {
        Rectangle()
            .fill(viewModel.buttonColors[y][x])
            .gesture(
                DragGesture(minimumDistance: 0)
                    .updating($isPressed) { _, state, _ in
                        if !state {
                            viewModel.HandlePress(x: x, y: y)
                            state = true
                        }
                    }
                    .onEnded { _ in
                        viewModel.HandleRelease(x: x, y: y)
                    }
            )
    }
} 
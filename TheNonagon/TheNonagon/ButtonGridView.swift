import SwiftUI

class ButtonGridViewModel: ObservableObject {
    @Published var buttonColors: [[Color]] = Array(repeating: Array(repeating: Color.gray, count: 16), count: 8)
    private let m_bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) {
        self.m_bridge = bridge
    }
    
    func SetButtonColor(x: Int, y: Int, color: Color) {
        buttonColors[y][x] = color
    }
    
    func HandlePress(x: Int, y: Int) {
        m_bridge.HandlePress(x: x, y: y)
    }
    
    func HandleRelease(x: Int, y: Int) {
        m_bridge.HandleRelease(x: x, y: y)
    }
}

struct ButtonGridView: View {
    @ObservedObject var viewModel: ButtonGridViewModel
    
    var body: some View {
        VStack(spacing: 2) {
            ForEach(0..<8, id: \.self) { row in
                HStack(spacing: 2) {
                    ForEach(0..<16, id: \.self) { col in
                        Button(action: {}) {
                            Rectangle()
                                .fill(viewModel.buttonColors[row][col])
                        }
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { _ in
                                    viewModel.HandlePress(x: col, y: row)
                                }
                                .onEnded { _ in
                                    viewModel.HandleRelease(x: col, y: row)
                                }
                        )
                    }
                }
            }
        }
    }
} 
import SwiftUI

class RightMenuViewModel: ObservableObject 
{
    static let x_ButtonCount: Int = 6
    @Published var buttonColors: [Color] = Array(repeating: Color.gray, count: x_ButtonCount)
    private let m_bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) 
    {
        self.m_bridge = bridge
    }
    
    func SetButtonColor(index: Int, color: Color) 
    {
        buttonColors[index] = color
    }
    
    func HandlePress(index: Int) 
    {
        m_bridge.HandleRightMenuPress(index: index)
    }
}

struct RightMenuView: View 
{
    @ObservedObject var viewModel: RightMenuViewModel
    
    var body: some View 
    {
        VStack(spacing: 10) 
        {
            ForEach(0..<RightMenuViewModel.x_ButtonCount) { index in
                Button(action: 
                {
                    viewModel.HandlePress(index: index)
                }) 
                {
                    RoundedRectangle(cornerRadius: 8)
                        .fill(viewModel.buttonColors[index])
                        .frame(height: 40)
                }
            }
            Spacer()
        }
        .padding(.top, 20)
    }
}

#Preview 
{
    RightMenuView(viewModel: RightMenuViewModel(bridge: TheNonagonBridge()))
} 
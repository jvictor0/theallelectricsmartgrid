import SwiftUI

class RightMenuViewModel: ObservableObject 
{
    static let x_ButtonCount: Int = 2
    @Published var buttonColors: [Color] = Array(repeating: Color.gray, count: x_ButtonCount)
    private let m_bridge: TheNonagonBridge
    private let m_centerViewModel: CenterViewModel
    
    init(bridge: TheNonagonBridge, centerViewModel: CenterViewModel) 
    {
        self.m_bridge = bridge
        self.m_centerViewModel = centerViewModel
    }
    
    func SetButtonColor(index: Int, color: Color) 
    {
        buttonColors[index] = color
    }
    
    func SetModeToTheoryOfTime() 
    {
        m_centerViewModel.mode = .TheoryOfTime
    }
    
    func SetModeToGrid() 
    {
        m_centerViewModel.mode = .Grid
    }
}

struct RightMenuView: View 
{
    @ObservedObject var viewModel: RightMenuViewModel
    
    var body: some View 
    {
        VStack(spacing: 10) 
        {
            // First button - Theory of Time
            Button(action: 
            {
                viewModel.SetModeToTheoryOfTime()
            }) 
            {
                RoundedRectangle(cornerRadius: 8)
                    .fill(viewModel.buttonColors[0])
                    .frame(height: 40)
                    .overlay(
                        Text("Theory")
                            .foregroundColor(.white)
                            .font(.caption)
                    )
            }
            
            // Second button - Grid
            Button(action: 
            {
                viewModel.SetModeToGrid()
            }) 
            {
                RoundedRectangle(cornerRadius: 8)
                    .fill(viewModel.buttonColors[1])
                    .frame(height: 40)
                    .overlay(
                        Text("Grid")
                            .foregroundColor(.white)
                            .font(.caption)
                    )
            }
            
            Spacer()
        }
        .padding(.top, 20)
    }
}

#Preview 
{
    let bridge = TheNonagonBridge()
    let centerViewModel = CenterViewModel(bridge: bridge)
    RightMenuView(viewModel: RightMenuViewModel(bridge: bridge, centerViewModel: centerViewModel))
} 
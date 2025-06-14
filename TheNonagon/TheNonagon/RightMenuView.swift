import SwiftUI

struct RightMenuView: View 
{
    private let m_bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) 
    {
        m_bridge = bridge
    }
    
    var body: some View 
    {
        VStack(spacing: 10) 
        {
            ForEach(0..<5) { index in
                Button(action: 
                {
                    m_bridge.HandleRightMenuPress(index: index)
                }) 
                {
                    RoundedRectangle(cornerRadius: 8)
                        .fill(m_bridge.GetRightMenuColor(index: index))
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
    RightMenuView(bridge: TheNonagonBridge())
} 
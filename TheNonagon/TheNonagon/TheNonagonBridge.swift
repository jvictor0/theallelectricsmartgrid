import Foundation

class TheNonagonBridge 
{
    private var m_state: UnsafeMutableRawPointer?
    
    init() 
    {
        m_state = createTheNonagonBridgeState()
    }
    
    deinit 
    {
        if let state = m_state 
        {
            destroyTheNonagonBridgeState(state)
        }
    }
    
    func HandlePress(x: Int, y: Int) 
    {
        if let state = m_state 
        {
            handleTheNonagonBridgeStatePress(state, Int32(x), Int32(y))
        }
    }
    
    func HandleRelease(x: Int, y: Int) 
    {
        if let state = m_state 
        {
            handleTheNonagonBridgeStateRelease(state, Int32(x), Int32(y))
        }
    }

    func GetColor(x: Int, y: Int) -> RGBColor 
    {
        var color = RGBColor(m_red: 0, m_green: 0, m_blue: 0)
        if let state = m_state 
        {
            getTheNonagonBridgeStateColor(state, Int32(x), Int32(y), &color)
        }
        return color
    }

    func Process() 
    {
        if let state = m_state 
        {
            processTheNonagonBridgeState(state)
        }
    }
} 
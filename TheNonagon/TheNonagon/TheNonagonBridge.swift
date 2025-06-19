import Foundation
import SwiftUI

public class TheNonagonBridge: ObservableObject
{
    @Published var buttonColors: [[[Color]]] = Array(repeating: Array(repeating: Array(repeating: Color.gray, count: 8), count: 8), count: Int(GridHandle_NumGrids.rawValue))

    private var m_state: UnsafeMutableRawPointer?
    
    public init() 
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
    
    public func HandlePress(gridHandle: GridHandle, x: Int, y: Int) 
    {
        if let state = m_state 
        {
            handleTheNonagonBridgeStatePress(state, gridHandle, Int32(x), Int32(y))
        }
    }
    
    public func HandleRelease(gridHandle: GridHandle, x: Int, y: Int) 
    {
        if let state = m_state 
        {
            handleTheNonagonBridgeStateRelease(state, gridHandle, Int32(x), Int32(y))
        }
    }

    public func GetColor(gridHandle: GridHandle, x: Int, y: Int) -> Color 
    {
        var color = RGBColor(m_red: 0, m_green: 0, m_blue: 0)
        if let state = m_state 
        {
            getTheNonagonBridgeStateColor(state, gridHandle, Int32(x), Int32(y), &color)
        }
        return Color(red: Double(color.m_red) / 255.0, green: Double(color.m_green) / 255.0, blue: Double(color.m_blue) / 255.0)
    }
    
    public func HandleRightMenuPress(index: Int) 
    {
        if let state = m_state 
        {
            handleTheNonagonBridgeStateRightMenuPress(state, Int32(index))
        }
    }
    
    public func GetRightMenuColor(index: Int) -> Color 
    {
        var color = RGBColor(m_red: 0, m_green: 0, m_blue: 0)
        if let state = m_state 
        {
            getTheNonagonBridgeStateRightMenuColor(state, Int32(index), &color)
        }
        return Color(red: Double(color.m_red) / 255.0, green: Double(color.m_green) / 255.0, blue: Double(color.m_blue) / 255.0)
    }

    public func Process(_ audioBuffer: UnsafeMutablePointer<UnsafeMutablePointer<Float>?>, _ numChannels: Int32, _ numFrames: Int32) 
    {
        if let state = m_state 
        {
            processTheNonagonBridgeState(state, audioBuffer, numChannels, numFrames)
        }
    }
    
    public func SetMidiInput(_ index: Int) 
    {
        if let state = m_state 
        {
            setTheNonagonBridgeStateMidiInput(state, Int32(index))
        }
    }
    
    public func SetMidiOutput(_ index: Int) 
    {
        if let state = m_state 
        {
            setTheNonagonBridgeStateMidiOutput(state, Int32(index))
        }
    }

    public func PopulateButtonColors()
    {
        for gridHandle in 0..<Int(GridHandle_NumGrids.rawValue)
        {
            for x in 0..<8
            {
                for y in 0..<8
                {
                    buttonColors[gridHandle][x][y] = GetColor(gridHandle: GridHandle(UInt32(gridHandle)), x: x, y: y)
                }
            }
        }
    }
} 
import Foundation
import QuartzCore
import SwiftUI

class DisplayLoopController 
{
    private var m_displayLink: CADisplayLink?
    private let m_bridge: TheNonagonBridge
    private let m_buttonView: ButtonGridViewModel
    
    init(bridge: TheNonagonBridge, buttonView: ButtonGridViewModel) 
    {
        self.m_bridge = bridge
        self.m_buttonView = buttonView
        SetupDisplayLink()
    }
    
    deinit 
    {
        StopDisplayLink()
    }
    
    private func SetupDisplayLink() 
    {
        m_displayLink = CADisplayLink(target: self, selector: #selector(DisplayLoop))
        m_displayLink?.preferredFrameRateRange = CAFrameRateRange(minimum: 30, maximum: 60, preferred: 60)
        m_displayLink?.add(to: .main, forMode: .common)
    }
    
    private func StopDisplayLink() 
    {
        m_displayLink?.invalidate()
        m_displayLink = nil
    }
    
    @objc private func DisplayLoop() 
    {
        // Process the bridge state
        m_bridge.Process()
        
        // Update colors for all buttons
        for y in 0..<8 
        {
            for x in 0..<16 
            {
                let color = m_bridge.GetColor(x: x, y: y)
                let swiftColor = SwiftUI.Color(red: Double(color.m_red) / 255.0,
                                     green: Double(color.m_green) / 255.0,
                                     blue: Double(color.m_blue) / 255.0)
                m_buttonView.SetButtonColor(x: x, y: y, color: swiftColor)
            }
        }
    }
} 
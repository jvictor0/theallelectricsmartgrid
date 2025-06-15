import Foundation
import QuartzCore
import SwiftUI

class DisplayLoopController 
{
    private var m_displayLink: CADisplayLink?
    private let m_bridge: TheNonagonBridge
    private let m_buttonView: ButtonGridViewModel
    private let m_rightMenuView: RightMenuViewModel
    
    init(bridge: TheNonagonBridge, buttonView: ButtonGridViewModel, rightMenuView: RightMenuViewModel) 
    {
        self.m_bridge = bridge
        self.m_buttonView = buttonView
        self.m_rightMenuView = rightMenuView
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
        // Update colors for all buttons
        for y in 0..<8 
        {
            for x in 0..<16 
            {
                let color = m_bridge.GetColor(x: x, y: y)
                m_buttonView.SetButtonColor(x: x, y: y, color: color)
            }
        }
        
        // Update colors for right menu buttons
        for index in 0..<RightMenuViewModel.x_ButtonCount 
        {
            let color = m_bridge.GetRightMenuColor(index: index)
            m_rightMenuView.SetButtonColor(index: index, color: color)
        }
    }
} 
import SwiftUI
import Combine

// CoMute skin - smaller circle
struct CoMuteSkin: ButtonSkinView
{
    let color: Color
    let isPressed: Bool
    
    private var padColor: Color
    {
        return color.opacity(0.8)
    }
    
    private var ledGlowColor: Color
    {
        return color.opacity(0.3)
    }
    
    var body: some View
    {
        ZStack
        {
            // LED glow background  
            Circle()
                .fill(ledGlowColor)
                .blur(radius: 1)
            
            // Main pad - smaller than CircularPadSkin
            Circle()
                .fill(padColor)
                .overlay(
                    Circle()
                        .stroke(Color.white.opacity(0.3), lineWidth: 0.5)
                )
                .shadow(color: Color.black.opacity(0.3), radius: 1, x: 0, y: 0.5)
        }
        .aspectRatio(0.8, contentMode: .fit) // Match StepSelector aspect ratio
        .scaleEffect(0.7) // Make it smaller than the default circular pad
        .scaleEffect(isPressed ? 0.85 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

// Step selector skin - rounded rectangle, slightly taller than wide
struct StepSelectorSkin: ButtonSkinView
{
    let color: Color
    let isPressed: Bool
    
    private var padColor: Color
    {
        return color.opacity(0.8)
    }
    
    private var ledGlowColor: Color
    {
        return color.opacity(0.3)
    }
    
    var body: some View
    {
        ZStack
        {
            // LED glow background
            RoundedRectangle(cornerRadius: 4)
                .fill(ledGlowColor)
                .blur(radius: 1)
            
            // Main pad - rounded rectangle, taller than wide
            RoundedRectangle(cornerRadius: 4)
                .fill(padColor)
                .overlay(
                    RoundedRectangle(cornerRadius: 4)
                        .stroke(Color.white.opacity(0.3), lineWidth: 0.5)
                )
                .shadow(color: Color.black.opacity(0.3), radius: 1, x: 0, y: 0.5)
        }
        .aspectRatio(0.8, contentMode: .fit) // Slightly taller than wide
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

struct CoMuteView: View
{
    @ObservedObject var bridge: TheNonagonBridge
    
    var body: some View
    {
        VStack(spacing: 4)
        {
            // Group rows in pairs for visual grouping
            ForEach(0..<3, id: \.self) { groupIndex in
                VStack(spacing: 2)
                {
                    // First row of the group (even row index)
                    let evenRow = groupIndex * 2
                    HStack(spacing: 2)
                    {
                        ForEach(0..<7, id: \.self) { col in
                            if col < 6 {
                                ButtonView<CoMuteSkin>(
                                    logic: ButtonCellLogic(
                                        bridge: bridge,
                                        x: col,
                                        y: evenRow,
                                        gridHandle: GridHandle_LameJuisCoMute
                                    )
                                )
                            } else {
                                // Empty space for 7th column to align with StepSelector layout
                                Color.clear
                                    .aspectRatio(0.8, contentMode: .fit) // Match StepSelector aspect ratio
                            }
                        }
                    }
                    
                    // Second row of the group (odd row index)
                    let oddRow = groupIndex * 2 + 1
                    HStack(spacing: 2)
                    {
                        ForEach(0..<7, id: \.self) { col in
                            ButtonView<StepSelectorSkin>(
                                logic: ButtonCellLogic(
                                    bridge: bridge,
                                    x: col,
                                    y: oddRow,
                                    gridHandle: GridHandle_LameJuisCoMute
                                )
                            )
                        }
                    }
                }
                .padding(.bottom, groupIndex < 2 ? 4 : 0) // Add spacing between groups
            }
        }
    }
} 
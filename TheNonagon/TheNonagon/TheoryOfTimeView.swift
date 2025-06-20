import SwiftUI
import Combine

// Theory of Time Multiplier skin - wider short square with rounded edges
struct TheoryOfTimeMultiplierSkin: ButtonSkinView
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
            RoundedRectangle(cornerRadius: 6)
                .fill(ledGlowColor)
                .blur(radius: 1)
            
            // Main pad - wider short rectangle
            RoundedRectangle(cornerRadius: 6)
                .fill(padColor)
                .overlay(
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(Color.white.opacity(0.3), lineWidth: 0.5)
                )
                .shadow(color: Color.black.opacity(0.3), radius: 1, x: 0, y: 0.5)
        }
        .aspectRatio(0.8, contentMode: .fit) // Match consistent aspect ratio
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

// ReParent skin - square with rounded edges
struct ReParentSkin: ButtonSkinView
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
            
            // Main pad - square with rounded edges
            RoundedRectangle(cornerRadius: 4)
                .fill(padColor)
                .overlay(
                    RoundedRectangle(cornerRadius: 4)
                        .stroke(Color.white.opacity(0.3), lineWidth: 0.5)
                )
                .shadow(color: Color.black.opacity(0.3), radius: 1, x: 0, y: 0.5)
        }
        .aspectRatio(1.0, contentMode: .fit) // Square aspect ratio
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

// PingPong skin - looks like step selector
struct PingPongSkin: ButtonSkinView
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
        .aspectRatio(0.8, contentMode: .fit) // Match consistent aspect ratio
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

// TimeIndicator skin - similar to step selector
struct TimeIndicatorSkin: ButtonSkinView
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
        .aspectRatio(0.8, contentMode: .fit) // Match consistent aspect ratio
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

struct TheoryOfTimeView: View
{
    @ObservedObject var bridge: TheNonagonBridge
    
    var body: some View
    {
        VStack(spacing: 2)
        {
            // Rows 0-3: First 5 columns TheoryOfTimeMultiplierSkin, last 2 blank
            ForEach(0..<4, id: \.self) { row in
                HStack(spacing: 2)
                {
                    ForEach(0..<7, id: \.self) { col in
                        if col < 5
                        {
                            ButtonView<TheoryOfTimeMultiplierSkin>(
                                logic: ButtonCellLogic(
                                    bridge: bridge,
                                    x: col,
                                    y: row,
                                    gridHandle: GridHandle_TheoryOfTimeTopology
                                )
                            )
                        }
                        else
                        {
                            // Blank space
                            Color.clear
                                .aspectRatio(0.8, contentMode: .fit)
                        }
                    }
                }
            }
            
            // Row 4: First 4 columns ReParentSkin, next 3 blank
            HStack(spacing: 2)
            {
                ForEach(0..<7, id: \.self) { col in
                    if col < 4
                    {
                        ButtonView<ReParentSkin>(
                            logic: ButtonCellLogic(
                                bridge: bridge,
                                x: col,
                                y: 4,
                                gridHandle: GridHandle_TheoryOfTimeTopology
                            )
                        )
                    }
                    else
                    {
                        // Blank space
                        Color.clear
                            .aspectRatio(0.8, contentMode: .fit)
                    }
                }
            }
            
            // Row 5: First 5 columns PingPongSkin, last 2 blank
            HStack(spacing: 2)
            {
                ForEach(0..<7, id: \.self) { col in
                    if col < 5
                    {
                        ButtonView<PingPongSkin>(
                            logic: ButtonCellLogic(
                                bridge: bridge,
                                x: col,
                                // Skip row 5
                                y: 6,
                                gridHandle: GridHandle_TheoryOfTimeTopology
                            )
                        )
                    }
                    else
                    {
                        // Blank space
                        Color.clear
                            .aspectRatio(0.8, contentMode: .fit)
                    }
                }
            }
            
            // Row 6: All 7 columns TimeIndicatorSkin
            HStack(spacing: 2)
            {
                ForEach(0..<7, id: \.self) { col in
                    ButtonView<TimeIndicatorSkin>(
                        logic: ButtonCellLogic(
                            bridge: bridge,
                            x: col,
                            y: 7,
                            gridHandle: GridHandle_TheoryOfTimeTopology
                        )
                    )
                }
            }
        }
    }
} 
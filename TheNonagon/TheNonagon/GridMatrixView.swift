import SwiftUI
import Combine

// Matrix Time Indicator skin - square version of TimeIndicatorSkin for grid alignment
struct MatrixTimeIndicatorSkin: ButtonSkinView
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
            
            // Main pad - square with rounded edges (matching MatrixCellSkin)
            RoundedRectangle(cornerRadius: 4)
                .fill(padColor)
                .overlay(
                    RoundedRectangle(cornerRadius: 4)
                        .stroke(Color.white.opacity(0.3), lineWidth: 0.5)
                )
                .shadow(color: Color.black.opacity(0.3), radius: 1, x: 0, y: 0.5)
        }
        .aspectRatio(1.0, contentMode: .fit) // Square aspect ratio to match matrix cells
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

// Matrix Cell skin - square with rounded edges
struct MatrixCellSkin: ButtonSkinView
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

// Indicator Out skin - wider than it is tall
struct IndicatorOutSkin: ButtonSkinView
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
            
            // Main pad - wider than it is tall
            RoundedRectangle(cornerRadius: 4)
                .fill(padColor)
                .overlay(
                    RoundedRectangle(cornerRadius: 4)
                        .stroke(Color.white.opacity(0.3), lineWidth: 0.5)
                )
                .shadow(color: Color.black.opacity(0.3), radius: 1, x: 0, y: 0.5)
        }
        .aspectRatio(1.5, contentMode: .fit) // Wider than tall
        .scaleEffect(isPressed ? 0.9 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isPressed)
    }
}

// RHS Cell skin - square with rounded edges (similar to MatrixCellSkin)
struct RHSCellSkin: ButtonSkinView
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

// Interval Select skin - square with rounded edges
struct IntervalSelectSkin: ButtonSkinView
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

struct GridMatrixView: View
{
    @ObservedObject var bridge: TheNonagonBridge
    
    var body: some View
    {
        VStack(spacing: 2)
        {
            // First 6 rows: 6 Matrix cells + 1 indicator out + 7 RHS cells + 1 interval select
            ForEach(0..<6, id: \.self) { row in
                HStack(spacing: 2)
                {
                    // First 6 columns: Matrix cells
                    ForEach(0..<6, id: \.self) { col in
                        ButtonView<MatrixCellSkin>(
                            logic: ButtonCellLogic(
                                bridge: bridge,
                                x: col,
                                y: row,
                                gridHandle: GridHandle_LameJuisMatrix
                            )
                        )
                    }
                    
                    // 7th column: Indicator out
                    ButtonView<IndicatorOutSkin>(
                        logic: ButtonCellLogic(
                            bridge: bridge,
                            x: 6,
                            y: row,
                            gridHandle: GridHandle_LameJuisMatrix
                        )
                    )
                    
                    // 8th-14th columns: RHS cells
                    ForEach(0..<7, id: \.self) { col in
                        ButtonView<RHSCellSkin>(
                            logic: ButtonCellLogic(
                                bridge: bridge,
                                x: col,
                                y: row,
                                gridHandle: GridHandle_LameJuisRHS
                            )
                        )
                    }
                    
                    // 15th column: Interval select
                    ButtonView<IntervalSelectSkin>(
                        logic: ButtonCellLogic(
                            bridge: bridge,
                            x: 7,
                            y: row,
                            gridHandle: GridHandle_LameJuisRHS
                        )
                    )
                }
            }
            
            // Last row: 6 indicators from TheoryOfTime + blank space
            HStack(spacing: 2)
            {
                ForEach(0..<6, id: \.self) { col in
                    ButtonView<MatrixTimeIndicatorSkin>(
                        logic: ButtonCellLogic(
                            bridge: bridge,
                            x: col,
                            y: 6,
                            gridHandle: GridHandle_LameJuisMatrix
                        )
                    )
                }
                
                // Blank space with same width as IndicatorOutSkin for alignment
                Color.clear
                    .aspectRatio(1.5, contentMode: .fit) // Match IndicatorOutSkin aspect ratio
                
                // Additional blank spaces to align with the expanded rows above
                ForEach(0..<8, id: \.self) { _ in
                    Color.clear
                        .aspectRatio(1.0, contentMode: .fit) // Match square aspect ratio
                }
            }
        }
    }
} 
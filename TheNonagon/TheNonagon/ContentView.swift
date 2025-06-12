//
//  ContentView.swift
//  TheNonagon
//
//  Created by Joyo on 6/9/25.
//

import SwiftUI

struct ContentView: View {
    private static let x_sharedBridge = TheNonagonBridge()
    private let m_bridge = ContentView.x_sharedBridge
    @StateObject private var m_viewModel: ButtonGridViewModel
    @State private var m_displayLoopController: DisplayLoopController?
    
    init() {
        // Create the view model with the shared bridge instance
        _m_viewModel = StateObject(wrappedValue: ButtonGridViewModel(bridge: ContentView.x_sharedBridge))
    }
    
    var body: some View {
        ButtonGridView(viewModel: m_viewModel)
            .onAppear {
                m_displayLoopController = DisplayLoopController(bridge: m_bridge, buttonView: m_viewModel)
            }
    }
}

#Preview {
    ContentView()
}

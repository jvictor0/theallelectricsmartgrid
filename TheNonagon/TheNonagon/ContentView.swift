//
//  ContentView.swift
//  TheNonagon
//
//  Created by Joyo on 6/9/25.
//

import SwiftUI

struct ContentView: View {
    private let m_bridge = TheNonagonBridge()
    @StateObject private var m_viewModel = ButtonGridViewModel(bridge: TheNonagonBridge())
    @State private var m_displayLoopController: DisplayLoopController?
    
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

import SwiftUI
import Combine

// Enum for center view modes
enum CenterViewMode: CaseIterable {
    case TheoryOfTime
    case Grid
}

// View model for center view
class CenterViewModel: ObservableObject {
    @Published var mode: CenterViewMode = .TheoryOfTime
    @ObservedObject var bridge: TheNonagonBridge
    
    init(bridge: TheNonagonBridge) {
        self.bridge = bridge
    }
}

struct CenterView: View {
    @ObservedObject var viewModel: CenterViewModel
    
    var body: some View {
        VStack(spacing: 0) {
            // Main content area
            switch viewModel.mode {
            case .TheoryOfTime:
                HStack(spacing: 2) {
                    CoMuteView(bridge: viewModel.bridge)
                    TheoryOfTimeView(bridge: viewModel.bridge)
                }
            case .Grid:
                GridMatrixView(bridge: viewModel.bridge)
            }
        }
    }
} 
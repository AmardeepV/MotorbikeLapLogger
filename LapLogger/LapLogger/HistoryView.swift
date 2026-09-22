import SwiftUI

struct HistoryView: View {
    @ObservedObject var viewModel: LapLoggerViewModel

    var body: some View {
        NavigationStack {
            List {
                if viewModel.bluetooth.activityLog.isEmpty {
                    ContentUnavailableView("No activity yet", systemImage: "clock", description: Text("Connection events, ESP32 messages, and confirmed command writes will appear here."))
                        .listRowBackground(AppTheme.background)
                } else {
                    ForEach(viewModel.bluetooth.activityLog) { event in
                        ActivityRow(event: event).listRowBackground(AppTheme.card)
                    }
                }
            }
            .scrollContentBackground(.hidden)
            .background(AppTheme.background)
            .navigationTitle("Activity")
            .toolbar {
                if !viewModel.bluetooth.activityLog.isEmpty {
                    Button("Clear", role: .destructive) { viewModel.clearHistory() }
                }
            }
        }
    }
}

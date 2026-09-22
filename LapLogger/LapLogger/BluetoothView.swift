import SwiftUI

struct BluetoothView: View {
    @ObservedObject var viewModel: LapLoggerViewModel

    var body: some View {
        NavigationStack {
            List {
                Section("Connection") {
                    LabeledContent("Bluetooth", value: viewModel.bluetooth.availability.description)
                    LabeledContent("State", value: viewModel.bluetooth.connectionState.description)
                    if let name = viewModel.bluetooth.connectedDeviceName { LabeledContent("Connected device", value: name) }
                    if viewModel.bluetooth.isReconnecting { Label("Reconnecting when the logger is discovered", systemImage: "arrow.triangle.2.circlepath").foregroundStyle(AppTheme.warning) }
                    if let error = viewModel.bluetooth.errorMessage { Label(error, systemImage: "exclamationmark.triangle.fill").foregroundStyle(AppTheme.danger) }
                }
                Section("Logger") {
                    if viewModel.isConnected {
                        Button("Disconnect", role: .destructive, action: viewModel.disconnect)
                    } else {
                        Button(viewModel.isScanning ? "Scanning…" : "Scan for loggers", action: viewModel.scan)
                            .disabled(viewModel.bluetooth.availability != .ready || viewModel.isScanning)
                    }
                }
                Section("Discovered devices") {
                    if viewModel.bluetooth.discoveredDevices.isEmpty {
                        Text(viewModel.isScanning ? "Searching for nearby lap loggers…" : "Start a scan to find the ESP32 logger.").foregroundStyle(.secondary)
                    } else {
                        ForEach(viewModel.bluetooth.discoveredDevices) { device in
                            HStack {
                                VStack(alignment: .leading) { Text(device.name); Text(device.id.uuidString).font(.caption2).foregroundStyle(.secondary) }
                                Spacer()
                                Button("Connect") { viewModel.connect(to: device) }.buttonStyle(.borderedProminent)
                            }
                        }
                    }
                }
            }
            .scrollContentBackground(.hidden).background(AppTheme.background)
            .navigationTitle("Bluetooth")
        }
    }
}

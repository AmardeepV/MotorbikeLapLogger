import SwiftUI

struct TelemetryView: View {
    let viewModel: LapLoggerViewModel

    private let columns = [GridItem(.flexible()), GridItem(.flexible())]

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    Text("Telemetry")
                        .font(.largeTitle.weight(.bold)).foregroundStyle(.white)
                    Text(telemetryDescription).font(.subheadline).foregroundStyle(.secondary)
                    LazyVGrid(columns: columns, spacing: 12) {
                        MetricCard(title: "Lean angle", symbol: "angle", value: currentLean, detail: availabilityDetail)
                        MetricCard(title: "Speed", symbol: "gauge.with.dots.needle.50percent", value: "Unavailable", detail: "BLE data not available")
                        MetricCard(title: "Lap time", symbol: "stopwatch", value: "Unavailable", detail: "BLE data not available")
                        MetricCard(title: "Maximum left", symbol: "arrow.turn.up.left", value: maximumLeft, detail: availabilityDetail)
                        MetricCard(title: "Maximum right", symbol: "arrow.turn.up.right", value: maximumRight, detail: availabilityDetail)
                        MetricCard(title: "Current lap", symbol: "flag.checkered", value: currentLapValue, detail: currentLapDetail)
                        MetricCard(title: "GPS", symbol: "location", value: "Unavailable", detail: "BLE data not available")
                        MetricCard(title: "Session", symbol: "road.lanes", value: viewModel.sessionNumber.map(String.init) ?? "Unavailable", detail: "Reported by logger status")
                    }
                    DashboardCard {
                        Label("Lean values are received from the ESP32. Speed, GPS, and lap-time telemetry are not transmitted.", systemImage: "info.circle")
                            .font(.footnote).foregroundStyle(.secondary)
                    }
                }
                .padding()
            }
            .background(AppTheme.background)
        }
    }

    private var isAvailable: Bool { viewModel.bluetooth.telemetryIsAvailable }
    private var telemetry: LeanTelemetry? { viewModel.bluetooth.telemetry }
    private var currentLean: String { isAvailable && telemetry != nil ? String(format: "%.1f°", telemetry!.currentLeanDegrees) : "Unavailable" }
    private var maximumLeft: String { isAvailable && telemetry != nil ? String(format: "%.1f°", telemetry!.maximumLeftLeanDegrees) : "Unavailable" }
    private var maximumRight: String { isAvailable && telemetry != nil ? String(format: "%.1f°", telemetry!.maximumRightLeanDegrees) : "Unavailable" }
    private var availabilityDetail: String { isAvailable ? "ESP32 · updated now" : "No recent BLE packet" }
    private var telemetryDescription: String { isAvailable ? "Receiving live lean-angle telemetry from the ESP32." : "Lean telemetry is unavailable or stale. Connect and check the logger." }
    private var currentLapValue: String { viewModel.currentLap > 0 ? "\(viewModel.currentLap)" : "Unavailable" }
    private var currentLapDetail: String { viewModel.currentLap > 0 ? "Reported by logger status" : "Not reported by logger" }
}

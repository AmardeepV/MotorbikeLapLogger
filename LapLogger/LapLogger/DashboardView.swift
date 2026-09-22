import SwiftUI

struct DashboardView: View {
    @ObservedObject var viewModel: LapLoggerViewModel
    @State private var showStopConfirmation = false

    var body: some View {
        NavigationStack {
            GeometryReader { proxy in
                let landscape = proxy.size.width > proxy.size.height
                Group {
                    if landscape {
                        HStack(spacing: 12) {
                            LeanPanel(side: .left, telemetry: viewModel.bluetooth.telemetry, available: viewModel.bluetooth.telemetryIsAvailable)
                            centerConsole
                            LeanPanel(side: .right, telemetry: viewModel.bluetooth.telemetry, available: viewModel.bluetooth.telemetryIsAvailable)
                        }
                    } else {
                        VStack(spacing: 12) {
                            centerConsole
                            HStack(spacing: 12) {
                                LeanPanel(side: .left, telemetry: viewModel.bluetooth.telemetry, available: viewModel.bluetooth.telemetryIsAvailable)
                                LeanPanel(side: .right, telemetry: viewModel.bluetooth.telemetry, available: viewModel.bluetooth.telemetryIsAvailable)
                            }
                        }
                    }
                }
                .padding()
            }
            .background(AppTheme.background)
            .navigationTitle("LAP LOGGER")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .topBarTrailing) { ConnectionPill(connected: viewModel.isConnected, title: viewModel.isConnected ? "CONNECTED" : "OFFLINE") } }
            .alert("Stop logging?", isPresented: $showStopConfirmation) {
                Button("Cancel", role: .cancel) { }
                Button("STOP", role: .destructive) { viewModel.stop() }
            } message: { Text("This sends STOP to the connected lap logger.") }
        }
    }

    private var centerConsole: some View {
        VStack(spacing: 12) {
            DashboardCard {
                VStack(alignment: .leading, spacing: 9) {
                    HStack {
                        VStack(alignment: .leading, spacing: 3) {
                            Text("LOGGER STATUS").font(.caption2.weight(.bold)).foregroundStyle(.secondary)
                            Text(primaryState).font(.title3.weight(.bold)).foregroundStyle(stateColor)
                        }
                        Spacer()
                        Image(systemName: stateSymbol).font(.title2).foregroundStyle(stateColor)
                    }
                    Divider().overlay(.white.opacity(0.12))
                    HStack {
                        detail("SESSION", viewModel.sessionNumber.map(String.init) ?? "—")
                        Spacer()
                        detail("LAP", viewModel.currentLap > 0 ? String(viewModel.currentLap) : "—")
                        Spacer()
                        detail("CAL", viewModel.calibrationState == .inProgress ? "ACTIVE" : "READY")
                    }
                }
            }
            HStack(spacing: 8) {
                controlButton("CAL", symbol: "scope", color: AppTheme.accent, enabled: viewModel.canCalibrate, action: viewModel.calibrate)
                controlButton("LAP", symbol: "flag.checkered", color: AppTheme.healthy, enabled: viewModel.canSendLap, action: viewModel.lap)
                controlButton("STOP", symbol: "stop.fill", color: AppTheme.danger, enabled: viewModel.canStop) { showStopConfirmation = true }
            }
            DashboardCard {
                VStack(alignment: .leading, spacing: 4) {
                    Text("LATEST ESP32 STATUS").font(.caption2.weight(.bold)).foregroundStyle(.secondary)
                    Text(viewModel.bluetooth.latestStatusMessage).font(.subheadline.weight(.semibold)).foregroundStyle(.white).lineLimit(2)
                    Text(telemetryDescription).font(.caption).foregroundStyle(viewModel.bluetooth.telemetryIsAvailable ? AppTheme.healthy : AppTheme.warning)
                }
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .center)
    }

    private func detail(_ title: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title).font(.caption2.weight(.bold)).foregroundStyle(.secondary)
            Text(value).font(.subheadline.weight(.semibold)).foregroundStyle(.white)
        }
    }

    private func controlButton(_ title: String, symbol: String, color: Color, enabled: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(spacing: 5) { Image(systemName: symbol); Text(title).font(.caption2.weight(.bold)) }
                .frame(maxWidth: .infinity).frame(height: 58)
        }
        .buttonStyle(.plain).foregroundStyle(enabled ? .white : .secondary)
        .background(enabled ? color.opacity(0.25) : AppTheme.cardSecondary, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).stroke(enabled ? color.opacity(0.5) : .clear))
        .disabled(!enabled)
        .accessibilityHint("Sends \(title == "CAL" ? "CALIBRATE" : title) to the connected lap logger")
    }

    private var primaryState: String {
        if !viewModel.isConnected { return "CONNECTION LOST" }
        if viewModel.calibrationState == .inProgress { return "CALIBRATING" }
        return viewModel.sessionState.rawValue.uppercased()
    }
    private var stateColor: Color { !viewModel.isConnected ? AppTheme.warning : viewModel.calibrationState == .inProgress ? AppTheme.accent : viewModel.sessionState == .logging ? AppTheme.healthy : .white }
    private var stateSymbol: String { !viewModel.isConnected ? "bolt.horizontal.circle" : viewModel.calibrationState == .inProgress ? "scope" : viewModel.sessionState == .logging ? "flag.checkered" : "circle" }
    private var telemetryDescription: String { viewModel.bluetooth.telemetryIsAvailable ? "LIVE LEAN TELEMETRY · 10 HZ" : "LEAN TELEMETRY UNAVAILABLE OR STALE" }
}

private enum LeanSide { case left, right }

private struct LeanPanel: View {
    let side: LeanSide
    let telemetry: LeanTelemetry?
    let available: Bool

    var body: some View {
        DashboardCard {
            VStack(alignment: side == .left ? .leading : .trailing, spacing: 8) {
                Text(side == .left ? "LEFT" : "RIGHT").font(.headline.weight(.bold)).foregroundStyle(accent)
                Spacer(minLength: 6)
                Text(currentDisplay)
                    .font(.system(size: 72, weight: .bold, design: .rounded))
                    .minimumScaleFactor(0.45).lineLimit(1).foregroundStyle(.white)
                Text(available ? currentCaption : "TELEMETRY UNAVAILABLE")
                    .font(.caption2.weight(.bold)).foregroundStyle(available ? .secondary : AppTheme.warning)
                Spacer(minLength: 6)
                Divider().overlay(.white.opacity(0.12))
                Text("MAX").font(.caption.weight(.bold)).foregroundStyle(.secondary)
                Text(maximumDisplay).font(.title2.weight(.bold)).foregroundStyle(.white)
            }
        }
    }

    private var currentDisplay: String {
        guard available, let value = telemetry?.currentLeanDegrees else {
            return "—"
        }

        switch side {
        case .left:
            return value > 0 ? String(format: "%.1f°", abs(value)) : "—"

        case .right:
            return value < 0 ? String(format: "%.1f°", abs(value)) : "—"
        }
    }
    private var maximumDisplay: String {
        guard available, let telemetry else {
            return "—"
        }

        let value = side == .left
            ? telemetry.maximumRightLeanDegrees
            : telemetry.maximumLeftLeanDegrees

        return String(format: "%.1f°", abs(value))
    }
    private var currentCaption: String {
        "CURRENT LEAN"
    }
    private var accent: Color { side == .left ? AppTheme.accent : AppTheme.healthy }
}

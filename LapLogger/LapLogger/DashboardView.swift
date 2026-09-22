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
                        landscapeDashboard
                    } else {
                        portraitDashboard
                    }
                }
                .padding(landscape ? 10 : 16)
            }
            .background(AppTheme.background)
            //.navigationTitle("LAP LOGGER")
            .navigationBarTitleDisplayMode(.inline)
//            .toolbar {
//                ToolbarItem(placement: .topBarTrailing) {
//                    ConnectionPill(
//                        connected: viewModel.isConnected,
//                        title: viewModel.isConnected ? "CONNECTED" : "OFFLINE"
//                    )
//                }
//            }
            .alert("Stop logging?", isPresented: $showStopConfirmation) {
                Button("Cancel", role: .cancel) { }
                Button("STOP", role: .destructive) { viewModel.stop() }
            } message: {
                Text("This sends STOP to the connected lap logger.")
            }
        }
    }

//    private var landscapeDashboard: some View {
//        VStack(spacing: 10) {
//            HStack(spacing: 10) {
//                landscapeStatusCard
//                    .frame(maxWidth: .infinity)
//
//                HStack(spacing: 8) {
//                    controlButton(
//                        "CAL",
//                        symbol: "scope",
//                        color: AppTheme.accent,
//                        enabled: viewModel.canCalibrate,
//                        action: viewModel.calibrate
//                    )
//                    .frame(width: 78)
//
//                    controlButton(
//                        "LAP",
//                        symbol: "flag.checkered",
//                        color: AppTheme.healthy,
//                        enabled: viewModel.canSendLap,
//                        action: viewModel.lap
//                    )
//                    .frame(width: 78)
//
//                    controlButton(
//                        "STOP",
//                        symbol: "stop.fill",
//                        color: AppTheme.danger,
//                        enabled: viewModel.canStop,
//                        action: { showStopConfirmation = true }
//                    )
//                    .frame(width: 78)
//                }
//            }
//            .frame(height: 104)
//
//            HStack(spacing: 12) {
//                LeanPanel(
//                    side: .left,
//                    telemetry: viewModel.bluetooth.telemetry,
//                    available: viewModel.bluetooth.telemetryIsAvailable
//                )
//
//                LeanPanel(
//                    side: .right,
//                    telemetry: viewModel.bluetooth.telemetry,
//                    available: viewModel.bluetooth.telemetryIsAvailable
//                )
//            }
//            .frame(maxHeight: .infinity)
//
//            DashboardCard {
//                HStack(spacing: 10) {
//                    Image(systemName: "antenna.radiowaves.left.and.right")
//                        .foregroundStyle(viewModel.bluetooth.telemetryIsAvailable ? AppTheme.healthy : AppTheme.warning)
//                    Text(viewModel.bluetooth.latestStatusMessage)
//                        .font(.caption.weight(.semibold))
//                        .foregroundStyle(.white)
//                        .lineLimit(1)
//                    Spacer(minLength: 4)
////                    Text(telemetryDescription)
////                        .font(.caption2.weight(.bold))
////                        .foregroundStyle(viewModel.bluetooth.telemetryIsAvailable ? AppTheme.healthy : AppTheme.warning)
////                        .lineLimit(1)
//                }
//            }
//            .frame(height: 42)
//        }
//    }
    
    private var landscapeDashboard: some View {
        VStack(spacing: 10) {
            // Top status bar
            landscapeStatusCard
                .frame(height: 82)

            // Main three-column dashboard
            HStack(spacing: 12) {
                // LEFT LEAN PANEL
                LeanPanel(
                    side: .left,
                    telemetry: viewModel.bluetooth.telemetry,
                    available: viewModel.bluetooth.telemetryIsAvailable
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity)

                // CENTER CONTROLS
                VStack(spacing: 14) {
                    controlButton(
                        "CAL",
                        symbol: "scope",
                        color: AppTheme.accent,
                        enabled: viewModel.canCalibrate,
                        action: viewModel.calibrate
                    )

                    controlButton(
                        "LAP",
                        symbol: "flag.checkered",
                        color: AppTheme.healthy,
                        enabled: viewModel.canSendLap,
                        action: viewModel.lap
                    )

                    controlButton(
                        "STOP",
                        symbol: "stop.fill",
                        color: AppTheme.danger,
                        enabled: viewModel.canStop,
                        action: { showStopConfirmation = true }
                    )
                }
                .frame(width: 92)
                .frame(maxHeight: .infinity, alignment: .center)

                // RIGHT LEAN PANEL
                LeanPanel(
                    side: .right,
                    telemetry: viewModel.bluetooth.telemetry,
                    available: viewModel.bluetooth.telemetryIsAvailable
                )
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .frame(maxHeight: .infinity)
        }
    }
    
    private var landscapeStatusCard: some View {
        DashboardCard {
            HStack(spacing: 18) {
                // LOGGER STATUS section
                VStack(alignment: .leading, spacing: 2) {
                    Text("LOGGER STATUS")
                        .font(.caption2.weight(.bold))
                        .foregroundStyle(.secondary)

                    Text(primaryState)
                        .font(.title3.weight(.bold))
                        .foregroundStyle(stateColor)
                }

                // SESSION
                detail(
                    "SESSION",
                    viewModel.sessionNumber.map(String.init) ?? "—"
                )

                // LAP
                detail(
                    "LAP",
                    viewModel.currentLap > 0 ? String(viewModel.currentLap) : "—"
                )

                // CAL
                detail(
                    "CAL",
                    viewModel.calibrationState == .inProgress ? "ACTIVE" : "READY"
                )

                Spacer()

//                Image(systemName: stateSymbol)
//                    .font(.title3)
//                    .foregroundStyle(stateColor)
            }
        }
    }

    private var portraitDashboard: some View {
        VStack(spacing: 12) {
            centerConsole

            HStack(spacing: 12) {
                LeanPanel(
                    side: .left,
                    telemetry: viewModel.bluetooth.telemetry,
                    available: viewModel.bluetooth.telemetryIsAvailable
                )
                LeanPanel(
                    side: .right,
                    telemetry: viewModel.bluetooth.telemetry,
                    available: viewModel.bluetooth.telemetryIsAvailable
                )
            }
        }
    }

    private var centerConsole: some View {
        HStack(spacing: 18) {
            // LOGGER STATUS section
            VStack(alignment: .leading, spacing: 2) {
                Text("LOGGER STATUS")
                    .font(.caption2.weight(.bold))
                    .foregroundStyle(.secondary)

                Text(primaryState)
                    .font(.title3.weight(.bold))
                    .foregroundStyle(stateColor)
            }

            // SESSION
            detail(
                "SESSION",
                viewModel.sessionNumber.map(String.init) ?? "—"
            )

            // LAP
            detail(
                "LAP",
                viewModel.currentLap > 0 ? String(viewModel.currentLap) : "—"
            )

            // CAL
            detail(
                "CAL",
                viewModel.calibrationState == .inProgress ? "ACTIVE" : "READY"
            )

            Spacer()

//            Image(systemName: stateSymbol)
//                .font(.title3)
//                .foregroundStyle(stateColor)
        }
        .frame(maxWidth: .infinity, alignment: .center)
    }

    private func detail(_ title: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
                .font(.caption2.weight(.bold))
                .foregroundStyle(.secondary)
            Text(value)
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(.white)
        }
    }

    private func controlButton(
        _ title: String,
        symbol: String,
        color: Color,
        enabled: Bool,
        action: @escaping () -> Void
    ) -> some View {
        Button(action: action) {
            VStack(spacing: 5) {
                Image(systemName: symbol)
                Text(title).font(.caption2.weight(.bold))
            }
            .frame(maxWidth: .infinity)
            .frame(height: 58)
        }
        .buttonStyle(.plain)
        .foregroundStyle(enabled ? .white : .secondary)
        .background(
            enabled ? color.opacity(0.25) : AppTheme.cardSecondary,
            in: RoundedRectangle(cornerRadius: 14, style: .continuous)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(enabled ? color.opacity(0.5) : .clear)
        )
        .disabled(!enabled)
        .accessibilityHint("Sends \(title == "CAL" ? "CALIBRATE" : title) to the connected lap logger")
    }

    private var primaryState: String {
        if !viewModel.isConnected { return "CONNECTION LOST" }
        if viewModel.calibrationState == .inProgress { return "CALIBRATING" }
        return viewModel.sessionState.rawValue.uppercased()
    }

    private var stateColor: Color {
        !viewModel.isConnected
            ? AppTheme.warning
            : viewModel.calibrationState == .inProgress
                ? AppTheme.accent
                : viewModel.sessionState == .logging ? AppTheme.healthy : .white
    }

//    private var stateSymbol: String {
//        !viewModel.isConnected
//            ? "bolt.horizontal.circle"
//            : viewModel.calibrationState == .inProgress
//                ? "scope"
//                : viewModel.sessionState == .logging ? "flag.checkered" : "circle"
//    }
//
//    private var telemetryDescription: String {
//        viewModel.bluetooth.telemetryIsAvailable
//            ? "LIVE LEAN TELEMETRY · 10 HZ"
//            : "LEAN TELEMETRY UNAVAILABLE OR STALE"
//    }
}

private enum LeanSide { case left, right }

private struct LeanPanel: View {
    let side: LeanSide
    let telemetry: LeanTelemetry?
    let available: Bool

    var body: some View {
        DashboardCard {
            VStack(alignment: side == .left ? .leading : .trailing, spacing: 8) {
                Text(side == .left ? "LEFT" : "RIGHT")
                    .font(.headline.weight(.bold))
                    .foregroundStyle(accent)

                Spacer(minLength: 4)

                Text(currentDisplay)
                    .font(.system(size: 94, weight: .bold, design: .rounded))
                    .minimumScaleFactor(0.4)
                    .lineLimit(1)
                    .foregroundStyle(.white)

                Text(available ? currentCaption : "TELEMETRY UNAVAILABLE")
                    .font(.caption2.weight(.bold))
                    .foregroundStyle(available ? .secondary : AppTheme.warning)

                Spacer(minLength: 4)

                Divider().overlay(.white.opacity(0.12))

                HStack {
                    if side == .left { Spacer(minLength: 0) }
                    VStack(alignment: side == .left ? .leading : .trailing, spacing: 2) {
                        Text("MAX")
                            .font(.caption.weight(.bold))
                            .foregroundStyle(.secondary)
                        Text(maximumDisplay)
                            .font(.system(size: 24, weight: .semibold, design: .rounded))
                            .foregroundStyle(.white)
                    }
                    if side == .right { Spacer(minLength: 0) }
                }
            }
            .frame(maxHeight: .infinity, alignment: .top)
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

    private var currentCaption: String { "CURRENT LEAN" }
    private var accent: Color { side == .left ? AppTheme.accent : AppTheme.healthy }
}

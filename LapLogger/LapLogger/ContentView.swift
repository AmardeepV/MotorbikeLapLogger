//
//  ContentView.swift
//  LapLogger
//
//  Created by Amardeep Verma on 21.09.26.
//


import SwiftUI

struct ContentView: View {
    @StateObject private var viewModel = LapLoggerViewModel()
    @State private var selectedPage: AppPage = .dashboard
    @State private var showMenu = false
    

    var body: some View {
        ZStack(alignment: .topTrailing) {
            selectedView

            Button {
                showMenu = true
            } label: {
                Image(systemName: "line.3.horizontal")
                    .font(.system(size: 22, weight: .semibold))
                    .foregroundStyle(.white)
                    .frame(width: 48, height: 68)
                    .background(
                        AppTheme.cardSecondary,
                        in: RoundedRectangle(
                            cornerRadius: 14,
                            style: .continuous
                        )
                    )
            }

            .padding(.trailing, 16)
            .padding(.top, 10)
            .zIndex(100)
        }
        .tint(AppTheme.accent)
        .preferredColorScheme(.dark)
        .background(AppTheme.background)
        .sheet(isPresented: $showMenu) {
            NavigationStack {
                List {
                    ForEach(AppPage.allCases) { page in
                        Button {
                            selectedPage = page
                            showMenu = false
                        } label: {
                            Label(page.title, systemImage: page.icon)
                        }
                    }
                }
                .navigationTitle("Navigation")
                .navigationBarTitleDisplayMode(.inline)
            }
            .presentationDetents([.medium])
        }
    }

    @ViewBuilder
    private var selectedView: some View {
        switch selectedPage {
        case .dashboard:
            DashboardView(viewModel: viewModel)

        case .telemetry:
            TelemetryView(viewModel: viewModel)

        case .activity:
            HistoryView(viewModel: viewModel)

        case .bluetooth:
            BluetoothView(viewModel: viewModel)

        case .settings:
            SettingsView()
        }
    }
}

private enum AppPage: String, CaseIterable, Identifiable {
    case dashboard
    case telemetry
    case activity
    case bluetooth
    case settings

    var id: Self { self }

    var title: String {
        switch self {
        case .dashboard:
            return "Dashboard"
        case .telemetry:
            return "Telemetry"
        case .activity:
            return "Activity"
        case .bluetooth:
            return "Bluetooth"
        case .settings:
            return "Settings"
        }
    }

    var icon: String {
        switch self {
        case .dashboard:
            return "gauge.with.dots.needle.50percent"
        case .telemetry:
            return "chart.xyaxis.line"
        case .activity:
            return "clock.arrow.circlepath"
        case .bluetooth:
            return "antenna.radiowaves.left.and.right"
        case .settings:
            return "gearshape"
        }
    }
}

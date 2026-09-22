//
//  ContentView.swift
//  LapLogger
//
//  Created by Amardeep Verma on 21.09.26.
//

import SwiftUI

struct ContentView: View {
    @StateObject private var viewModel = LapLoggerViewModel()

    var body: some View {
        TabView {
            DashboardView(viewModel: viewModel)
                .tabItem { Label("Dashboard", systemImage: "gauge.with.dots.needle.50percent") }
            TelemetryView(viewModel: viewModel)
                .tabItem { Label("Telemetry", systemImage: "chart.xyaxis.line") }
            HistoryView(viewModel: viewModel)
                .tabItem { Label("Activity", systemImage: "clock.arrow.circlepath") }
            BluetoothView(viewModel: viewModel)
                .tabItem { Label("Bluetooth", systemImage: "bluetooth") }
            SettingsView()
                .tabItem { Label("Settings", systemImage: "gearshape") }
        }
        .tint(AppTheme.accent)
        .preferredColorScheme(.dark)
        .background(AppTheme.background)
    }
}

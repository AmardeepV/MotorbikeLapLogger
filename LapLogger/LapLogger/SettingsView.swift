import SwiftUI

struct SettingsView: View {
    @AppStorage("clearHistoryOnLaunch") private var clearHistoryOnLaunch = false

    var body: some View {
        NavigationStack {
            List {
                Section("History") { Toggle("Clear activity at app launch", isOn: $clearHistoryOnLaunch) }
                Section("Bluetooth protocol") {
                    LabeledContent("Service", value: "6e400001…cca9e")
                    LabeledContent("Commands", value: "LAP · STOP · CALIBRATE")
                    LabeledContent("Lean telemetry", value: "6e400004…cca9e · 10 Hz")
                    Text("Telemetry packets use T1: timestamp, signed lean angle, left maximum, right maximum, and logging state. Speed, GPS, lap time, and SD-card downloads are not available.").font(.footnote).foregroundStyle(.secondary)
                }
                Section("About") { LabeledContent("Version", value: versionText) }
            }
            .scrollContentBackground(.hidden).background(AppTheme.background)
            .navigationTitle("Settings")
        }
    }

    private var versionText: String {
        let version = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "1.0"
        let build = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "1"
        return "\(version) (\(build))"
    }
}

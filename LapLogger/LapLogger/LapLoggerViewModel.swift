	import Combine
import Foundation

enum LoggerSessionState: String { case idle = "Idle", logging = "Logging", stopped = "Stopped" }
enum CalibrationState: String { case idle = "Not calibrating", inProgress = "Calibration in progress" }

@MainActor
final class LapLoggerViewModel: ObservableObject {
    let bluetooth = BluetoothManager()
    @Published private(set) var sessionState: LoggerSessionState = .idle
    @Published private(set) var calibrationState: CalibrationState = .idle
    @Published private(set) var currentLap = 0
    @Published private(set) var sessionNumber: Int?
    private var cancellables = Set<AnyCancellable>()

    init() {
        if UserDefaults.standard.bool(forKey: "clearHistoryOnLaunch") {
            bluetooth.clearActivityLog()
        }
        bluetooth.$calibrationProgress.sink { [weak self] progress in
            guard self?.bluetooth.isNiclaLive == true else { return }
            self?.calibrationState = progress.active ? .inProgress : .idle
        }.store(in: &cancellables)
        bluetooth.$liveSession.sink { [weak self] session in
            guard let self, self.bluetooth.isNiclaLive else { return }
            self.sessionState = session.map { $0.active ? .logging : ($0.number == 0 ? .idle : .stopped) } ?? .idle
            self.currentLap = Int(session?.lap ?? 0)
            self.sessionNumber = session.flatMap { $0.number == 0 ? nil : Int($0.number) }
        }.store(in: &cancellables)
        bluetooth.objectWillChange.sink { [weak self] _ in self?.objectWillChange.send() }.store(in: &cancellables)
        bluetooth.$latestStatusMessage.dropFirst().sink { [weak self] in self?.process(status: $0) }.store(in: &cancellables)
    }

    var isConnected: Bool { bluetooth.isConnected }
    var isScanning: Bool { bluetooth.isScanning }
    var canCalibrate: Bool { isConnected && calibrationState == .idle && sessionState != .logging }
    var canSendLap: Bool { isConnected && calibrationState == .idle }
    var canStop: Bool { isConnected && sessionState == .logging }
    func scan() { bluetooth.scanForDevices() }
    func connect(to device: BluetoothDevice) { bluetooth.connect(to: device) }
    func disconnect() { bluetooth.disconnect() }
    func calibrate() { bluetooth.send(command: "CALIBRATE") }
    func lap() { bluetooth.send(command: "LAP") }
    func stop() { bluetooth.send(command: "STOP") }

    func clearHistory() { bluetooth.clearActivityLog() }

    private func process(status: String) {
        let parts = status.split(separator: ",", maxSplits: 1).map(String.init)
        guard let kind = parts.first else { return }
        switch kind {
        case "CALIBRATION_STARTED": if !bluetooth.isNiclaLive { calibrationState = .inProgress }
        case "CALIBRATION_TIMEOUT", "READY_TO_CALIBRATE", "SENSOR_FAULT", "CALIBRATION_COMPLETE", "CALIBRATION_REJECTED_LOGGING", "CALIBRATION_REJECTED_ALREADY_ACTIVE": if !bluetooth.isNiclaLive { calibrationState = .idle }
        case "SESSION_STARTED":
            sessionState = .logging
            sessionNumber = parts.count == 2 ? Int(parts[1]) : nil
        case "LAP_STARTED": currentLap = parts.count == 2 ? Int(parts[1]) ?? currentLap : currentLap
        case "SESSION_STOPPED": sessionState = .stopped; currentLap = 0
        default: break
        }
    }
}

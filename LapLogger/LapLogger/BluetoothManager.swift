import CoreBluetooth
import Combine
import Foundation

enum BluetoothAvailability: Equatable {
    case unknown
    case ready
    case unauthorized
    case unavailable(String)

    var description: String {
        switch self {
        case .unknown: "Checking Bluetooth…"
        case .ready: "Bluetooth ready"
        case .unauthorized: "Bluetooth permission is required"
        case .unavailable(let message): message
        }
    }
}

enum BluetoothConnectionState: Equatable {
    case disconnected
    case scanning
    case connecting
    case discovering
    case connected
    case disconnecting
    case failed(String)

    var description: String {
        switch self {
        case .disconnected: "Disconnected"
        case .scanning: "Scanning nearby loggers…"
        case .connecting: "Connecting…"
        case .discovering: "Preparing connection…"
        case .connected: "Connected"
        case .disconnecting: "Disconnecting…"
        case .failed(let message): "Connection failed: \(message)"
        }
    }
}

struct BluetoothDevice: Identifiable, Equatable {
    let id: UUID
    let name: String
}

/// Version 1 packet: T1,<millis>,<signed-degrees>,<left-max-degrees>,<right-max-degrees>,<logging>.
struct LeanTelemetry: Equatable {
    let sourceTimestampMilliseconds: UInt32
    let currentLeanDegrees: Float
    let maximumLeftLeanDegrees: Float
    let maximumRightLeanDegrees: Float
    let isLogging: Bool
    let receivedAt: Date

    var isStale: Bool { Date.now.timeIntervalSince(receivedAt) > 2 }
}

enum ActivityEventKind: String {
    case status
    case command
    case connection
    case error

    var symbolName: String {
        switch self {
        case .status: "antenna.radiowaves.left.and.right"
        case .command: "arrow.up.circle.fill"
        case .connection: "link.circle.fill"
        case .error: "exclamationmark.triangle.fill"
        }
    }
}

struct ActivityEvent: Identifiable {
    let id = UUID()
    let date: Date
    let kind: ActivityEventKind
    let message: String
}

@MainActor
final class BluetoothManager: NSObject, ObservableObject {
    static let serviceUUID = CBUUID(string: "6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    static let commandUUID = CBUUID(string: "6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    static let statusUUID = CBUUID(string: "6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    static let telemetryUUID = CBUUID(string: "6e400004-b5a3-f393-e0a9-e50e24dcca9e")

    static let motionUUID = CBUUID(string: "6e400005-b5a3-f393-e0a9-e50e24dcca9e")
    static let ratesUUID = CBUUID(string: "6e400006-b5a3-f393-e0a9-e50e24dcca9e")
    @Published private(set) var liveMotion: LiveMotion?
    @Published private(set) var liveRates: LiveRates?
    @Published private(set) var isNiclaLive = false
    @Published private(set) var missedLivePackets: UInt64 = 0
    private var motionOrder = LivePacketOrder()
    private var ratesOrder = LivePacketOrder()
    @Published private(set) var liveSession: LiveSession?
    private var sessionOrder = LivePacketOrder()
    static let sessionUUID = CBUUID(string: "6e400007-b5a3-f393-e0a9-e50e24dcca9e")
    private var calibrationRequestAt: TimeInterval?
    private var awaitingCalibrationStart = false
    @Published private(set) var calibrationProgress = LiveCalibrationProgress()
    private var displayJoin = LiveDisplayJoin()
    private var pendingCommandAt: TimeInterval?

    @Published private(set) var availability: BluetoothAvailability = .unknown
    @Published private(set) var connectionState: BluetoothConnectionState = .disconnected
    @Published private(set) var discoveredDevices: [BluetoothDevice] = []
    @Published private(set) var latestStatusMessage = "No status received yet"
    @Published private(set) var activityLog: [ActivityEvent] = []
    @Published private(set) var errorMessage: String?
    @Published private(set) var lastConfirmedCommand: String?
    @Published private(set) var telemetry: LeanTelemetry?
    @Published private(set) var telemetryHeartbeat = Date.now

    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var commandCharacteristic: CBCharacteristic?
    private var statusCharacteristic: CBCharacteristic?
    private var telemetryCharacteristic: CBCharacteristic?
    private var wantsConnection = false
    private let savedPeripheralIDKey = "savedPeripheralID"
    private var pendingCommand: String?
    private var telemetryRefreshTimer: Timer?
    private var lastTelemetryErrorAt: Date?

    var isConnected: Bool { connectionState == .connected }
    var isScanning: Bool { centralManager?.isScanning == true }
    var connectedDeviceName: String? { isConnected ? (discoveredDevices.first(where: { $0.id == peripheral?.identifier })?.name ?? peripheral?.name) : nil }
    var isReconnecting: Bool { wantsConnection && connectionState == .scanning }
    var telemetryIsAvailable: Bool {
        isConnected && (isNiclaLive ? displayJoin.isFresh() && telemetry != nil : telemetry?.isStale == false)
    }
    var motionIsAvailable: Bool { isConnected && liveMotion?.isFresh == true && liveMotion?.motionState == .valid }
    var ratesAreAvailable: Bool {
        motionIsAvailable && liveRates?.isFresh == true && liveRates?.sequence == liveMotion?.sequence &&
        liveRates?.sourceMilliseconds == liveMotion?.sourceMilliseconds && liveRates?.angularRates != nil
    }
    var liveStateDescription: String {
        guard isConnected else { return "Disconnected" }
        guard let packet = liveMotion, packet.isFresh else { return "Waiting for fresh sensor data" }
        return packet.orientationState.label
    }

    func invalidateLiveData() { liveMotion = nil; liveRates = nil; telemetry = nil; displayJoin = LiveDisplayJoin() }
    private func resetLiveConnection() {
        invalidateLiveData(); motionOrder = LivePacketOrder(); ratesOrder = LivePacketOrder()
        calibrationProgress.finish()
        liveSession = nil; sessionOrder = LivePacketOrder(); missedLivePackets = 0
        pendingCommand = nil; pendingCommandAt = nil; awaitingCalibrationStart = false; calibrationRequestAt = nil
    }

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: .main)
        telemetryRefreshTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in self?.refreshTelemetryState() }
        }
    }

    private func refreshTelemetryState() {
        telemetryHeartbeat = .now
        let now = ProcessInfo.processInfo.systemUptime
        if calibrationProgress.expire(now: now) {
            awaitingCalibrationStart = false; calibrationRequestAt = nil
            errorMessage = "Calibration did not complete. Keep the box upright and still, then retry."
            record("CALIBRATION_TIMEOUT", kind: .status)
        }
        if let requested = calibrationRequestAt, now-requested > 5 {
            calibrationRequestAt = nil; awaitingCalibrationStart = false
            calibrationProgress.finish()
            errorMessage = "Calibration did not start. Reconnect and try again."
        }
        if let sent = pendingCommandAt, now-sent > 5 {
            pendingCommand = nil; pendingCommandAt = nil; awaitingCalibrationStart = false
            calibrationRequestAt = nil
            calibrationProgress.finish()
            errorMessage = "Command acknowledgement timed out. Reconnect and try again."
        }
    }

    deinit { telemetryRefreshTimer?.invalidate() }

    func scanForDevices() {
        guard centralManager.state == .poweredOn else {
            updateAvailability(for: centralManager.state)
            return
        }
        errorMessage = nil
        discoveredDevices = []
        // Scanning must not demote a live connection and trigger a reconnect.
        if connectionState != .connected && connectionState != .connecting && connectionState != .discovering {
            connectionState = .scanning
        }
        centralManager.scanForPeripherals(withServices: [Self.serviceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }
    
    func reconnectToSavedDevice() {
        guard connectionState != .connected && connectionState != .connecting && connectionState != .discovering else { return }
        guard centralManager.state == .poweredOn else {
            updateAvailability(for: centralManager.state)
            return
        }

        guard
            let savedID = UserDefaults.standard.string(forKey: savedPeripheralIDKey),
            let uuid = UUID(uuidString: savedID)
        else {
            scanForDevices()
            return
        }

        let peripherals = centralManager.retrievePeripherals(
            withIdentifiers: [uuid]
        )

        if let savedPeripheral = peripherals.first {
            connect(to: savedPeripheral)
        } else {
            scanForDevices()
        }
    }

    func connect(to device: BluetoothDevice) {
        guard centralManager.state == .poweredOn,
              let target = centralManager.retrievePeripherals(withIdentifiers: [device.id]).first else {
            errorMessage = "That logger is no longer available. Scan again and retry."
            return
        }
        connect(to: target)
    }
	
    func disconnect() {
        wantsConnection = false
        resetLiveConnection()
        stopScanning()
        guard let peripheral else { connectionState = .disconnected; return }
        connectionState = .disconnecting
        centralManager.cancelPeripheralConnection(peripheral)
    }

    func send(command: String) {
        guard isConnected, let commandCharacteristic, let peripheral else {
            errorMessage = "Connect to the lap logger before sending commands."
            return
        }
        guard pendingCommand == nil else {
            errorMessage = "Waiting for the previous command to be confirmed."
            return
        }
        if isNiclaLive && command == "CALIBRATE" && calibrationProgress.active { return }
        guard let data = command.data(using: .utf8) else { return }
        errorMessage = nil
        pendingCommand = command
        pendingCommandAt = ProcessInfo.processInfo.systemUptime
        if isNiclaLive && command == "CALIBRATE" { awaitingCalibrationStart = true; calibrationRequestAt = ProcessInfo.processInfo.systemUptime; calibrationProgress.begin(now: ProcessInfo.processInfo.systemUptime); invalidateLiveData() }
        peripheral.writeValue(data, for: commandCharacteristic, type: .withResponse)
    }

    func clearActivityLog() {
        activityLog = []
    }

    private func connect(to target: CBPeripheral) {
        if peripheral?.identifier == target.identifier &&
            (connectionState == .connected || connectionState == .connecting || connectionState == .discovering || connectionState == .disconnecting) { return }
        if let previous = peripheral, previous.identifier != target.identifier {
            centralManager.cancelPeripheralConnection(previous)
        }
        resetLiveConnection(); isNiclaLive = false
        wantsConnection = true
        UserDefaults.standard.set(
            target.identifier.uuidString,
            forKey: savedPeripheralIDKey
        )
        stopScanning()
        errorMessage = nil
        peripheral = target
        commandCharacteristic = nil
        statusCharacteristic = nil
        telemetryCharacteristic = nil
        connectionState = .connecting
        centralManager.connect(target)
    }

    private func stopScanning() {
        if centralManager.isScanning { centralManager.stopScan() }
    }

    private func updateAvailability(for state: CBManagerState) {
        availability = switch state {
        case .poweredOn: .ready
        case .unauthorized: .unauthorized
        case .poweredOff: .unavailable("Bluetooth is turned off")
        case .unsupported: .unavailable("Bluetooth LE is not supported on this device")
        case .resetting: .unavailable("Bluetooth is resetting")
        case .unknown: .unknown
        @unknown default: .unavailable("Bluetooth is unavailable")
        }
    }

    private func record(_ message: String, kind: ActivityEventKind) {
        latestStatusMessage = message
        activityLog.insert(ActivityEvent(date: .now, kind: kind, message: message), at: 0)
        if activityLog.count > 100 { activityLog.removeLast() }
    }
}

extension BluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        updateAvailability(for: central.state)
        guard central.state == .poweredOn else { resetLiveConnection(); stopScanning(); connectionState = .disconnected; return }
        reconnectToSavedDevice()
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? peripheral.name ?? "Unnamed lap logger"
        let device = BluetoothDevice(id: peripheral.identifier, name: name)
        if let index = discoveredDevices.firstIndex(where: { $0.id == device.id }) {
            discoveredDevices[index] = device
        } else { discoveredDevices.append(device) }
        if let savedID = UserDefaults.standard.string(
            forKey: savedPeripheralIDKey
        ),
           savedID == peripheral.identifier.uuidString,
           connectionState == .scanning {
            connect(to: peripheral)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        self.peripheral = peripheral
        peripheral.delegate = self
        connectionState = .discovering
        peripheral.discoverServices([Self.serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        let message = error?.localizedDescription ?? "The logger did not accept the connection."
        errorMessage = message
        connectionState = .failed(message)
        if wantsConnection { scanForDevices() }
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        resetLiveConnection()
        commandCharacteristic = nil
        statusCharacteristic = nil
        telemetryCharacteristic = nil
        pendingCommand = nil
        connectionState = .disconnected
        if let error { errorMessage = error.localizedDescription }
        let reason: String
        if let error {
            let detail = error as NSError
            reason = "\(detail.localizedDescription) [\(detail.domain):\(detail.code)]"
        } else { reason = wantsConnection ? "Link ended without an iOS error" : "Disconnect requested" }
        record("Disconnected from \(peripheral.name ?? "lap logger"): \(reason)", kind: .connection)

        if wantsConnection, central.state == .poweredOn { scanForDevices() }
    }
}

extension BluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        if let error { errorMessage = error.localizedDescription; connectionState = .failed(error.localizedDescription); return }
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else {
            errorMessage = "The lap logger service was not found."
            connectionState = .failed("Required service not found")
            return
        }
        peripheral.discoverCharacteristics([Self.commandUUID, Self.statusUUID, Self.telemetryUUID, Self.motionUUID, Self.ratesUUID, Self.sessionUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        if let error { errorMessage = error.localizedDescription; connectionState = .failed(error.localizedDescription); return }
        isNiclaLive = service.characteristics?.contains(where: { $0.uuid == Self.motionUUID }) == true
        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == Self.motionUUID || characteristic.uuid == Self.ratesUUID || characteristic.uuid == Self.sessionUUID {
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
            }
            if characteristic.uuid == Self.commandUUID { commandCharacteristic = characteristic }
            if characteristic.uuid == Self.statusUUID { statusCharacteristic = characteristic }
            if characteristic.uuid == Self.telemetryUUID { telemetryCharacteristic = characteristic }
        }
        guard let statusCharacteristic, commandCharacteristic != nil else {
            errorMessage = "Required logger characteristics were not found."
            connectionState = .failed("Required characteristics not found")
            return
        }
        peripheral.setNotifyValue(true, for: statusCharacteristic)
        peripheral.readValue(for: statusCharacteristic)
        if let telemetryCharacteristic {
            peripheral.setNotifyValue(true, for: telemetryCharacteristic)
            peripheral.readValue(for: telemetryCharacteristic)
        }
        connectionState = .connected
        record("BLE link established", kind: .connection)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        if let error { invalidateLiveData(); errorMessage = error.localizedDescription; return }
        guard let value = characteristic.value else { return }
        if characteristic.uuid == Self.motionUUID {
            guard let packet = LiveMotion.decode(value) else { invalidateLiveData(); recordMalformedTelemetry(); return }
            guard motionOrder.accept(sequence:packet.sequence,milliseconds:packet.sourceMilliseconds) else { return }
            if awaitingCalibrationStart {
                if packet.orientationState != .calibrating && packet.orientationState != .fault { return }
                awaitingCalibrationStart = false; calibrationRequestAt = nil
            }
            calibrationProgress.observe(packet.orientationState, now: ProcessInfo.processInfo.systemUptime)
            displayJoin.receive(packet)
            liveMotion = packet; missedLivePackets = motionOrder.missingPackets
            updateLiveTelemetry()
            return
        }
        if characteristic.uuid == Self.sessionUUID {
            guard let packet = LiveSession.decode(value) else { recordMalformedTelemetry(); return }
            if sessionOrder.accept(sequence:packet.sequence,milliseconds:packet.sourceMilliseconds) {
                displayJoin.receive(packet)
                liveSession = packet
                updateLiveTelemetry()
            }
            return
        }
        if characteristic.uuid == Self.ratesUUID {
            guard let packet = LiveRates.decode(value) else { liveRates = nil; recordMalformedTelemetry(); return }
            if ratesOrder.accept(sequence:packet.sequence,milliseconds:packet.sourceMilliseconds) { liveRates = packet }
            return
        }
        guard let message = String(data:value,encoding:.utf8) else { return }
        if characteristic.uuid == Self.statusUUID {
            if isNiclaLive {
                calibrationProgress.status(message, now: ProcessInfo.processInfo.systemUptime)
                if message == "CALIBRATION_STARTED" { calibrationRequestAt = nil }
                if message == "LAP_NOT_READY" {
                    errorMessage = "Place the box upright and calibrate before starting a lap."
                }
                if message == "CAL_REJECTED_ACTIVE" {
                    awaitingCalibrationStart = false; calibrationRequestAt = nil
                    errorMessage = "Stop the session before calibrating."
                }
                if message == "CALIBRATION_COMPLETE" || message == "CALIBRATION_RETAINED" {
                    awaitingCalibrationStart = false; calibrationRequestAt = nil; errorMessage = nil
                }
                if message == "CALIBRATION_TIMEOUT" || message == "SENSOR_FAULT" || message == "COMMAND_UNSUPPORTED" {
                    awaitingCalibrationStart = false; calibrationRequestAt = nil; invalidateLiveData()
                    errorMessage = message == "CALIBRATION_TIMEOUT" ? "Calibration timed out. Place upright and still, then retry." :
                        message == "SENSOR_FAULT" ? "Sensor initialization failed. Restart the Nicla." : "This command is not supported by the Nicla."
                }
            }
            record(message, kind: .status)
        } else if characteristic.uuid == Self.telemetryUUID {
            parseTelemetry(message)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        let command = pendingCommand
        pendingCommand = nil
        pendingCommandAt = nil
        if let error { calibrationProgress.finish(); awaitingCalibrationStart = false; calibrationRequestAt = nil; errorMessage = error.localizedDescription; return }
        guard characteristic.uuid == Self.commandUUID, let command else { return }
        lastConfirmedCommand = command
        record("Command transmitted: \(command)", kind: .command)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard peripheral.identifier == self.peripheral?.identifier else { return }
        if let error {
            invalidateLiveData(); errorMessage = error.localizedDescription
            connectionState = .failed("Could not subscribe to live measurements")
        }
    }

    private func updateLiveTelemetry() {
        guard let sample = displayJoin.complete, let roll = sample.motion.roll else {
            telemetry = nil; return
        }
        let packet = sample.motion, session = sample.session
        // Unmatched notifications must not refresh the previous sample's age.
        guard telemetry?.sourceTimestampMilliseconds != packet.sourceMilliseconds else { return }
        telemetry = LeanTelemetry(sourceTimestampMilliseconds:packet.sourceMilliseconds,
            currentLeanDegrees:roll,maximumLeftLeanDegrees:session.left,maximumRightLeanDegrees:session.right,
            isLogging:session.active,receivedAt:.now)
    }

    private func parseTelemetry(_ message: String) {
        let fields = message.split(separator: ",", omittingEmptySubsequences: false)
        guard fields.count == 6,
              fields[0] == "T1",
              let timestamp = UInt32(fields[1]),
              let currentLean = Float(fields[2]),
              let maximumLeft = Float(fields[3]),
              let maximumRight = Float(fields[4]),
              let loggingValue = Int(fields[5]),
              [currentLean, maximumLeft, maximumRight].allSatisfy(\.isFinite),
              maximumLeft >= 0, maximumRight >= 0,
              loggingValue == 0 || loggingValue == 1 else {
            recordMalformedTelemetry()
            return
        }

        telemetry = LeanTelemetry(
            sourceTimestampMilliseconds: timestamp,
            currentLeanDegrees: currentLean,
            maximumLeftLeanDegrees: maximumLeft,
            maximumRightLeanDegrees: maximumRight,
            isLogging: loggingValue == 1,
            receivedAt: .now
        )
    }

    private func recordMalformedTelemetry() {
        guard lastTelemetryErrorAt == nil || Date.now.timeIntervalSince(lastTelemetryErrorAt!) > 5 else { return }
        lastTelemetryErrorAt = .now
        record("Ignored malformed telemetry packet", kind: .error)
    }
}

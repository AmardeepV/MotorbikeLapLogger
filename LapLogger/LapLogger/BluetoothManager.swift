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
    var connectedDeviceName: String? { isConnected ? peripheral?.name : nil }
    var isReconnecting: Bool { wantsConnection && connectionState == .scanning }
    var telemetryIsAvailable: Bool { isConnected && telemetry?.isStale == false }

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: .main)
        telemetryRefreshTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            self?.telemetryHeartbeat = .now
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
        connectionState = .scanning
        centralManager.scanForPeripherals(withServices: [Self.serviceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }
    
    func reconnectToSavedDevice() {
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
        guard let data = command.data(using: .utf8) else { return }
        pendingCommand = command
        peripheral.writeValue(data, for: commandCharacteristic, type: .withResponse)
    }

    func clearActivityLog() {
        activityLog = []
    }

    private func connect(to target: CBPeripheral) {
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
        guard central.state == .poweredOn else { stopScanning(); connectionState = .disconnected; return }
        reconnectToSavedDevice()
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = peripheral.name ?? (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? "Unnamed lap logger"
        let device = BluetoothDevice(id: peripheral.identifier, name: name)
        if !discoveredDevices.contains(device) { discoveredDevices.append(device) }
        if let savedID = UserDefaults.standard.string(
            forKey: savedPeripheralIDKey
        ),
           savedID == peripheral.identifier.uuidString,
           !isConnected {
            connect(to: peripheral)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        self.peripheral = peripheral
        peripheral.delegate = self
        connectionState = .discovering
        peripheral.discoverServices([Self.serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        let message = error?.localizedDescription ?? "The logger did not accept the connection."
        errorMessage = message
        connectionState = .failed(message)
        if wantsConnection { scanForDevices() }
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        commandCharacteristic = nil
        statusCharacteristic = nil
        telemetryCharacteristic = nil
        pendingCommand = nil
        connectionState = .disconnected
        if let error { errorMessage = error.localizedDescription }
        record("Disconnected from \(peripheral.name ?? "lap logger")", kind: .connection)
        if wantsConnection, central.state == .poweredOn { scanForDevices() }
    }
}

extension BluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error { errorMessage = error.localizedDescription; connectionState = .failed(error.localizedDescription); return }
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.serviceUUID }) else {
            errorMessage = "The lap logger service was not found."
            connectionState = .failed("Required service not found")
            return
        }
        peripheral.discoverCharacteristics([Self.commandUUID, Self.statusUUID, Self.telemetryUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error { errorMessage = error.localizedDescription; connectionState = .failed(error.localizedDescription); return }
        for characteristic in service.characteristics ?? [] {
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
        if let error { errorMessage = error.localizedDescription; return }
        guard let value = characteristic.value, let message = String(data: value, encoding: .utf8) else { return }
        if characteristic.uuid == Self.statusUUID {
            record(message, kind: .status)
        } else if characteristic.uuid == Self.telemetryUUID {
            parseTelemetry(message)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        let command = pendingCommand
        pendingCommand = nil
        if let error { errorMessage = error.localizedDescription; return }
        guard characteristic.uuid == Self.commandUUID, let command else { return }
        lastConfirmedCommand = command
        record("Command transmitted: \(command)", kind: .command)
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

import Foundation

enum MotionState: UInt8 {
    case startup = 0, calibrating, stale, invalid, fault, valid, waiting
    var label: String {
        switch self {
        case .startup: return "Starting sensors"
        case .calibrating: return "Keep upright and still"
        case .stale: return "Sensor data delayed"
        case .invalid: return "Measurement unavailable"
        case .fault: return "Sensor fault — restart Nicla"
        case .valid: return "Live"
        case .waiting: return "Place upright, then calibrate"
        }
    }
}

private struct PacketBytes {
    let b: [UInt8]
    func u16(_ at: Int) -> UInt16 { UInt16(b[at]) | UInt16(b[at+1]) << 8 }
    func u32(_ at: Int) -> UInt32 { UInt32(u16(at)) | UInt32(u16(at+2)) << 16 }
    func signed(_ at: Int) -> Int16 { Int16(bitPattern: u16(at)) }
    func values(_ at: Int, count: Int, scale: Float, usable: Bool) -> [Float]? {
        let raw = (0..<count).map { signed(at+2*$0) }
        if usable { return raw.allSatisfy { $0 != 32767 } ? raw.map { Float($0)/scale } : nil }
        return raw.allSatisfy { $0 == 32767 } ? [] : nil
    }
}

struct LiveMotion {
    let sequence: UInt16
    let sourceMilliseconds: UInt32
    let orientationState: MotionState
    let motionState: MotionState
    let flags: UInt8
    let roll: Float?
    let pitch: Float?
    let acceleration: [Float]?
    let receivedUptime: TimeInterval
    var isFresh: Bool { ProcessInfo.processInfo.systemUptime-receivedUptime < 1 }

    static func decode(_ data: Data, now: TimeInterval = ProcessInfo.processInfo.systemUptime) -> LiveMotion? {
        guard data.count == 20 else { return nil }
        let p = PacketBytes(b: Array(data))
        guard p.b[0] == 0xA1, let orientation = MotionState(rawValue: p.b[1]),
              let motion = MotionState(rawValue: p.b[2]), p.b[3] & ~UInt8(7) == 0,
              let angles = p.values(10, count: 2, scale: 100, usable: orientation == .valid),
              let acceleration = p.values(14, count: 3, scale: 100, usable: motion == .valid),
              (orientation != .valid || (abs(angles[0]) <= 180 && abs(angles[1]) <= 90)),
              (motion != .valid || (orientation == .valid && p.b[3] & 5 == 0)) else { return nil }
        return LiveMotion(sequence: p.u16(4), sourceMilliseconds: p.u32(6),
            orientationState: orientation, motionState: motion, flags: p.b[3],
            roll: angles.first, pitch: angles.last, acceleration: acceleration.isEmpty ? nil : acceleration,
            receivedUptime: now)
    }
}

struct LiveRates {
    let sequence: UInt16
    let sourceMilliseconds: UInt32
    let angularRates: [Float]?
    let magneticCounts: [Int16]?
    let receivedUptime: TimeInterval
    var isFresh: Bool { ProcessInfo.processInfo.systemUptime-receivedUptime < 1 }
    static func decode(_ data: Data, now: TimeInterval = ProcessInfo.processInfo.systemUptime) -> LiveRates? {
        guard data.count == 20 else { return nil }
        let p = PacketBytes(b: Array(data))
        guard p.b[0] == 0xB1, p.b[1] & ~UInt8(3) == 0,
              let rates = p.values(8,count:3,scale:10,usable:p.b[1] & 1 != 0) else { return nil }
        let mag = (0..<3).map { p.signed(14+$0*2) }
        guard p.b[1] & 2 != 0 || mag.allSatisfy({ $0 == 32767 }) else { return nil }
        return LiveRates(sequence:p.u16(2),sourceMilliseconds:p.u32(4),
            angularRates:rates.isEmpty ? nil:rates, magneticCounts:p.b[1] & 2 != 0 ? mag:nil,receivedUptime:now)
    }
}

// Duplicate reads/notifications must not make an old sample fresh again.
// Unsigned subtraction handles sequence (16-bit) and uptime (32-bit) wraps.
struct LivePacketOrder {
    private var lastSequence: UInt16?
    private var lastMilliseconds: UInt32?
    private(set) var missingPackets: UInt64 = 0
    mutating func accept(sequence: UInt16, milliseconds: UInt32) -> Bool {
        if let previous = lastSequence, let time = lastMilliseconds {
            let step = sequence &- previous, elapsed = milliseconds &- time
            guard step > 0, step < 32768, elapsed > 0, elapsed < 0x80000000 else { return false }
            missingPackets += UInt64(step-1)
        }
        lastSequence = sequence; lastMilliseconds = milliseconds
        return true
    }
}

struct LiveSession {
    let sequence: UInt16
    let sourceMilliseconds: UInt32
    let active: Bool
    let number: UInt16
    let lap: UInt16
    let left: Float
    let right: Float
    static func decode(_ data: Data) -> LiveSession? {
        guard data.count == 20 else { return nil }
        let p = PacketBytes(b: Array(data))
        guard p.b[0] == 0xC1, p.b[1] <= 1, p.b[16...19].allSatisfy({ $0 == 0 }),
              p.u16(12) <= 18000, p.u16(14) <= 18000,
              (p.b[1] == 1 ? p.u16(10) > 0 : p.u16(10) == 0) else { return nil }
        return LiveSession(sequence:p.u16(2), sourceMilliseconds:p.u32(4), active:p.b[1] == 1,
            number:p.u16(8), lap:p.u16(10), left:Float(p.u16(12))/100, right:Float(p.u16(14))/100)
    }
}

// Quality fluctuations do not end an operation. Only completion, failure,
// disconnection, or a bounded timeout releases the calibration controls.
struct LiveCalibrationProgress {
    private(set) var active = false
    private var beganAt: TimeInterval?
    mutating func begin(now: TimeInterval) { active = true; beganAt = now }
    mutating func finish() { active = false; beganAt = nil }
    mutating func status(_ message: String, now: TimeInterval) {
        switch message {
        case "CALIBRATION_STARTED": if !active { begin(now: now) }
        case "CALIBRATION_COMPLETE", "CALIBRATION_RETAINED", "CALIBRATION_TIMEOUT", "SENSOR_FAULT",
             "CAL_REJECTED_ACTIVE", "COMMAND_UNSUPPORTED", "READY_TO_CALIBRATE": finish()
        default: break
        }
    }
    mutating func observe(_ state: MotionState, now: TimeInterval) {
        if state == .calibrating && !active { begin(now: now) }
        if state == .valid || state == .fault { finish() }
    }
    mutating func expire(now: TimeInterval) -> Bool {
        guard let beganAt, now-beganAt > 35 else { return false }
        finish(); return true
    }
}

// Retain the last complete sample while the next pair is in transit. Never
// refresh its age until both matching packets actually arrive.
struct LiveDisplayJoin {
    private var motion: LiveMotion?
    private var session: LiveSession?
    private(set) var complete: (motion: LiveMotion, session: LiveSession)?
    mutating func receive(_ packet: LiveMotion) {
        motion = packet
        // A short delivery gap may retain the previous sample briefly; faults,
        // invalid orientation and calibration must clear immediately.
        if packet.orientationState != .valid && packet.orientationState != .stale { complete = nil }
        join()
    }
    mutating func receive(_ packet: LiveSession) { session = packet; join() }
    private mutating func join() {
        guard let motion, let session, motion.orientationState == .valid,
              motion.sequence == session.sequence,
              motion.sourceMilliseconds == session.sourceMilliseconds else { return }
        complete = (motion,session)
    }
    func isFresh(now: TimeInterval = ProcessInfo.processInfo.systemUptime) -> Bool {
        guard let complete else { return false }
        let limit: TimeInterval = motion?.orientationState == .stale ? 0.3 : 1
        return now-complete.motion.receivedUptime < limit
    }
}

struct LiveLinkHealth {
    let disconnects: UInt16
    let uptimeMilliseconds: UInt32
    let sensorMaxUs: UInt32
    let bluetoothMaxUs: UInt32
    let loopMaxUs: UInt32
    let receivedUptime: TimeInterval
    var pollMaxMs: UInt16? = nil
    var notifyMaxMs: UInt16? = nil
    var statusMaxMs: UInt16? = nil
    var creditDeferrals: UInt32? = nil
    static func decode(_ data: Data, now: TimeInterval = ProcessInfo.processInfo.systemUptime) -> LiveLinkHealth? {
        guard data.count == 20 else { return nil }
        let p=PacketBytes(b:Array(data))
        if p.b[0] == 0xD2 {
            let poll=p.u16(8),notify=p.u16(10),status=p.u16(12)
            return LiveLinkHealth(disconnects:UInt16(p.b[1]),uptimeMilliseconds:p.u32(4),
                sensorMaxUs:UInt32(p.u16(2))*1000,bluetoothMaxUs:UInt32(max(poll,max(notify,status)))*1000,
                loopMaxUs:UInt32(p.u16(14))*1000,receivedUptime:now,
                pollMaxMs:poll,notifyMaxMs:notify,statusMaxMs:status,creditDeferrals:p.u32(16))
        }
        guard p.b[0] == 0xD1, p.b[1] == 1 else { return nil }
        return LiveLinkHealth(disconnects:p.u16(2),uptimeMilliseconds:p.u32(4),
            sensorMaxUs:p.u32(8),bluetoothMaxUs:p.u32(12),loopMaxUs:p.u32(16),receivedUptime:now)
    }
    var summary: String {
        if let poll=pollMaxMs, let notify=notifyMaxMs, let status=statusMaxMs, let deferred=creditDeferrals {
            return String(format:"Board uptime %.1fs; drops %u; max sensor/poll/notify/status/loop %u/%u/%u/%u/%ums; TX deferrals %u",
                Double(uptimeMilliseconds)/1000,Int(disconnects),Int(sensorMaxUs/1000),Int(poll),Int(notify),
                Int(status),Int(loopMaxUs/1000),Int(deferred))
        }
        return String(format:"Board uptime %.1fs; drops %u; max sensor/BLE/loop %.1f/%.1f/%.1fms",
            Double(uptimeMilliseconds)/1000,Int(disconnects),Double(sensorMaxUs)/1000,
            Double(bluetoothMaxUs)/1000,Double(loopMaxUs)/1000)
    }
}

import Foundation
@main struct Tests {
    static func hex(_ text: String) -> Data {
        let chars=Array(text);return Data(stride(from:0,to:chars.count,by:2).map {UInt8(String(chars[$0...$0+1]),radix:16)!})
    }
    static func main() {
        let healthData=hex("d101010088130000e8030000d007000074000000")
        let health=LiveLinkHealth.decode(healthData,now:0)!
        assert(health.disconnects==1 && health.uptimeMilliseconds==5000 && health.sensorMaxUs==1000)
        assert(health.bluetoothMaxUs==2000 && health.loopMaxUs==116)
        assert(health.summary.contains("uptime 5.0s"))
        assert(LiveLinkHealth.decode(healthData.dropLast())==nil)
        var unknownHealth=healthData;unknownHealth[1]=2
        assert(LiveLinkHealth.decode(unknownHealth)==nil)
        let detailedHealth=LiveLinkHealth.decode(hex("d201010088130000020003000400000001000000"))!
        assert(detailedHealth.pollMaxMs==2 && detailedHealth.notifyMaxMs==3 && detailedHealth.statusMaxMs==4)
        assert(detailedHealth.creditDeferrals==1 && detailedHealth.summary.contains("TX deferrals 1"))
        let sessionData=hex("c101010064000000020001000000000000000000")
        let session=LiveSession.decode(sessionData)!
        assert(session.active && session.number==2 && session.lap==1 && session.left==0)
        var invalidSession=sessionData;invalidSession[1]=0
        assert(LiveSession.decode(invalidSession)==nil)
        invalidSession=sessionData;invalidSession[12]=255;invalidSession[13]=127
        assert(LiveSession.decode(invalidSession)==nil)
        // Golden bytes emitted by the C++ firmware encoder test.
        let data=hex("a1050502ffff7856341230f6f5057b0016ff3200")
        let packet=LiveMotion.decode(data,now:0)!
        assert(packet.sequence==65535 && packet.sourceMilliseconds==0x12345678)
        assert(abs(packet.roll! + 25.12)<0.0001 && abs(packet.pitch! - 15.25)<0.0001)
        assert(packet.acceleration! == [1.23,-2.34,0.5] && packet.flags==2)
        let rates=LiveRates.decode(hex("b103ffff7856341265ff1700e803f6ff1400e2ff"))!
        assert(rates.angularRates! == [-15.5,2.3,100] && rates.magneticCounts! == [-10,20,-30])
        assert(LiveMotion.decode(data.dropLast()) == nil)
        var bad=data;bad[0]=0xFF;assert(LiveMotion.decode(bad)==nil)
        bad=data;bad[1]=99;assert(LiveMotion.decode(bad)==nil)
        bad=data;bad[10]=255;bad[11]=127;assert(LiveMotion.decode(bad)==nil)
        bad=data;bad[2]=2;assert(LiveMotion.decode(bad)==nil) // stale fields must be absent
        bad=data;bad[3]=1;assert(LiveMotion.decode(bad)==nil) // clipped motion cannot be valid
        var waiting=data;waiting[1]=6;waiting[2]=6;waiting[3]=0
        for i in stride(from:10,to:20,by:2) { waiting[i]=255;waiting[i+1]=127 }
        let pending=LiveMotion.decode(waiting)!
        assert(pending.roll==nil && pending.pitch==nil && pending.acceleration==nil)
        assert(!packet.isFresh)
        // Calibration must survive quality gaps and unrelated status messages.
        var calibration=LiveCalibrationProgress()
        calibration.begin(now:0)
        for state in [MotionState.startup,.stale,.invalid,.waiting,.calibrating] {
            calibration.observe(state,now:1);assert(calibration.active)
        }
        calibration.status("CAL_HOLD_STILL",now:2);assert(calibration.active)
        calibration.status("CALIBRATION_STARTED",now:20)
        assert(calibration.expire(now:36) && !calibration.active)
        calibration.begin(now:40);calibration.status("CALIBRATION_TIMEOUT",now:70)
        assert(!calibration.active)
        calibration.begin(now:80);calibration.observe(.valid,now:84);assert(!calibration.active)
        calibration.begin(now:90);calibration.finish();assert(!calibration.active)
        // Different BLE characteristics arrive separately, in either order.
        func samples(_ seq: UInt16, _ ms: UInt32, _ now: TimeInterval) -> (LiveMotion,LiveSession) {
            var a=data,c=sessionData
            a[4]=UInt8(truncatingIfNeeded:seq);a[5]=UInt8(seq>>8)
            c[2]=a[4];c[3]=a[5]
            for i in 0..<4 { a[6+i]=UInt8(truncatingIfNeeded:ms>>(8*i));c[4+i]=a[6+i] }
            return (LiveMotion.decode(a,now:now)!,LiveSession.decode(c)!)
        }
        let first=samples(1,100,0),second=samples(2,200,0.1),third=samples(3,300,0.2)
        var join=LiveDisplayJoin()
        join.receive(first.0);assert(join.complete==nil)
        join.receive(first.1);assert(join.complete?.motion.sequence==1)
        join.receive(second.0);assert(join.complete?.motion.sequence==1 && join.isFresh(now:0.1))
        join.receive(second.1);assert(join.complete?.motion.sequence==2)
        join.receive(third.1);assert(join.complete?.motion.sequence==2)
        join.receive(third.0);assert(join.complete?.motion.sequence==3)
        // A missing partner must expire the last complete sample, never refresh it.
        join.receive(samples(4,400,0.9).0);assert(!join.isFresh(now:1.3))
        join.receive(pending);assert(join.complete==nil)
        var briefGap=LiveDisplayJoin()
        briefGap.receive(first.0);briefGap.receive(first.1)
        var stale=waiting;stale[1]=2;stale[2]=2
        briefGap.receive(LiveMotion.decode(stale,now:0.1)!)
        assert(briefGap.isFresh(now:0.2) && !briefGap.isFresh(now:0.31))
        briefGap.receive(LiveMotion.decode(stale,now:0.25)!)
        assert(!briefGap.isFresh(now:0.31)) // repeated gaps cannot extend the hold
        briefGap.receive(second.1);briefGap.receive(second.0)
        assert(briefGap.complete?.motion.sequence==2 && briefGap.isFresh(now:0.2))
        var invalid=waiting;invalid[1]=3;invalid[2]=3
        briefGap.receive(LiveMotion.decode(invalid,now:0.2)!)
        assert(briefGap.complete==nil)
        var order=LivePacketOrder()
        assert(order.accept(sequence:65535,milliseconds:0xFFFFFFF0))
        assert(!order.accept(sequence:65535,milliseconds:0xFFFFFFF0))
        assert(order.accept(sequence:0,milliseconds:100))
        assert(order.accept(sequence:3,milliseconds:400) && order.missingPackets==2)
        assert(!order.accept(sequence:2,milliseconds:300))
        assert(!order.accept(sequence:4,milliseconds:400))
        order=LivePacketOrder();assert(order.accept(sequence:1,milliseconds:10))
        print("Live Bluetooth decoder/order checks passed")
    }
}

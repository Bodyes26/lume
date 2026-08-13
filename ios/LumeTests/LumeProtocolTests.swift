import XCTest
@testable import Lume

final class LumeProtocolTests: XCTestCase {
    func testTimeSyncUsesLocalCalendarDayAndMinutes() throws {
        var calendar = Calendar(identifier: .gregorian)
        calendar.timeZone = try XCTUnwrap(TimeZone(identifier: "Europe/Rome"))
        let date = try XCTUnwrap(calendar.date(from: DateComponents(
            year: 2026,
            month: 8,
            day: 13,
            hour: 21,
            minute: 5
        )))

        let data = try LumeProtocol.makeTimeSync(date: date, calendar: calendar)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])

        XCTAssertEqual(json["schemaVersion"] as? Int, 1)
        XCTAssertEqual(json["type"] as? String, "time.sync")
        XCTAssertEqual(json["day"] as? Int, 20_260_813)
        XCTAssertEqual(json["minutesIntoDay"] as? Int, 1_265)
    }

    func testPrioritySnapshotMatchesFirmwareContract() throws {
        let items = [
            PriorityItem(id: "p1", title: "Finire la specifica BLE", note: "oggi", isDone: false),
            PriorityItem(id: "p2", title: "Chiamare la banca", isDone: true)
        ]

        let payloads = try LumeProtocol.makePrioritySnapshots(
            items: items,
            maximumPayloadBytes: 512,
            now: Date(timeIntervalSince1970: 1_755_000_000)
        )

        XCTAssertEqual(payloads.count, 1)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: payloads[0]) as? [String: Any])
        XCTAssertEqual(json["type"] as? String, "card")
        XCTAssertEqual(json["kind"] as? String, "priorities.snapshot")
        XCTAssertEqual(json["id"] as? String, "priorities-sync-1755000000")
        XCTAssertEqual(json["title"] as? String, "Priorità")
        XCTAssertEqual(json["part"] as? Int, 0)
        XCTAssertEqual(json["parts"] as? Int, 1)

        let wireItems = try XCTUnwrap(json["priorityItems"] as? [[Any]])
        XCTAssertEqual(wireItems.count, 2)
        XCTAssertEqual(wireItems[0][0] as? String, "p1")
        XCTAssertEqual(wireItems[0][1] as? String, "Finire la specifica BLE")
        XCTAssertEqual(wireItems[0][2] as? String, "oggi")
        XCTAssertEqual(wireItems[0][3] as? Bool, false)
        XCTAssertEqual(wireItems[1][3] as? Bool, true)
    }

    func testPrioritySnapshotSplitsInOrderWithinNegotiatedLimit() throws {
        let items = (1...8).map { index in
            PriorityItem(
                id: "priority-\(index)",
                title: "Priorità numero \(index) con un titolo concreto",
                note: "Nota utile per la priorità numero \(index)",
                isDone: index.isMultiple(of: 3)
            )
        }
        let maximumBytes = 360

        let payloads = try LumeProtocol.makePrioritySnapshots(
            items: items,
            maximumPayloadBytes: maximumBytes,
            now: Date(timeIntervalSince1970: 1_755_000_001)
        )

        XCTAssertGreaterThan(payloads.count, 1)
        var recoveredIDs: [String] = []
        for (index, payload) in payloads.enumerated() {
            XCTAssertLessThanOrEqual(payload.count, maximumBytes)
            let json = try XCTUnwrap(JSONSerialization.jsonObject(with: payload) as? [String: Any])
            XCTAssertEqual(json["id"] as? String, "priorities-sync-1755000001")
            XCTAssertEqual(json["part"] as? Int, index)
            XCTAssertEqual(json["parts"] as? Int, payloads.count)
            let wireItems = try XCTUnwrap(json["priorityItems"] as? [[Any]])
            recoveredIDs.append(contentsOf: wireItems.compactMap { $0.first as? String })
        }
        XCTAssertEqual(recoveredIDs, items.map(\.id))
    }

    func testPriorityToggleActionDecodes() throws {
        let data = Data(#"{"schemaVersion":1,"type":"priority.toggle","id":"p1","done":true,"sequence":7}"#.utf8)
        let action = try LumeProtocol.decodeAction(data)

        XCTAssertEqual(action, LumeDeviceAction(
            schemaVersion: 1,
            type: "priority.toggle",
            id: "p1",
            done: true,
            sequence: 7
        ))
    }
}

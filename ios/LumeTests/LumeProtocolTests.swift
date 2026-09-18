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

    func testReminderSnapshotMatchesFirmwareContract() throws {
        let lists = [
            ReminderList(id: "cal-today", title: "Oggi"),
            ReminderList(id: "cal-home", title: "Casa")
        ]
        let items = [
            ReminderItem(
                id: "rem-1",
                handle: 17,
                listIndex: 0,
                title: "Chiamare il commercialista",
                dueLabel: "Oggi 09:00",
                isOverdue: false
            ),
            ReminderItem(
                id: "rem-2",
                handle: 18,
                listIndex: 1,
                title: "Comprare lampadine",
                dueLabel: "",
                isOverdue: false
            )
        ]

        let payloads = try LumeProtocol.makeReminderSnapshots(
            lists: lists,
            items: items,
            generation: 41,
            maximumPayloadBytes: 512,
            now: Date(timeIntervalSince1970: 1_786_717_487)
        )

        XCTAssertEqual(payloads.count, 1)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: payloads[0]) as? [String: Any])
        XCTAssertEqual(json["type"] as? String, "card")
        XCTAssertEqual(json["kind"] as? String, "reminders.snapshot")
        XCTAssertEqual(json["id"] as? String, "reminders-sync-1786717487")
        XCTAssertEqual(json["title"] as? String, "Promemoria")
        XCTAssertEqual(json["body"] as? String, "2 da fare")
        XCTAssertEqual(json["gen"] as? Int, 41)
        XCTAssertEqual(json["part"] as? Int, 0)
        XCTAssertEqual(json["parts"] as? Int, 1)

        let wireLists = try XCTUnwrap(json["reminderLists"] as? [[Any]])
        XCTAssertEqual(wireLists.count, 2)
        XCTAssertEqual(wireLists[0][0] as? Int, 0)
        XCTAssertEqual(wireLists[0][1] as? String, "Oggi")
        XCTAssertEqual(wireLists[1][0] as? Int, 1)
        XCTAssertEqual(wireLists[1][1] as? String, "Casa")

        let wireItems = try XCTUnwrap(json["reminderItems"] as? [[Any]])
        XCTAssertEqual(wireItems.count, 2)
        XCTAssertEqual(wireItems[0][0] as? Int, 17)
        XCTAssertEqual(wireItems[0][1] as? Int, 0)
        XCTAssertEqual(wireItems[0][2] as? String, "Chiamare il commercialista")
        XCTAssertEqual(wireItems[0][3] as? String, "Oggi 09:00")
        XCTAssertEqual(wireItems[1][0] as? Int, 18)
        XCTAssertEqual(wireItems[1][1] as? Int, 1)
        XCTAssertEqual(wireItems[1][2] as? String, "Comprare lampadine")
        XCTAssertEqual(wireItems[1][3] as? String, "")
    }

    func testReminderSnapshotEmptyListShowsNoRemindersSummary() throws {
        let lists = [ReminderList(id: "cal-today", title: "Oggi")]
        let payloads = try LumeProtocol.makeReminderSnapshots(
            lists: lists,
            items: [],
            generation: 1,
            maximumPayloadBytes: 512,
            now: Date(timeIntervalSince1970: 1_786_717_487)
        )

        XCTAssertEqual(payloads.count, 1)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: payloads[0]) as? [String: Any])
        XCTAssertEqual(json["body"] as? String, "Nessun promemoria")
        let wireItems = try XCTUnwrap(json["reminderItems"] as? [[Any]])
        XCTAssertTrue(wireItems.isEmpty)
    }

    func testReminderSnapshotSplitsInOrderWithinNegotiatedLimit() throws {
        let lists = [ReminderList(id: "cal-work", title: "Lavoro")]
        let items = (1...12).map { index in
            ReminderItem(
                id: "rem-\(index)",
                handle: UInt16(index),
                listIndex: 0,
                title: "Promemoria numero \(index) con testo concreto per il test di split",
                dueLabel: "Oggi 1\(index % 10):00",
                isOverdue: false
            )
        }
        let maximumBytes = 360

        let payloads = try LumeProtocol.makeReminderSnapshots(
            lists: lists,
            items: items,
            generation: 7,
            maximumPayloadBytes: maximumBytes,
            now: Date(timeIntervalSince1970: 1_786_717_488)
        )

        XCTAssertGreaterThan(payloads.count, 1)
        var recoveredHandles: [Int] = []
        for (index, payload) in payloads.enumerated() {
            XCTAssertLessThanOrEqual(payload.count, maximumBytes)
            let json = try XCTUnwrap(JSONSerialization.jsonObject(with: payload) as? [String: Any])
            XCTAssertEqual(json["id"] as? String, "reminders-sync-1786717488")
            XCTAssertEqual(json["gen"] as? Int, 7)
            XCTAssertEqual(json["part"] as? Int, index)
            XCTAssertEqual(json["parts"] as? Int, payloads.count)

            let wireLists = try XCTUnwrap(json["reminderLists"] as? [[Any]])
            XCTAssertEqual(wireLists.count, 1)
            XCTAssertEqual(wireLists[0][1] as? String, "Lavoro")

            let wireItems = try XCTUnwrap(json["reminderItems"] as? [[Any]])
            recoveredHandles.append(contentsOf: wireItems.compactMap { $0.first as? Int })
        }
        XCTAssertEqual(recoveredHandles, items.map { Int($0.handle) })
    }

    func testReminderToggleActionDecodes() throws {
        let data = Data(#"{"schemaVersion":1,"type":"reminder.toggle","gen":41,"handle":17,"done":true,"sequence":7}"#.utf8)
        let action = try LumeProtocol.decodeAction(data)

        XCTAssertEqual(action, LumeDeviceAction(
            schemaVersion: 1,
            type: "reminder.toggle",
            id: nil,
            done: true,
            sequence: 7,
            gen: 41,
            handle: 17
        ))
    }

    func testTrailCatalogPayloadEncodes() throws {
        let stories = [
            TrailStoryInfo(
                id: "silk_road",
                title: "La Via della Seta",
                description: "Test",
                chapterCount: 8,
                locale: "it",
                currentChapter: 0,
                currentDay: 1,
                isInstalled: true,
                isCompleted: false,
                fileSize: 19417
            )
        ]
        let data = try LumeProtocol.makeTrailCatalog(stories: stories)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        XCTAssertEqual(json["type"] as? String, "trail.catalog")
        let list = try XCTUnwrap(json["stories"] as? [[String: Any]])
        XCTAssertEqual(list.count, 1)
        XCTAssertEqual(list[0]["id"] as? String, "silk_road")
        XCTAssertEqual(list[0]["title"] as? String, "La Via della Seta")
        XCTAssertEqual(list[0]["chapters"] as? Int, 8)
        XCTAssertEqual(list[0]["installed"] as? Bool, true)
    }

    func testTrailSaveActionDecodes() throws {
        let raw = #"{"schemaVersion":1,"type":"trail.save","storyId":"silk_road","chapter":3,"save":"YWJjZGVmZ2hpams="}"#
        let action = try LumeProtocol.decodeAction(Data(raw.utf8))

        XCTAssertEqual(action, LumeDeviceAction(
            schemaVersion: 1,
            type: "trail.save",
            storyId: "silk_road",
            chapter: 3,
            save: "YWJjZGVmZ2hpams="
        ))
    }
}

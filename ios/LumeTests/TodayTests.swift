import XCTest
@testable import Lume

final class TodayTests: XCTestCase {
    private var calendar: Calendar {
        var value = Calendar(identifier: .gregorian)
        value.timeZone = TimeZone(secondsFromGMT: 0)!
        return value
    }

    func testProjectionKeepsOnlyOpenItemsInNextTwentyFourHours() throws {
        let now = try XCTUnwrap(calendar.date(from: DateComponents(
            year: 2026,
            month: 8,
            day: 13,
            hour: 10
        )))
        let sources = [
            source("ongoing", .event, "Stand-up", hour: 9, endHour: 11),
            source("later", .event, "Cena", hour: 19, endHour: 20),
            source("overdue", .reminder, "Prenota visita", hour: 8),
            source("undated", .reminder, "Compra il tè"),
            source("completed", .reminder, "Già fatto", hour: 9, completed: true),
            source("outside", .event, "Troppo tardi", day: 14, hour: 11, endHour: 12)
        ]

        let entries = TodayProjection.entries(
            from: sources,
            now: now,
            calendar: calendar,
            locale: Locale(identifier: "it_IT")
        )

        XCTAssertEqual(entries.map(\.id), ["overdue", "ongoing", "later", "undated"])
        XCTAssertEqual(entries.first(where: { $0.id == "ongoing" })?.time, "In corso · fino alle 11:00")
        XCTAssertEqual(entries.first(where: { $0.id == "later" })?.subtitle, "TONIGHT")
        XCTAssertEqual(entries.first(where: { $0.id == "overdue" })?.state, "overdue")
        XCTAssertEqual(entries.first(where: { $0.id == "undated" })?.time, "Senza scadenza")
    }

    func testProjectionCapsFirmwareSnapshotAtSixItems() throws {
        let now = try XCTUnwrap(calendar.date(from: DateComponents(
            year: 2026,
            month: 8,
            day: 13,
            hour: 8
        )))
        let sources = (0..<9).map { index in
            source("event-\(index)", .event, "Evento \(index)", hour: 9 + index, endHour: 10 + index)
        }

        let entries = TodayProjection.entries(from: sources, now: now, calendar: calendar)

        XCTAssertEqual(entries.count, 6)
        XCTAssertEqual(entries.map(\.id), (0..<6).map { "event-\($0)" })
    }

    func testTodaySnapshotMatchesFirmwareContractAndPayloadLimit() throws {
        let entries = [
            TodayEntry(
                id: "event-1",
                kind: .event,
                time: "09:30",
                title: "Revisione progetto",
                subtitle: "TODAY",
                state: "upcoming",
                sortDate: nil
            ),
            TodayEntry(
                id: "reminder-1",
                kind: .reminder,
                time: "Senza scadenza",
                title: "Comprare il tè",
                subtitle: "",
                state: "undated",
                sortDate: nil
            )
        ]
        let now = Date(timeIntervalSince1970: 1_755_000_000)

        let payload = try LumeProtocol.makeTodaySnapshot(
            entries: entries,
            maximumPayloadBytes: 512,
            now: now,
            calendar: calendar,
            locale: Locale(identifier: "it_IT")
        )

        XCTAssertLessThanOrEqual(payload.count, 512)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: payload) as? [String: Any])
        XCTAssertEqual(json["schemaVersion"] as? Int, 1)
        XCTAssertEqual(json["type"] as? String, "card")
        XCTAssertEqual(json["kind"] as? String, "today.snapshot")
        XCTAssertEqual(json["id"] as? String, "today-sync-1755000000")
        XCTAssertEqual(json["title"] as? String, "Oggi")

        let wireItems = try XCTUnwrap(json["items"] as? [[Any]])
        XCTAssertEqual(wireItems.count, 2)
        XCTAssertEqual(wireItems[0] as? [String], ["event", "09:30", "Revisione progetto", "TODAY", "upcoming"])
        XCTAssertEqual(wireItems[1] as? [String], ["reminder", "Senza scadenza", "Comprare il tè", "", "undated"])
    }

    private func source(
        _ id: String,
        _ kind: TodayEntryKind,
        _ title: String,
        day: Int = 13,
        hour: Int? = nil,
        endHour: Int? = nil,
        completed: Bool = false
    ) -> TodaySourceItem {
        let date = hour.flatMap { hour in
            calendar.date(from: DateComponents(year: 2026, month: 8, day: day, hour: hour))
        }
        let endDate = endHour.flatMap { hour in
            calendar.date(from: DateComponents(year: 2026, month: 8, day: day, hour: hour))
        }
        return TodaySourceItem(
            id: id,
            kind: kind,
            title: title,
            date: date,
            endDate: endDate,
            isAllDay: false,
            isCompleted: completed
        )
    }
}

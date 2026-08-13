import Foundation

struct LumeDeviceAction: Decodable, Equatable, Sendable {
    let schemaVersion: Int
    let type: String
    let id: String?
    let done: Bool?
    let sequence: UInt32?
}

enum LumeProtocolError: LocalizedError, Equatable {
    case payloadLimitTooSmall(Int)
    case itemTooLarge(String)

    var errorDescription: String? {
        switch self {
        case .payloadLimitTooSmall(let bytes):
            return "Il collegamento accetta solo \(bytes) byte per messaggio."
        case .itemTooLarge(let title):
            return "“\(title)” è troppo lunga per essere inviata all’X3."
        }
    }
}

enum LumeProtocol {
    static let serviceUUID = "F39F34A5-7DDD-487B-85B6-CE7695466BAE"
    static let cardWriteUUID = "F6361620-0F61-40E9-AA80-5252733C5416"
    static let actionNotifyUUID = "F626E419-C6A8-4048-B684-98C4604D19A3"
    static let schemaVersion = 1
    static let maximumCardBytes = 512
    static let maximumPriorityItems = 10
    static let maximumTodayItems = 6

    static func makeTimeSync(date: Date = .now, calendar: Calendar = .current) throws -> Data {
        let components = calendar.dateComponents([.year, .month, .day, .hour, .minute], from: date)
        let day = (components.year ?? 0) * 10_000 + (components.month ?? 0) * 100 + (components.day ?? 0)
        let minutes = (components.hour ?? 0) * 60 + (components.minute ?? 0)
        return try encode(TimeSyncPayload(day: day, minutesIntoDay: minutes))
    }

    static func makePrioritySnapshots(
        items: [PriorityItem],
        maximumPayloadBytes: Int,
        now: Date = .now
    ) throws -> [Data] {
        let byteLimit = min(maximumCardBytes, maximumPayloadBytes)
        guard byteLimit >= 96 else { throw LumeProtocolError.payloadLimitTooSmall(byteLimit) }

        let safeItems = Array(items.prefix(maximumPriorityItems)).map(SafePriority.init)
        let identifier = "priorities-sync-\(Int(now.timeIntervalSince1970))"
        let active = safeItems.lazy.filter { !$0.done }.count
        let completed = safeItems.count - active
        let summary = "\(active) da fare / \(completed) completate"

        if safeItems.isEmpty {
            let payload = try encodeSnapshot(
                id: identifier,
                body: summary,
                part: 0,
                parts: 1,
                items: []
            )
            guard payload.count <= byteLimit else { throw LumeProtocolError.payloadLimitTooSmall(byteLimit) }
            return [payload]
        }

        var groups: [[SafePriority]] = []
        var current: [SafePriority] = []

        for item in safeItems {
            let candidate = current + [item]
            // 9/10 is the worst field-width case for the protocol's ten-item cap.
            let probe = try encodeSnapshot(id: identifier, body: summary, part: 9, parts: 10, items: candidate)
            if probe.count <= byteLimit {
                current = candidate
                continue
            }

            guard !current.isEmpty else { throw LumeProtocolError.itemTooLarge(item.title) }
            groups.append(current)
            current = [item]

            let single = try encodeSnapshot(id: identifier, body: summary, part: 9, parts: 10, items: current)
            guard single.count <= byteLimit else { throw LumeProtocolError.itemTooLarge(item.title) }
        }
        if !current.isEmpty { groups.append(current) }

        let partCount = groups.count
        return try groups.enumerated().map { index, group in
            let data = try encodeSnapshot(
                id: identifier,
                body: summary,
                part: index,
                parts: partCount,
                items: group
            )
            guard data.count <= byteLimit else { throw LumeProtocolError.itemTooLarge(group[0].title) }
            return data
        }
    }

    static func makeTodaySnapshot(
        entries: [TodayEntry],
        maximumPayloadBytes: Int,
        now: Date = .now,
        calendar: Calendar = .current,
        locale: Locale = Locale(identifier: "it_IT")
    ) throws -> Data {
        let byteLimit = min(maximumCardBytes, maximumPayloadBytes)
        let identifier = "today-sync-\(Int(now.timeIntervalSince1970))"
        let formatter = DateFormatter()
        formatter.calendar = calendar
        formatter.locale = locale
        formatter.timeZone = calendar.timeZone
        formatter.dateFormat = "HH:mm"
        let sync = "Aggiornato \(formatter.string(from: now))"

        var packed: [SafeTodayEntry] = []
        let empty = try encode(TodaySnapshotPayload(
            id: identifier,
            sync: sync,
            items: []
        ))
        guard empty.count <= byteLimit else {
            throw LumeProtocolError.payloadLimitTooSmall(byteLimit)
        }

        for entry in entries.prefix(maximumTodayItems).map(SafeTodayEntry.init) {
            let candidate = packed + [entry]
            let probe = try encode(TodaySnapshotPayload(
                id: identifier,
                sync: sync,
                items: candidate
            ))
            guard probe.count <= byteLimit else {
                if packed.isEmpty { throw LumeProtocolError.itemTooLarge(entry.title) }
                break
            }
            packed = candidate
        }

        return try encode(TodaySnapshotPayload(
            id: identifier,
            sync: sync,
            items: packed
        ))
    }

    static func decodeAction(_ data: Data) throws -> LumeDeviceAction {
        try JSONDecoder().decode(LumeDeviceAction.self, from: data)
    }

    private static func encode<T: Encodable>(_ value: T) throws -> Data {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys, .withoutEscapingSlashes]
        return try encoder.encode(value)
    }

    private static func encodeSnapshot(
        id: String,
        body: String,
        part: Int,
        parts: Int,
        items: [SafePriority]
    ) throws -> Data {
        try encode(PrioritySnapshotPayload(
            id: id,
            body: body,
            part: part,
            parts: parts,
            priorityItems: items
        ))
    }
}

private struct TimeSyncPayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "time.sync"
    let day: Int
    let minutesIntoDay: Int
}

private struct PrioritySnapshotPayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "card"
    let kind = "priorities.snapshot"
    let id: String
    let title = "Priorità"
    let body: String
    let part: Int
    let parts: Int
    let priorityItems: [SafePriority]
}

private struct TodaySnapshotPayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "card"
    let kind = "today.snapshot"
    let id: String
    let title = "Oggi"
    let sync: String
    let items: [SafeTodayEntry]
}

private struct SafeTodayEntry: Encodable {
    let kind: String
    let time: String
    let title: String
    let subtitle: String
    let state: String

    init(_ entry: TodayEntry) {
        kind = entry.kind.rawValue
        time = entry.time.prefixUTF8(maxBytes: 48)
        title = entry.title.prefixUTF8(maxBytes: 96)
        subtitle = entry.subtitle.prefixUTF8(maxBytes: 48)
        state = entry.state.prefixUTF8(maxBytes: 64)
    }

    func encode(to encoder: Encoder) throws {
        var values = encoder.unkeyedContainer()
        try values.encode(kind)
        try values.encode(time)
        try values.encode(title)
        try values.encode(subtitle)
        try values.encode(state)
    }
}

private struct SafePriority: Encodable {
    let id: String
    let title: String
    let note: String
    let done: Bool

    init(_ item: PriorityItem) {
        id = item.id.prefixUTF8(maxBytes: 64)
        title = item.title.prefixUTF8(maxBytes: 96)
        note = item.note.prefixUTF8(maxBytes: 120)
        done = item.isDone
    }

    func encode(to encoder: Encoder) throws {
        var values = encoder.unkeyedContainer()
        try values.encode(id)
        try values.encode(title)
        try values.encode(note)
        try values.encode(done)
    }
}

private extension String {
    func prefixUTF8(maxBytes: Int) -> String {
        guard utf8.count > maxBytes else { return self }
        var result = ""
        result.reserveCapacity(maxBytes)
        for character in self {
            let candidate = result + String(character)
            guard candidate.utf8.count <= maxBytes else { break }
            result = candidate
        }
        return result
    }
}

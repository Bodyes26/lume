import Foundation

struct LumeDeviceAction: Decodable, Equatable, Sendable {
    let schemaVersion: Int
    let type: String
    let id: String?
    let done: Bool?
    let sequence: UInt32?
    let gen: UInt16?
    let handle: UInt16?
    let storyId: String?
    let chapter: Int?
    let save: String?

    init(
        schemaVersion: Int = 1,
        type: String,
        id: String? = nil,
        done: Bool? = nil,
        sequence: UInt32? = nil,
        gen: UInt16? = nil,
        handle: UInt16? = nil,
        storyId: String? = nil,
        chapter: Int? = nil,
        save: String? = nil
    ) {
        self.schemaVersion = schemaVersion
        self.type = type
        self.id = id
        self.done = done
        self.sequence = sequence
        self.gen = gen
        self.handle = handle
        self.storyId = storyId
        self.chapter = chapter
        self.save = save
    }
}
enum SleepMode: Int, Codable, Sendable, CaseIterable, Identifiable {
    case auto = 0
    case dashboard = 1
    case reader = 2
    case reminders = 3
    case quote = 4
    case minimal = 5

    var id: Int { rawValue }

    var title: String {
        switch self {
        case .auto: return "Automatico"
        case .dashboard: return "Lavagna Quotidiana"
        case .reader: return "Compagno di Lettura"
        case .reminders: return "Promemoria & Focus"
        case .quote: return "Citazione & Mantra"
        case .minimal: return "Minimalista Lume"
        }
    }

    var subtitle: String {
        switch self {
        case .auto: return "Adatta il poster al contesto (lettura, agenda o workout)"
        case .dashboard: return "Quadro completo: data, telemetria, impegni e promemoria"
        case .reader: return "Libro in corso, progresso %, pagine lette e citazione"
        case .reminders: return "Checklist completa delle tue priorità da completare"
        case .quote: return "Frase ispirazionale, mantra o nota con grande risalto"
        case .minimal: return "Marchio geometrico Lume, data e telemetria essenziale"
        }
    }

    var iconName: String {
        switch self {
        case .auto: return "sparkles"
        case .dashboard: return "rectangle.3.group"
        case .reader: return "book.closed"
        case .reminders: return "checklist"
        case .quote: return "quote.opening"
        case .minimal: return "circle.hexagongrid"
        }
    }
}

struct SleepConfig: Codable, Sendable, Equatable {
    var mode: SleepMode = .auto
    var customTitle: String = ""
    var customQuote: String = ""
    var customAuthor: String = ""
    var showBattery: Bool = true
    var showTemperature: Bool = true
    var showNextEvent: Bool = true
    var showReadingStats: Bool = true
    var showSleepTime: Bool = true
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
    static let maximumReminderLists = 4
    static let maximumReminderItems = 40
    static let maximumRemindersPerList = 20
    static let maximumReminderTitleCharacters = 96
    static let maximumReminderDueCharacters = 16
    static let maximumReminderListNameCharacters = 24
    static let maximumTodayItems = 6

    static func makeTimeSync(date: Date = .now, calendar: Calendar = .current) throws -> Data {
        let components = calendar.dateComponents([.year, .month, .day, .hour, .minute], from: date)
        let day = (components.year ?? 0) * 10_000 + (components.month ?? 0) * 100 + (components.day ?? 0)
        let minutes = (components.hour ?? 0) * 60 + (components.minute ?? 0)
        return try encode(TimeSyncPayload(day: day, minutesIntoDay: minutes))
    }

    static func makeReminderSnapshots(
        lists: [ReminderList],
        items: [ReminderItem],
        generation: UInt16,
        maximumPayloadBytes: Int,
        now: Date = .now
    ) throws -> [Data] {
        let byteLimit = min(maximumCardBytes, maximumPayloadBytes)
        guard byteLimit >= 96 else { throw LumeProtocolError.payloadLimitTooSmall(byteLimit) }

        let safeLists = lists.prefix(maximumReminderLists).enumerated().map { index, list in
            SafeReminderList(index: index, title: list.title)
        }
        let safeItems = items.prefix(maximumReminderItems).map(SafeReminder.init)
        let identifier = "reminders-sync-\(Int(now.timeIntervalSince1970))"
        let summary: String
        if safeItems.isEmpty {
            summary = "Nessun promemoria"
        } else if safeItems.count == 1 {
            summary = "1 da fare"
        } else {
            summary = "\(safeItems.count) da fare"
        }

        if safeItems.isEmpty {
            let payload = try encodeSnapshot(
                id: identifier,
                body: summary,
                generation: generation,
                lists: safeLists,
                part: 0,
                parts: 1,
                items: []
            )
            guard payload.count <= byteLimit else { throw LumeProtocolError.payloadLimitTooSmall(byteLimit) }
            return [payload]
        }

        var groups: [[SafeReminder]] = []
        var current: [SafeReminder] = []

        for item in safeItems {
            let candidate = current + [item]
            let probe = try encodeSnapshot(
                id: identifier,
                body: summary,
                generation: generation,
                lists: safeLists,
                part: 9,
                parts: 10,
                items: candidate
            )
            if probe.count <= byteLimit {
                current = candidate
                continue
            }

            guard !current.isEmpty else { throw LumeProtocolError.itemTooLarge(item.title) }
            groups.append(current)
            current = [item]

            let single = try encodeSnapshot(
                id: identifier,
                body: summary,
                generation: generation,
                lists: safeLists,
                part: 9,
                parts: 10,
                items: current
            )
            guard single.count <= byteLimit else { throw LumeProtocolError.itemTooLarge(item.title) }
        }
        if !current.isEmpty { groups.append(current) }

        let partCount = groups.count
        return try groups.enumerated().map { index, group in
            let data = try encodeSnapshot(
                id: identifier,
                body: summary,
                generation: generation,
                lists: safeLists,
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
        guard byteLimit >= 96 else { throw LumeProtocolError.payloadLimitTooSmall(byteLimit) }

        let safeEntries = Array(entries.prefix(maximumTodayItems)).map(SafeTodayEntry.init)
        let identifier = "today-sync-\(Int(now.timeIntervalSince1970))"
        let syncFormatter = DateFormatter()
        syncFormatter.calendar = calendar
        syncFormatter.locale = locale
        syncFormatter.dateFormat = "HH:mm"
        let syncText = "Sinc. \(syncFormatter.string(from: now))"

        let payload = try encode(TodaySnapshotPayload(
            id: identifier,
            sync: syncText,
            items: safeEntries
        ))

        guard payload.count <= byteLimit else {
            if let first = safeEntries.first {
                throw LumeProtocolError.itemTooLarge(first.title)
            }
            throw LumeProtocolError.payloadLimitTooSmall(byteLimit)
        }

        return payload
    }
    static func makeSleepConfig(
        config: SleepConfig,
        maximumPayloadBytes: Int = maximumCardBytes
    ) throws -> Data {
        let byteLimit = min(maximumCardBytes, maximumPayloadBytes)
        guard byteLimit >= 96 else { throw LumeProtocolError.payloadLimitTooSmall(byteLimit) }

        let payload = SleepConfigPayload(
            face: config.mode.rawValue,
            title: config.customTitle.prefixUTF8(maxBytes: 48),
            quote: config.customQuote.prefixUTF8(maxBytes: 160),
            author: config.customAuthor.prefixUTF8(maxBytes: 48),
            battery: config.showBattery,
            temp: config.showTemperature,
            event: config.showNextEvent,
            stats: config.showReadingStats,
            stamp: config.showSleepTime
        )
        let data = try encode(payload)
        guard data.count <= byteLimit else {
            throw LumeProtocolError.itemTooLarge(config.customQuote)
        }
        return data
    }

    static func makeTrailCatalog(stories: [TrailStoryInfo]) throws -> Data {
        let payload = TrailCatalogPayload(
            stories: stories.map {
                TrailStoryEntry(
                    id: $0.id,
                    title: $0.title,
                    chapters: $0.chapterCount,
                    size: $0.fileSize,
                    locale: $0.locale,
                    installed: $0.isInstalled
                )
            }
        )
        return try encode(payload)
    }

    static func makeTrailSaveRestore(storyId: String, saveBase64: String) throws -> Data {
        let payload = TrailSaveRestorePayload(
            storyId: storyId,
            save: saveBase64
        )
        return try encode(payload)
    }


    static func decodeAction(_ data: Data) throws -> LumeDeviceAction {
        try JSONDecoder().decode(LumeDeviceAction.self, from: data)
    }

    private static func encode<T: Encodable>(_ value: T) throws -> Data {
        try JSONEncoder().encode(value)
    }

    private static func encodeSnapshot(
        id: String,
        body: String,
        generation: UInt16,
        lists: [SafeReminderList],
        part: Int,
        parts: Int,
        items: [SafeReminder]
    ) throws -> Data {
        try encode(ReminderSnapshotPayload(
            id: id,
            body: body,
            gen: generation,
            part: part,
            parts: parts,
            reminderLists: lists,
            reminderItems: items
        ))
    }
}

private struct TimeSyncPayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "time.sync"
    let day: Int
    let minutesIntoDay: Int
}
private struct SleepConfigPayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "sleep.config"
    let face: Int
    let title: String
    let quote: String
    let author: String
    let battery: Bool
    let temp: Bool
    let event: Bool
    let stats: Bool
    let stamp: Bool
}

private struct TrailStoryEntry: Encodable {
    let id: String
    let title: String
    let chapters: Int
    let size: Int
    let locale: String
    let installed: Bool
}

private struct TrailCatalogPayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "trail.catalog"
    let stories: [TrailStoryEntry]
}

private struct TrailSaveRestorePayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "trail.save.restore"
    let storyId: String
    let save: String
}


private struct ReminderSnapshotPayload: Encodable {
    let schemaVersion = LumeProtocol.schemaVersion
    let type = "card"
    let kind = "reminders.snapshot"
    let id: String
    let title = "Promemoria"
    let body: String
    let gen: UInt16
    let part: Int
    let parts: Int
    let reminderLists: [SafeReminderList]
    let reminderItems: [SafeReminder]
}

private struct SafeReminderList: Encodable {
    let index: Int
    let title: String

    init(index: Int, title: String) {
        self.index = index
        self.title = title.prefixUTF8(maxBytes: 24)
    }

    func encode(to encoder: Encoder) throws {
        var values = encoder.unkeyedContainer()
        try values.encode(index)
        try values.encode(title)
    }
}

private struct SafeReminder: Encodable {
    let handle: UInt16
    let listIndex: Int
    let title: String
    let due: String

    init(_ item: ReminderItem) {
        handle = item.handle
        listIndex = item.listIndex
        title = item.title.prefixUTF8(maxBytes: 96)
        due = item.dueLabel.prefixUTF8(maxBytes: 16)
    }

    func encode(to encoder: Encoder) throws {
        var values = encoder.unkeyedContainer()
        try values.encode(handle)
        try values.encode(listIndex)
        try values.encode(title)
        try values.encode(due)
    }
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

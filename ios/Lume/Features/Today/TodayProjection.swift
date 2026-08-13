import Foundation

struct TodaySourceItem: Equatable, Sendable {
    let id: String
    let kind: TodayEntryKind
    let title: String
    let date: Date?
    let endDate: Date?
    let isAllDay: Bool
    let isCompleted: Bool
}

enum TodayProjection {
    static let maximumItems = 6

    static func entries(
        from sources: [TodaySourceItem],
        now: Date = .now,
        calendar: Calendar = .current,
        locale: Locale = .current
    ) -> [TodayEntry] {
        guard let horizon = calendar.date(byAdding: .hour, value: 24, to: now) else { return [] }

        return sources
            .filter { source in
                switch source.kind {
                case .event:
                    guard let start = source.date, let end = source.endDate else { return false }
                    return end > now && start < horizon
                case .reminder:
                    guard !source.isCompleted else { return false }
                    return source.date.map { $0 < horizon } ?? true
                }
            }
            .sorted { lhs, rhs in
                switch (lhs.date, rhs.date) {
                case let (left?, right?) where left != right:
                    return left < right
                case (nil, _?):
                    return false
                case (_?, nil):
                    return true
                default:
                    if lhs.kind != rhs.kind { return lhs.kind == .event }
                    return lhs.title.localizedCaseInsensitiveCompare(rhs.title) == .orderedAscending
                }
            }
            .prefix(maximumItems)
            .map { source in
                TodayEntry(
                    id: source.id,
                    kind: source.kind,
                    time: timeLabel(for: source, now: now, calendar: calendar, locale: locale),
                    title: cleanTitle(source.title),
                    subtitle: source.kind == .event
                        ? bucketLabel(for: source.date ?? now, now: now, calendar: calendar, locale: locale)
                        : "",
                    state: state(for: source, now: now),
                    sortDate: source.date
                )
            }
    }

    private static func cleanTitle(_ title: String) -> String {
        let clean = title.trimmingCharacters(in: .whitespacesAndNewlines)
        return clean.isEmpty ? "Senza titolo" : clean
    }

    private static func timeLabel(
        for source: TodaySourceItem,
        now: Date,
        calendar: Calendar,
        locale: Locale
    ) -> String {
        guard let date = source.date else { return "Senza scadenza" }
        if source.isAllDay { return "Tutto il giorno" }

        let time = formatter("HH:mm", calendar: calendar, locale: locale).string(from: date)
        switch source.kind {
        case .event:
            if date <= now, let end = source.endDate, end > now {
                let endTime = formatter("HH:mm", calendar: calendar, locale: locale).string(from: end)
                return "In corso · fino alle \(endTime)"
            }
            return time
        case .reminder:
            if date < now { return "Scaduto · \(time)" }
            if calendar.isDateInTomorrow(date) { return "Domani · \(time)" }
            return "Oggi · \(time)"
        }
    }

    private static func bucketLabel(
        for date: Date,
        now: Date,
        calendar: Calendar,
        locale: Locale
    ) -> String {
        if calendar.isDate(date, inSameDayAs: now) {
            return calendar.component(.hour, from: date) >= 18 ? "TONIGHT" : "TODAY"
        }
        if calendar.isDateInTomorrow(date) { return "TOMORROW" }
        return formatter("EEE d MMM", calendar: calendar, locale: locale)
            .string(from: date)
            .uppercased(with: locale)
    }

    private static func state(for source: TodaySourceItem, now: Date) -> String {
        switch source.kind {
        case .event:
            if source.isAllDay { return "all-day" }
            if let start = source.date, let end = source.endDate, start <= now, end > now { return "ongoing" }
            return "upcoming"
        case .reminder:
            if source.date == nil { return "undated" }
            return source.date! < now ? "overdue" : "due"
        }
    }

    private static func formatter(
        _ format: String,
        calendar: Calendar,
        locale: Locale
    ) -> DateFormatter {
        let formatter = DateFormatter()
        formatter.calendar = calendar
        formatter.locale = locale
        formatter.timeZone = calendar.timeZone
        formatter.dateFormat = format
        return formatter
    }
}

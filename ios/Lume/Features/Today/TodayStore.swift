@preconcurrency import EventKit
import Foundation

@MainActor
enum TodayAccessState: Equatable {
    case notDetermined
    case requesting
    case available
    case partial
    case denied
    case restricted
}

@MainActor
final class TodayStore: ObservableObject {
    @Published private(set) var entries: [TodayEntry] = []
    @Published private(set) var accessState: TodayAccessState = .notDetermined
    @Published private(set) var isRefreshing = false
    @Published private(set) var lastRefreshDate: Date?
    @Published private(set) var errorMessage: String?

    private let eventStore: EKEventStore

    init(eventStore: EKEventStore = EKEventStore()) {
        self.eventStore = eventStore
        updateAccessState()
    }

    var canSync: Bool {
        canRead(.event) || canRead(.reminder)
    }

    func requestAccessAndRefresh() async {
        accessState = .requesting
        errorMessage = nil

        do {
            if EKEventStore.authorizationStatus(for: .event) == .notDetermined {
                _ = try await eventStore.requestFullAccessToEvents()
            }
            if EKEventStore.authorizationStatus(for: .reminder) == .notDetermined {
                _ = try await eventStore.requestFullAccessToReminders()
            }
        } catch {
            errorMessage = "L’accesso a Calendario e Promemoria non è riuscito: \(error.localizedDescription)"
        }

        updateAccessState()
        await refreshIfAuthorized()
    }

    func refreshIfAuthorized() async {
        updateAccessState()
        guard canSync, !isRefreshing else { return }

        isRefreshing = true
        errorMessage = nil
        defer { isRefreshing = false }

        let now = Date()
        guard let horizon = Calendar.current.date(byAdding: .hour, value: 24, to: now) else { return }
        var sources: [TodaySourceItem] = []

        if canRead(.event) {
            let predicate = eventStore.predicateForEvents(withStart: now, end: horizon, calendars: nil)
            sources += eventStore.events(matching: predicate).compactMap { event in
                guard event.status != .canceled else { return nil }
                return TodaySourceItem(
                    id: event.eventIdentifier ?? event.calendarItemIdentifier,
                    kind: .event,
                    title: event.title ?? "",
                    date: event.startDate,
                    endDate: event.endDate,
                    isAllDay: event.isAllDay,
                    isCompleted: false
                )
            }
        }

        if canRead(.reminder) {
            let predicate = eventStore.predicateForIncompleteReminders(
                withDueDateStarting: nil,
                ending: nil,
                calendars: nil
            )
            sources += await fetchReminderSources(matching: predicate)
        }

        entries = TodayProjection.entries(from: sources, now: now)
        lastRefreshDate = now
    }

    private func fetchReminderSources(matching predicate: NSPredicate) async -> [TodaySourceItem] {
        await withCheckedContinuation { continuation in
            eventStore.fetchReminders(matching: predicate) { reminders in
                let sources = (reminders ?? []).map { reminder in
                    let dueComponents = reminder.dueDateComponents
                    return TodaySourceItem(
                        id: reminder.calendarItemIdentifier,
                        kind: .reminder,
                        title: reminder.title,
                        date: dueComponents.flatMap(Self.date(from:)),
                        endDate: nil,
                        isAllDay: dueComponents?.hour == nil,
                        isCompleted: reminder.isCompleted
                    )
                }
                continuation.resume(returning: sources)
            }
        }
    }

    private func canRead(_ entity: EKEntityType) -> Bool {
        EKEventStore.authorizationStatus(for: entity) == .fullAccess
    }

    private func updateAccessState() {
        let eventStatus = EKEventStore.authorizationStatus(for: .event)
        let reminderStatus = EKEventStore.authorizationStatus(for: .reminder)
        let readable = [eventStatus, reminderStatus].filter { $0 == .fullAccess }.count

        if readable == 2 {
            accessState = .available
        } else if readable == 1 {
            accessState = .partial
        } else if eventStatus == .restricted || reminderStatus == .restricted {
            accessState = .restricted
        } else if eventStatus == .denied || reminderStatus == .denied {
            accessState = .denied
        } else {
            accessState = .notDetermined
        }
    }

    nonisolated private static func date(from components: DateComponents) -> Date? {
        var calendar = components.calendar ?? Calendar.current
        if let timeZone = components.timeZone { calendar.timeZone = timeZone }
        return calendar.date(from: components)
    }
}

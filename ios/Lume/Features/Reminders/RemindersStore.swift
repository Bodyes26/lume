@preconcurrency import EventKit
import Foundation

/// The single source of truth for Lume's to-dos: iOS Reminders.
///
/// There is no local database any more (the old `PrioritiesStore` and its
/// `lume.priorities.v1` blob are gone): everything the X3 shows is read from
/// EventKit at refresh time, and a tick coming from the device is written back
/// with `EKEventStore.save(_:commit:)` so it lands in Reminders on
/// iPhone/Mac/Watch. Only the *selection of lists* is ours, and it is a list of
/// `EKCalendar.calendarIdentifier` in `UserDefaults`.
///
/// Access is the same shape as `TodayStore` (Features/Today/TodayStore.swift):
/// `requestFullAccessToReminders()` is already requested there, so this store
/// never adds a new prompt or entitlement — it only reports the current status.
@MainActor
final class RemindersStore: ObservableObject {
    enum AccessState: Equatable {
        case notDetermined
        case denied
        case restricted
        case available
    }

    /// Every reminders list on the phone, for the selection screen.
    @Published private(set) var lists: [ReminderList] = []

    /// Selected lists in the order of `selectedListIDs`.
    var selectedLists: [ReminderList] {
        let byID = Dictionary(uniqueKeysWithValues: lists.map { ($0.id, $0) })
        return selectedListIDs.compactMap { byID[$0] }
    }

    /// The lists promoted to cards on the X3, in card order.
    ///
    /// Settable by the UI; the observer keeps the invariants the firmware
    /// relies on (no duplicates, at most `LumeProtocol.maximumReminderLists`)
    /// and persists them. It deliberately does NOT refresh: the caller decides
    /// when to pay for the EventKit fetch (`refresh()` /
    /// `LumeBluetoothManager.syncReminders()`).
    @Published var selectedListIDs: [String] = [] {
        didSet {
            let sanitized = sanitizedSelection(selectedListIDs)
            guard sanitized == selectedListIDs else {
                // Re-entering didSet once is safe: sanitize is idempotent, so
                // the second pass compares equal and falls through to persist.
                selectedListIDs = sanitized
                return
            }
            defaults.set(sanitized, forKey: Self.selectionKey)
        }
    }

    /// Open reminders of the selected lists, in the order the X3 draws them.
    @Published private(set) var items: [ReminderItem] = []
    @Published private(set) var accessState: AccessState = .notDetermined
    @Published private(set) var errorMessage: String?

    /// Generation of the handle map. The device echoes it back in
    /// `reminder.toggle`; a mismatch means the list moved under the device and
    /// the handle no longer identifies the same reminder (see `complete`).
    var generation: UInt16 { currentGeneration }

    static let selectionKey = "lume.reminders.selectedLists"

    private let eventStore: EKEventStore
    private let defaults: UserDefaults
    private let calendar: Calendar
    private let locale: Locale

    private var currentGeneration: UInt16 = 0
    /// handle -> `EKReminder.calendarItemIdentifier` for the current generation.
    private var identifiersByHandle: [UInt16: String] = [:]
    /// True when a cap (20 per list / 40 total) dropped reminders, so the card
    /// body can say it instead of silently lying about the workload.
    private var didTruncate = false

    init(
        eventStore: EKEventStore = EKEventStore(),
        defaults: UserDefaults = .standard,
        calendar: Calendar = .current,
        locale: Locale = Locale(identifier: "it_IT")
    ) {
        self.eventStore = eventStore
        self.defaults = defaults
        self.calendar = calendar
        self.locale = locale
        // Property observers do not run during init, so the stored selection is
        // sanitized by hand here. It is NOT filtered against `lists` yet: the
        // lists are unknown until the first refresh, and filtering now would
        // silently wipe the user's selection on every cold start.
        let stored = defaults.stringArray(forKey: Self.selectionKey) ?? []
        var unique: [String] = []
        for identifier in stored where !unique.contains(identifier) {
            unique.append(identifier)
        }
        selectedListIDs = Array(unique.prefix(LumeProtocol.maximumReminderLists))
        updateAccessState()
    }

    func requestAccessAndRefresh() async {
        errorMessage = nil
        if EKEventStore.authorizationStatus(for: .reminder) == .notDetermined {
            do {
                _ = try await eventStore.requestFullAccessToReminders()
            } catch {
                errorMessage = "L’accesso ai Promemoria non è riuscito: \(error.localizedDescription)"
            }
        }
        updateAccessState()
        await refresh()
    }
    func refreshIfAuthorized() async {
        updateAccessState()
        guard accessState == .available else { return }
        await refresh()
    }


    func refresh() async {
        updateAccessState()
        // Every refresh mints a new generation, even a rejected one: the device
        // must never keep handles from a map we no longer hold.
        bumpGeneration()

        guard accessState == .available else {
            identifiersByHandle = [:]
            items = []
            didTruncate = false
            return
        }

        let calendars = eventStore.calendars(for: .reminder)
        lists = calendars.map { ReminderList(id: $0.calendarIdentifier, title: $0.title) }

        // Lists deleted on the iPhone are dropped here (didSet persists the
        // pruned selection): the selection screen and the cards must not offer
        // an identifier EventKit no longer resolves.
        let known = Set(lists.map(\.id))
        let pruned = selectedListIDs.filter { known.contains($0) }
        if pruned != selectedListIDs { selectedListIDs = pruned }

        let selected = selectedListIDs.compactMap { identifier in
            calendars.first { $0.calendarIdentifier == identifier }
        }
        guard !selected.isEmpty else {
            identifiersByHandle = [:]
            items = []
            didTruncate = false
            return
        }

        let predicate = eventStore.predicateForIncompleteReminders(
            withDueDateStarting: nil,
            ending: nil,
            calendars: selected
        )
        let fetched = await fetchOpenReminders(matching: predicate)
        let now = Date()

        var packed: [ReminderItem] = []
        var identifiers: [UInt16: String] = [:]
        var truncated = false
        var nextHandle: UInt16 = 1

        for (listIndex, calendar) in selected.enumerated() {
            let listReminders = fetched
                .filter { $0.calendarIdentifier == calendar.calendarIdentifier }
                .sorted { Self.isOrderedBefore($0, $1, now: now) }
            if listReminders.count > LumeProtocol.maximumRemindersPerList { truncated = true }

            for source in listReminders.prefix(LumeProtocol.maximumRemindersPerList) {
                guard packed.count < LumeProtocol.maximumReminderItems else {
                    truncated = true
                    break
                }
                let handle = nextHandle
                nextHandle &+= 1
                identifiers[handle] = source.identifier
                packed.append(ReminderItem(
                    id: source.identifier,
                    handle: handle,
                    listIndex: listIndex,
                    title: source.title,
                    dueLabel: dueLabel(for: source, now: now),
                    isOverdue: source.isOverdue(now: now, calendar: self.calendar)
                ))
            }
        }

        identifiersByHandle = identifiers
        items = packed
        didTruncate = truncated
        errorMessage = nil
    }

    /// Ticks a reminder off in EventKit. `false` means "refused": the caller
    /// re-sends a snapshot so the device drops its optimistic state.
    @discardableResult
    func complete(handle: UInt16, generation: UInt16) async -> Bool {
        guard
            handle != 0,
            generation == currentGeneration,
            let identifier = identifiersByHandle[handle]
        else {
            // Stale map: the handle may now point at a different reminder, so
            // completing it would tick off the wrong thing.
            await refresh()
            return false
        }

        guard let reminder = eventStore.calendarItem(withIdentifier: identifier) as? EKReminder else {
            // Deleted or completed on the iPhone since the snapshot.
            await refresh()
            return false
        }

        if reminder.hasRecurrenceRules {
            // EventKit has no per-occurrence span for reminders (EKSpan only
            // exists for EKEvent): setting isCompleted here would close the
            // whole series, not today's occurrence. Refuse and say so.
            errorMessage = "“\(reminder.title ?? "")” è ricorrente: completalo nell’app Promemoria."
            await refresh()
            return false
        }

        do {
            reminder.isCompleted = true
            try eventStore.save(reminder, commit: true)
        } catch {
            errorMessage = "Non è stato possibile completare “\(reminder.title ?? "")”: \(error.localizedDescription)"
            await refresh()
            return false
        }

        await refresh()
        return true
    }

    /// The card payloads for the current snapshot, split to fit the negotiated
    /// GATT write length.
    func snapshots(maximumPayloadBytes: Int) throws -> [Data] {
        return try LumeProtocol.makeReminderSnapshots(
            lists: selectedLists,
            items: items,
            generation: currentGeneration,
            maximumPayloadBytes: maximumPayloadBytes
        )
    }

    // MARK: - EventKit plumbing

    /// What the sort and the labels need from an `EKReminder`, snapshotted on
    /// the fetch callback so the rest of the store stays on the main actor.
    private struct Source {
        let identifier: String
        let calendarIdentifier: String
        let title: String
        let dueDate: Date?
        /// A date-only reminder ("today", no clock time) is not overdue until
        /// the day is over, so the label needs to know whether time is set.
        let hasTime: Bool

        func isOverdue(now: Date, calendar: Calendar) -> Bool {
            guard let dueDate else { return false }
            if hasTime { return dueDate < now }
            return calendar.startOfDay(for: dueDate) < calendar.startOfDay(for: now)
        }
    }

    private func fetchOpenReminders(matching predicate: NSPredicate) async -> [Source] {
        await withCheckedContinuation { continuation in
            eventStore.fetchReminders(matching: predicate) { reminders in
                let sources = (reminders ?? []).compactMap { reminder -> Source? in
                    // predicateForIncompleteReminders already filters completed
                    // ones; this guard also covers items completed elsewhere
                    // between the predicate and the callback.
                    guard !reminder.isCompleted else { return nil }
                    let components = reminder.dueDateComponents
                    return Source(
                        identifier: reminder.calendarItemIdentifier,
                        calendarIdentifier: reminder.calendar?.calendarIdentifier ?? "",
                        title: reminder.title ?? "",
                        dueDate: components.flatMap(Self.date(from:)),
                        hasTime: components?.hour != nil
                    )
                }
                continuation.resume(returning: sources)
            }
        }
    }

    private static func isOrderedBefore(_ lhs: Source, _ rhs: Source, now: Date) -> Bool {
        // Overdue first, then by due date, then the undated ones by title.
        // Total order (identifier last) so two refreshes of the same list
        // always hand out the same handles.
        let lhsRank = lhs.dueDate == nil ? 1 : 0
        let rhsRank = rhs.dueDate == nil ? 1 : 0
        if lhsRank != rhsRank { return lhsRank < rhsRank }

        if let lhsDate = lhs.dueDate, let rhsDate = rhs.dueDate, lhsDate != rhsDate {
            return lhsDate < rhsDate
        }

        let lhsTitle = lhs.title.lowercased()
        let rhsTitle = rhs.title.lowercased()
        if lhsTitle != rhsTitle { return lhsTitle < rhsTitle }
        return lhs.identifier < rhs.identifier
    }

    private func dueLabel(for source: Source, now: Date) -> String {
        guard let dueDate = source.dueDate else { return "" }

        let label: String
        if source.isOverdue(now: now, calendar: calendar) {
            label = "Scaduto"
        } else {
            let dayDelta = calendar.dateComponents(
                [.day],
                from: calendar.startOfDay(for: now),
                to: calendar.startOfDay(for: dueDate)
            ).day ?? 0
            let time = source.hasTime ? " " + formatted(dueDate, as: "HH:mm") : ""
            switch dayDelta {
            case 0:
                label = "Oggi" + time
            case 1:
                label = "Domani" + time
            default:
                // Past tomorrow the clock time is dropped: the day is what the
                // user scans for, and `char due[17]` has no room for both.
                let sameYear = calendar.component(.year, from: dueDate) == calendar.component(.year, from: now)
                label = formatted(dueDate, as: sameYear ? "EEE d MMM" : "d MMM yy")
            }
        }

        // Every form above is short by construction (worst case "mer 22 set",
        // 10 chars); this clamp is the guarantee for an injected locale with
        // longer abbreviations, and mirrors MAX_REMINDER_DUE_CHARS.
        return String(label.prefix(LumeProtocol.maximumReminderDueCharacters))
    }

    private func formatted(_ date: Date, as format: String) -> String {
        let formatter = DateFormatter()
        formatter.calendar = calendar
        formatter.locale = locale
        formatter.timeZone = calendar.timeZone
        formatter.dateFormat = format
        return formatter.string(from: date)
    }

    private func sanitizedSelection(_ identifiers: [String]) -> [String] {
        var unique: [String] = []
        for identifier in identifiers where !unique.contains(identifier) {
            unique.append(identifier)
        }
        // `lists` is empty before the first refresh: filtering then would drop
        // the restored selection, so unknown identifiers survive until
        // refresh() has actually seen what EventKit holds.
        if !lists.isEmpty {
            let known = Set(lists.map(\.id))
            unique = unique.filter { known.contains($0) }
        }
        return Array(unique.prefix(LumeProtocol.maximumReminderLists))
    }

    private func bumpGeneration() {
        // 0 is the firmware's "no snapshot yet", so the wrap skips it.
        currentGeneration = currentGeneration == UInt16.max ? 1 : currentGeneration + 1
    }

    private func updateAccessState() {
        switch EKEventStore.authorizationStatus(for: .reminder) {
        case .fullAccess:
            accessState = .available
        case .denied, .writeOnly:
            // Write-only is useless here: without reading we cannot build a card.
            accessState = .denied
        case .restricted:
            accessState = .restricted
        case .notDetermined:
            accessState = .notDetermined
        @unknown default:
            accessState = .notDetermined
        }
    }

    nonisolated private static func date(from components: DateComponents) -> Date? {
        var calendar = components.calendar ?? Calendar.current
        if let timeZone = components.timeZone { calendar.timeZone = timeZone }
        return calendar.date(from: components)
    }
}

import Foundation

/// A Reminders list the user can promote to a card on the X3.
///
/// `id` is `EKCalendar.calendarIdentifier`: it survives renames, so the
/// selection persisted under `lume.reminders.selectedLists` keeps pointing at
/// the same list even after the user renames it on the iPhone.
struct ReminderList: Identifiable, Equatable, Sendable {
    let id: String
    let title: String
}

/// One open reminder, already shaped for the wire.
///
/// EventKit types stay behind `RemindersStore`: this struct is pure Foundation
/// so the SPM host target (ios/Package.swift) can compile it and `swift test`
/// can exercise the encoder without EventKit, a device or Reminders access.
struct ReminderItem: Identifiable, Equatable, Sendable {
    /// `EKReminder.calendarItemIdentifier` — stable, used to resolve the item
    /// again when the device sends a `reminder.toggle`.
    let id: String
    /// 1...65535 handle the device echoes back; 0 is reserved as "invalid" so
    /// the firmware can reject a zeroed field (CompanionProtocol.h caps).
    let handle: UInt16
    /// Index into the snapshot's `reminderLists`, i.e. which card on the X3.
    let listIndex: Int
    let title: String
    /// Already formatted and localized here: the firmware has no date maths
    /// and no locale data, it only draws the string (`char due[17]`).
    let dueLabel: String
    let isOverdue: Bool
}

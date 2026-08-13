import Foundation

enum TodayEntryKind: String, Codable, Sendable {
    case event
    case reminder
}

struct TodayEntry: Identifiable, Equatable, Codable, Sendable {
    let id: String
    let kind: TodayEntryKind
    let time: String
    let title: String
    let subtitle: String
    let state: String
    let sortDate: Date?
}

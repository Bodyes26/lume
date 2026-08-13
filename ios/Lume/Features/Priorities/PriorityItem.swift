import Foundation

struct PriorityItem: Codable, Equatable, Identifiable, Sendable {
    let id: String
    var title: String
    var note: String
    var isDone: Bool
    let createdAt: Date

    init(
        id: String = UUID().uuidString.lowercased(),
        title: String,
        note: String = "",
        isDone: Bool = false,
        createdAt: Date = .now
    ) {
        self.id = id
        self.title = title
        self.note = note
        self.isDone = isDone
        self.createdAt = createdAt
    }
}

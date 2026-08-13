import Foundation
import Combine

@MainActor
final class PrioritiesStore: ObservableObject {
    nonisolated static let maximumItems = 10

    @Published private(set) var items: [PriorityItem]

    private let defaults: UserDefaults
    private let storageKey = "lume.priorities.v1"
    private let encoder = JSONEncoder()
    private let decoder = JSONDecoder()

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        if
            let data = defaults.data(forKey: storageKey),
            let saved = try? decoder.decode([PriorityItem].self, from: data)
        {
            items = Array(saved.prefix(Self.maximumItems))
        } else {
            items = []
        }
    }

    var activeCount: Int { items.lazy.filter { !$0.isDone }.count }
    var completedCount: Int { items.count - activeCount }
    var canAdd: Bool { items.count < Self.maximumItems }

    @discardableResult
    func add(title: String, note: String) -> Bool {
        let cleanTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
        let cleanNote = note.trimmingCharacters(in: .whitespacesAndNewlines)
        guard canAdd, !cleanTitle.isEmpty else { return false }

        items.append(PriorityItem(title: cleanTitle, note: cleanNote))
        persist()
        return true
    }

    func setDone(id: String, done: Bool) {
        guard let index = items.firstIndex(where: { $0.id == id }) else { return }
        items[index].isDone = done
        persist()
    }

    func toggle(id: String) {
        guard let index = items.firstIndex(where: { $0.id == id }) else { return }
        items[index].isDone.toggle()
        persist()
    }

    func remove(id: String) {
        items.removeAll { $0.id == id }
        persist()
    }

    func move(fromOffsets: IndexSet, toOffset: Int) {
        items.move(fromOffsets: fromOffsets, toOffset: toOffset)
        persist()
    }

    private func persist() {
        guard let data = try? encoder.encode(items) else { return }
        defaults.set(data, forKey: storageKey)
    }
}

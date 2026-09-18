import Combine
import Foundation

@MainActor
final class TrailStore: ObservableObject {
    @Published private(set) var stories: [TrailStoryInfo] = []
    @Published private(set) var saves: [String: Data] = [:]

    private let defaults: UserDefaults
    private static let catalogKey = "lume.trail.catalog.v1"
    private static let savesKey = "lume.trail.saves.v1"

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        loadFromStorage()
    }

    // MARK: - Save and Load

    private func loadFromStorage() {
        if let data = defaults.data(forKey: Self.catalogKey),
           let decoded = try? JSONDecoder().decode([TrailStoryInfo].self, from: data) {
            self.stories = decoded
        } else {
            self.stories = TrailStoryInfo.defaults
        }

        if let savesData = defaults.dictionary(forKey: Self.savesKey) as? [String: Data] {
            self.saves = savesData
        }
    }

    private func persistCatalog() {
        if let encoded = try? JSONEncoder().encode(stories) {
            defaults.set(encoded, forKey: Self.catalogKey)
        }
    }

    private func persistSaves() {
        defaults.set(saves, forKey: Self.savesKey)
    }

    // MARK: - Actions from Device

    func handleDeviceSave(storyId: String, chapter: Int, day: Int = 1, saveData: Data) {
        saves[storyId] = saveData
        persistSaves()

        if let index = stories.firstIndex(where: { $0.id == storyId }) {
            stories[index].currentChapter = chapter
            stories[index].currentDay = day
            if chapter >= stories[index].chapterCount {
                stories[index].isCompleted = true
            }
            persistCatalog()
        }
    }

    func markInstalled(storyId: String, isInstalled: Bool) {
        if let index = stories.firstIndex(where: { $0.id == storyId }) {
            stories[index].isInstalled = isInstalled
            persistCatalog()
        }
    }

    func getSave(for storyId: String) -> Data? {
        saves[storyId]
    }

    func resetStoryProgress(storyId: String) {
        saves.removeValue(forKey: storyId)
        persistSaves()

        if let index = stories.firstIndex(where: { $0.id == storyId }) {
            stories[index].currentChapter = 0
            stories[index].currentDay = 1
            stories[index].isCompleted = false
            persistCatalog()
        }
    }
}

import Foundation
import SwiftUI

@MainActor
final class SleepStore: ObservableObject {
    @Published var config: SleepConfig {
        didSet {
            save()
        }
    }

    @Published private(set) var isSyncing: Bool = false
    @Published private(set) var lastSyncedAt: Date?
    @Published private(set) var errorMessage: String?

    private let defaults: UserDefaults
    static let configKey = "lume.sleep.config.v1"
    static let lastSyncKey = "lume.sleep.lastSync"

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        if let data = defaults.data(forKey: Self.configKey),
           let saved = try? JSONDecoder().decode(SleepConfig.self, from: data) {
            self.config = saved
        } else {
            self.config = SleepConfig(
                mode: .auto,
                customTitle: "Scrivania di Maurizio",
                customQuote: "La semplicità è la suprema sofisticazione.",
                customAuthor: "Leonardo da Vinci",
                showBattery: true,
                showTemperature: true,
                showNextEvent: true,
                showReadingStats: true,
                showSleepTime: true
            )
        }

        if let syncDate = defaults.object(forKey: Self.lastSyncKey) as? Date {
            self.lastSyncedAt = syncDate
        }
    }

    func applyPreset(_ preset: PresetQuote) {
        config.customQuote = preset.text
        config.customAuthor = preset.author
    }

    func markSyncCompleted() {
        let now = Date.now
        lastSyncedAt = now
        defaults.set(now, forKey: Self.lastSyncKey)
        isSyncing = false
    }

    func markSyncFailed(_ message: String) {
        errorMessage = message
        isSyncing = false
    }

    func resetToDefaults() {
        config = SleepConfig(
            mode: .auto,
            customTitle: "",
            customQuote: "La semplicità è la suprema sofisticazione.",
            customAuthor: "Leonardo da Vinci",
            showBattery: true,
            showTemperature: true,
            showNextEvent: true,
            showReadingStats: true,
            showSleepTime: true
        )
    }

    private func save() {
        if let data = try? JSONEncoder().encode(config) {
            defaults.set(data, forKey: Self.configKey)
        }
    }
}

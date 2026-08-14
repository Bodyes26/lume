import Combine
import Foundation

@MainActor
final class AppModel: ObservableObject {
    let reminders: RemindersStore
    let today: TodayStore
    let bluetooth: LumeBluetoothManager

    init(defaults: UserDefaults = .standard) {
        let reminders = RemindersStore(defaults: defaults)
        let today = TodayStore()
        self.reminders = reminders
        self.today = today
        bluetooth = LumeBluetoothManager(
            reminders: reminders,
            today: today,
            defaults: defaults
        )
    }
}

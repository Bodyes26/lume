import Combine
import Foundation

@MainActor
final class AppModel: ObservableObject {
    let reminders: RemindersStore
    let today: TodayStore
    let sleep: SleepStore
    let trail: TrailStore
    let bluetooth: LumeBluetoothManager

    init(defaults: UserDefaults = .standard) {
        let reminders = RemindersStore(defaults: defaults)
        let today = TodayStore()
        self.reminders = reminders
        let sleep = SleepStore(defaults: defaults)
        self.sleep = sleep
        self.today = today
        let trail = TrailStore(defaults: defaults)
        self.trail = trail
        bluetooth = LumeBluetoothManager(
            reminders: reminders,
            today: today,
            trail: trail,
            defaults: defaults
        )
    }
}

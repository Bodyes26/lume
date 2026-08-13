import Combine
import Foundation

@MainActor
final class AppModel: ObservableObject {
    let priorities: PrioritiesStore
    let today: TodayStore
    let bluetooth: LumeBluetoothManager

    init(defaults: UserDefaults = .standard) {
        let priorities = PrioritiesStore(defaults: defaults)
        let today = TodayStore()
        self.priorities = priorities
        self.today = today
        bluetooth = LumeBluetoothManager(
            priorities: priorities,
            today: today,
            defaults: defaults
        )
    }
}

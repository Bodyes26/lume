import Combine
import Foundation

@MainActor
final class AppModel: ObservableObject {
    let priorities: PrioritiesStore
    let bluetooth: LumeBluetoothManager

    init(defaults: UserDefaults = .standard) {
        let priorities = PrioritiesStore(defaults: defaults)
        self.priorities = priorities
        bluetooth = LumeBluetoothManager(priorities: priorities, defaults: defaults)
    }
}

import SwiftUI

@main
@MainActor
struct LumeApp: App {
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup {
            RootView(
                bluetooth: model.bluetooth,
                reminders: model.reminders,
                sleep: model.sleep,
                today: model.today,
                trail: model.trail
            )
        }
    }
}

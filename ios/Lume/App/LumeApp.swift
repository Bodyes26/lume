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
                today: model.today
            )
        }
    }
}

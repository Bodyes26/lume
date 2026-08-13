import SwiftUI

@main
@MainActor
struct LumeApp: App {
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup {
            RootView(
                bluetooth: model.bluetooth,
                priorities: model.priorities
            )
        }
    }
}

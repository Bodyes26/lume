// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "LumeProtocol",
    platforms: [.macOS(.v15)],
    products: [
        .library(name: "Lume", targets: ["Lume"])
    ],
    targets: [
        .target(
            name: "Lume",
            path: "Lume",
            exclude: [
                "App",
                "Bluetooth/LumeBluetoothManager.swift",
                "Design",
                "Features/Reminders/RemindersStore.swift",
                "Features/Reminders/RemindersView.swift",
                "Features/Reminders/ReminderListSelectionView.swift",
                "Features/Today/TodayStore.swift",
                "Features/Today/TodayView.swift",
                "Features/Sleep",
                "Features/Trail/TrailStore.swift",
                "Features/Trail/TrailStoryCard.swift",
                "Features/Trail/TrailView.swift",
                "Resources"
            ],
            sources: [
                "Bluetooth/LumeProtocol.swift",
                "Features/Reminders/ReminderItem.swift",
                "Features/Today/TodayEntry.swift",
                "Features/Today/TodayProjection.swift",
                "Features/Trail/TrailStoryInfo.swift"
            ]
        ),
        .testTarget(
            name: "LumeTests",
            dependencies: ["Lume"],
            path: "LumeTests"
        )
    ]
)

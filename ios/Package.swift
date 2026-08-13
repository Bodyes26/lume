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
                "Features/Priorities/PrioritiesStore.swift",
                "Features/Priorities/PrioritiesView.swift",
                "Resources"
            ],
            sources: [
                "Bluetooth/LumeProtocol.swift",
                "Features/Priorities/PriorityItem.swift"
            ]
        ),
        .testTarget(
            name: "LumeTests",
            dependencies: ["Lume"],
            path: "LumeTests"
        )
    ]
)

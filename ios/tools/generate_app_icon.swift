#!/usr/bin/env swift

import AppKit
import Foundation

let sourcePath = CommandLine.arguments.dropFirst().first
    ?? ("\(NSHomeDirectory())/Downloads/Lume_color_1024.png")

guard FileManager.default.fileExists(atPath: sourcePath) else {
    fputs("Source icon file not found at \(sourcePath)\n", stderr)
    exit(1)
}

guard let image = NSImage(contentsOfFile: sourcePath) else {
    fputs("Could not load image at \(sourcePath)\n", stderr)
    exit(1)
}

let targets: [(path: String, width: Int, height: Int)] = [
    ("Lume/Resources/Assets.xcassets/AppIcon.appiconset/Lume-1024.png", 1024, 1024),
    ("Lume/Resources/AppIcon60@2x.png", 120, 120),
    ("Lume/Resources/AppIcon60@3x.png", 180, 180)
]

for target in targets {
    let size = NSSize(width: target.width, height: target.height)
    guard let bitmap = NSBitmapImageRep(
        bitmapDataPlanes: nil,
        pixelsWide: target.width,
        pixelsHigh: target.height,
        bitsPerSample: 8,
        samplesPerPixel: 4,
        hasAlpha: true,
        isPlanar: false,
        colorSpaceName: .deviceRGB,
        bytesPerRow: 0,
        bitsPerPixel: 0
    ) else {
        continue
    }
    
    bitmap.size = size
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
    NSGraphicsContext.current?.imageInterpolation = .high
    
    image.draw(in: NSRect(origin: .zero, size: size), from: .zero, operation: .copy, fraction: 1.0)
    NSGraphicsContext.restoreGraphicsState()
    
    if let png = bitmap.representation(using: .png, properties: [:]) {
        let url = URL(fileURLWithPath: target.path)
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? png.write(to: url, options: .atomic)
        print("Wrote \(target.width)x\(target.height) icon to \(url.path)")
    }
}

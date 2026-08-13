#!/usr/bin/env swift

import AppKit
import Foundation

let output = CommandLine.arguments.dropFirst().first
    ?? "Lume/Resources/Assets.xcassets/AppIcon.appiconset/Lume-1024.png"
let size = NSSize(width: 1024, height: 1024)
guard let bitmap = NSBitmapImageRep(
    bitmapDataPlanes: nil,
    pixelsWide: 1024,
    pixelsHigh: 1024,
    bitsPerSample: 8,
    samplesPerPixel: 4,
    hasAlpha: true,
    isPlanar: false,
    colorSpaceName: .deviceRGB,
    bytesPerRow: 0,
    bitsPerPixel: 0
) else {
    fputs("Could not allocate Lume app icon bitmap\n", stderr)
    exit(1)
}
bitmap.size = size
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
NSGraphicsContext.current?.imageInterpolation = .high

let paper = NSColor(srgbRed: 0.953, green: 0.929, blue: 0.859, alpha: 1)
let ink = NSColor(srgbRed: 0.102, green: 0.098, blue: 0.078, alpha: 1)
let light = NSColor(srgbRed: 0.780, green: 0.443, blue: 0.098, alpha: 1)

paper.setFill()
NSBezierPath(rect: NSRect(origin: .zero, size: size)).fill()

func roundedRect(_ rect: NSRect, radius: CGFloat, color: NSColor) {
    color.setFill()
    NSBezierPath(roundedRect: rect, xRadius: radius, yRadius: radius).fill()
}

// The same L + three rays used on the X3, redrawn at icon resolution.
roundedRect(NSRect(x: 248, y: 230, width: 152, height: 566), radius: 76, color: ink)
roundedRect(NSRect(x: 248, y: 230, width: 468, height: 152), radius: 76, color: ink)

func ray(from start: NSPoint, to end: NSPoint) {
    let path = NSBezierPath()
    path.move(to: start)
    path.line(to: end)
    path.lineWidth = 42
    path.lineCapStyle = .round
    light.setStroke()
    path.stroke()
}

ray(from: NSPoint(x: 600, y: 652), to: NSPoint(x: 810, y: 652))
ray(from: NSPoint(x: 558, y: 728), to: NSPoint(x: 706, y: 876))
ray(from: NSPoint(x: 600, y: 550), to: NSPoint(x: 748, y: 402))

NSGraphicsContext.restoreGraphicsState()

guard let png = bitmap.representation(using: .png, properties: [:]) else {
    fputs("Could not render Lume app icon\n", stderr)
    exit(1)
}

let url = URL(fileURLWithPath: output)
try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
try png.write(to: url, options: .atomic)
print("Wrote \(url.path)")

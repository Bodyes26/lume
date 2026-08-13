import SwiftUI

struct LumeMark: View {
    var body: some View {
        GeometryReader { proxy in
            let side = min(proxy.size.width, proxy.size.height)
            ZStack {
                LumeCoronaShape()
                    .fill(LumeTheme.ink)
                LumeStarShape()
                    .fill(LumeTheme.ink)
                LumeTowerShape()
                    .fill(LumeTheme.ink)
                LumeHorizonShape()
                    .stroke(
                        LumeTheme.ink,
                        style: StrokeStyle(lineWidth: side * (2.3 / 120.0), lineCap: .round)
                    )
            }
            .frame(width: side, height: side)
            .position(x: proxy.size.width / 2, y: proxy.size.height / 2)
        }
        .aspectRatio(1, contentMode: .fit)
        .accessibilityHidden(true)
    }
}

private struct LumeCoronaShape: Shape {
    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height)
        let s = unit / 120.0
        var path = Path()
        path.addEllipse(in: CGRect(x: (60 * s) - (2.05 * s), y: (25.2 * s) - (2.05 * s), width: 4.1 * s, height: 4.1 * s))
        path.addEllipse(in: CGRect(x: (46.9 * s) - (0.95 * s), y: (28.4 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (38.6 * s) - (0.95 * s), y: (34.2 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (32.8 * s) - (0.95 * s), y: (42.4 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (30.8 * s) - (2.05 * s), y: (54 * s) - (2.05 * s), width: 4.1 * s, height: 4.1 * s))
        path.addEllipse(in: CGRect(x: (32.8 * s) - (0.95 * s), y: (65.6 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (38.6 * s) - (0.95 * s), y: (73.8 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (46.9 * s) - (0.95 * s), y: (79.6 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (73.1 * s) - (0.95 * s), y: (79.6 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (81.4 * s) - (0.95 * s), y: (73.8 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (87.2 * s) - (0.95 * s), y: (65.6 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (89.2 * s) - (2.05 * s), y: (54 * s) - (2.05 * s), width: 4.1 * s, height: 4.1 * s))
        path.addEllipse(in: CGRect(x: (87.2 * s) - (0.95 * s), y: (42.4 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (81.4 * s) - (0.95 * s), y: (34.2 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        path.addEllipse(in: CGRect(x: (73.1 * s) - (0.95 * s), y: (28.4 * s) - (0.95 * s), width: 1.9 * s, height: 1.9 * s))
        return path
    }
}

private struct LumeStarShape: Shape {
    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height)
        let s = unit / 120.0
        var path = Path()
        path.move(to: CGPoint(x: 60 * s, y: 31 * s))
        path.addLine(to: CGPoint(x: 62.35 * s, y: 50.4 * s))
        path.addLine(to: CGPoint(x: 85 * s, y: 54 * s))
        path.addLine(to: CGPoint(x: 62.35 * s, y: 57.6 * s))
        path.addLine(to: CGPoint(x: 60 * s, y: 64.5 * s))
        path.addLine(to: CGPoint(x: 57.65 * s, y: 57.6 * s))
        path.addLine(to: CGPoint(x: 35 * s, y: 54 * s))
        path.addLine(to: CGPoint(x: 57.65 * s, y: 50.4 * s))
        path.closeSubpath()
        return path
    }
}

private struct LumeTowerShape: Shape {
    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height)
        let s = unit / 120.0
        var path = Path()
        path.move(to: CGPoint(x: 60 * s, y: 64.5 * s))
        path.addLine(to: CGPoint(x: 64.55 * s, y: 94.45 * s))
        path.addLine(to: CGPoint(x: 55.45 * s, y: 94.45 * s))
        path.closeSubpath()
        return path
    }
}

private struct LumeHorizonShape: Shape {
    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height)
        let s = unit / 120.0
        var path = Path()
        path.move(to: CGPoint(x: 7.8 * s, y: 107.2 * s))
        path.addQuadCurve(
            to: CGPoint(x: 112.2 * s, y: 107.2 * s),
            control: CGPoint(x: 60.0 * s, y: 81.5 * s)
        )
        return path
    }
}

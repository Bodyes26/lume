import SwiftUI

struct LumeMark: View {
    var body: some View {
        GeometryReader { proxy in
            let side = min(proxy.size.width, proxy.size.height)
            ZStack {
                LumeLetterShape()
                    .fill(LumeTheme.ink)
                LumeRaysShape()
                    .stroke(
                        LumeTheme.light,
                        style: StrokeStyle(lineWidth: side * 0.055, lineCap: .round)
                    )
            }
            .frame(width: side, height: side)
            .position(x: proxy.size.width / 2, y: proxy.size.height / 2)
        }
        .aspectRatio(1, contentMode: .fit)
        .accessibilityHidden(true)
    }
}

private struct LumeLetterShape: Shape {
    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height)
        var path = Path()
        path.addRoundedRect(
            in: CGRect(x: unit * 0.22, y: unit * 0.18, width: unit * 0.16, height: unit * 0.64),
            cornerSize: CGSize(width: unit * 0.08, height: unit * 0.08)
        )
        path.addRoundedRect(
            in: CGRect(x: unit * 0.22, y: unit * 0.66, width: unit * 0.48, height: unit * 0.16),
            cornerSize: CGSize(width: unit * 0.08, height: unit * 0.08)
        )
        return path
    }
}

private struct LumeRaysShape: Shape {
    func path(in rect: CGRect) -> Path {
        let unit = min(rect.width, rect.height)
        var path = Path()
        path.move(to: CGPoint(x: unit * 0.59, y: unit * 0.40))
        path.addLine(to: CGPoint(x: unit * 0.84, y: unit * 0.40))
        path.move(to: CGPoint(x: unit * 0.55, y: unit * 0.33))
        path.addLine(to: CGPoint(x: unit * 0.73, y: unit * 0.15))
        path.move(to: CGPoint(x: unit * 0.59, y: unit * 0.50))
        path.addLine(to: CGPoint(x: unit * 0.77, y: unit * 0.68))
        return path
    }
}

import SwiftUI

struct TrailStoryCard: View {
    let story: TrailStoryInfo
    let isDeviceReady: Bool
    let onTransfer: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: LumeSpace.small) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                    Text(story.title)
                        .font(.system(.headline, design: .serif, weight: .bold))
                        .foregroundStyle(LumeTheme.ink)

                    Text(story.description)
                        .font(.system(.subheadline, design: .serif))
                        .foregroundStyle(LumeTheme.secondaryInk)
                        .lineLimit(2)
                }

                Spacer()

                statusBadge
            }

            // Progress bar if started
            if story.currentChapter > 0 || story.isCompleted {
                VStack(alignment: .leading, spacing: 2) {
                    GeometryReader { geo in
                        ZStack(alignment: .leading) {
                            RoundedRectangle(cornerRadius: 2)
                                .fill(LumeTheme.hairline)
                                .frame(height: 4)

                            RoundedRectangle(cornerRadius: 2)
                                .fill(story.isCompleted ? LumeTheme.positive : LumeTheme.light)
                                .frame(width: geo.size.width * CGFloat(story.progressFraction), height: 4)
                        }
                    }
                    .frame(height: 4)

                    HStack {
                        Text(story.progressText)
                            .font(.system(.caption2, design: .monospaced))
                            .foregroundStyle(LumeTheme.secondaryInk)

                        Spacer()

                        Text("\(story.fileSize / 1024) KB")
                            .font(.system(.caption2, design: .monospaced))
                            .foregroundStyle(LumeTheme.secondaryInk)
                    }
                }
                .padding(.top, LumeSpace.xSmall)
            } else {
                HStack {
                    Text("\(story.chapterCount) capitoli · \(story.fileSize / 1024) KB")
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundStyle(LumeTheme.secondaryInk)

                    Spacer()

                    if !story.isInstalled && isDeviceReady {
                        Button(action: onTransfer) {
                            Text("Invia a Lume")
                                .font(.system(.caption, design: .monospaced, weight: .bold))
                                .foregroundStyle(LumeTheme.light)
                        }
                    }
                }
                .padding(.top, LumeSpace.xSmall)
            }
        }
        .padding(LumeSpace.regular)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(LumeTheme.raisedPaper)
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .strokeBorder(LumeTheme.hairline, lineWidth: 1)
                )
        )
    }

    @ViewBuilder
    private var statusBadge: some View {
        if story.isCompleted {
            Text("Completata")
                .font(.system(.caption2, design: .monospaced, weight: .bold))
                .padding(.horizontal, 8)
                .padding(.vertical, 3)
                .background(LumeTheme.positive.opacity(0.15))
                .foregroundStyle(LumeTheme.positive)
                .clipShape(Capsule())
        } else if story.isInstalled {
            Text("Sul device")
                .font(.system(.caption2, design: .monospaced, weight: .bold))
                .padding(.horizontal, 8)
                .padding(.vertical, 3)
                .background(LumeTheme.hairline.opacity(0.5))
                .foregroundStyle(LumeTheme.ink)
                .clipShape(Capsule())
        } else {
            Text("Disponibile")
                .font(.system(.caption2, design: .monospaced))
                .padding(.horizontal, 8)
                .padding(.vertical, 3)
                .background(LumeTheme.hairline.opacity(0.25))
                .foregroundStyle(LumeTheme.secondaryInk)
                .clipShape(Capsule())
        }
    }
}

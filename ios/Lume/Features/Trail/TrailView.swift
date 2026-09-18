import SwiftUI

struct TrailView: View {
    @ObservedObject var store: TrailStore
    let isDeviceReady: Bool
    let onSyncCatalog: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: LumeSpace.medium) {
            sectionHeader

            VStack(spacing: LumeSpace.compact) {
                ForEach(store.stories) { story in
                    TrailStoryCard(
                        story: story,
                        isDeviceReady: isDeviceReady,
                        onTransfer: onSyncCatalog
                    )
                }
            }

            footerNote
        }
    }

    private var sectionHeader: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text("Avventure")
                    .font(.system(.title2, design: .serif, weight: .bold))
                    .foregroundStyle(LumeTheme.ink)

                Text("Storie interattive da giocare su Lume")
                    .font(.system(.subheadline, design: .serif))
                    .foregroundStyle(LumeTheme.secondaryInk)
            }

            Spacer()

            if isDeviceReady {
                Button(action: onSyncCatalog) {
                    Image(systemName: "arrow.triangle.2.circlepath")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(LumeTheme.light)
                        .padding(8)
                        .background(
                            Circle()
                                .fill(LumeTheme.raisedPaper)
                                .overlay(Circle().strokeBorder(LumeTheme.hairline, lineWidth: 1))
                        )
                }
                .accessibilityLabel("Sincronizza storie")
            }
        }
    }

    private var footerNote: some View {
        Text("Le avventure si giocano per intero su Lume X3 con i tasti fisici. I progressi e i salvataggi vengono sincronizzati automaticamente con l'app.")
            .font(.system(.caption, design: .serif))
            .foregroundStyle(LumeTheme.secondaryInk)
            .padding(.top, LumeSpace.xSmall)
    }
}

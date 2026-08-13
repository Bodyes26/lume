import SwiftUI

struct PrioritiesView: View {
    @ObservedObject var store: PrioritiesStore
    let isDeviceReady: Bool

    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @FocusState private var focusedField: Field?
    @State private var isComposing = false
    @State private var draftTitle = ""
    @State private var draftNote = ""

    private enum Field { case title, note }

    var body: some View {
        VStack(alignment: .leading, spacing: LumeSpace.medium) {
            sectionHeader

            if isComposing {
                composer
                    .transition(.opacity.combined(with: .move(edge: .top)))
            }

            if store.items.isEmpty {
                emptyState
            } else {
                VStack(spacing: 0) {
                    ForEach(store.items) { item in
                        PriorityRow(
                            item: item,
                            onToggle: { store.toggle(id: item.id) },
                            onDelete: { store.remove(id: item.id) }
                        )
                        if item.id != store.items.last?.id {
                            Divider().overlay(LumeTheme.hairline.opacity(0.75))
                        }
                    }
                }
            }

            syncHint
        }
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: isComposing)
    }

    private var sectionHeader: some View {
        VStack(alignment: .leading, spacing: LumeSpace.compact) {
            HStack(alignment: .firstTextBaseline) {
                VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                    Text("Priorità")
                        .font(.system(size: 30, weight: .medium, design: .serif))
                        .tracking(-0.6)
                    if store.items.isEmpty {
                        Text("La giornata comincia da ciò che scegli.")
                    } else {
                        Text("\(store.activeCount) da fare · \(store.completedCount) completate")
                    }
                }
                .font(.subheadline)
                .foregroundStyle(LumeTheme.secondaryInk)

                Spacer(minLength: LumeSpace.regular)

                if !isComposing {
                    Button {
                        isComposing = true
                        focusedField = .title
                    } label: {
                        Label("Aggiungi", systemImage: "plus")
                    }
                    .buttonStyle(.bordered)
                    .buttonBorderShape(.capsule)
                    .disabled(!store.canAdd)
                }
            }

            if !store.canAdd {
                Text("Hai raggiunto il limite di 10 priorità dell’X3.")
                    .font(.footnote)
                    .foregroundStyle(LumeTheme.secondaryInk)
            }
        }
    }

    private var composer: some View {
        VStack(alignment: .leading, spacing: LumeSpace.regular) {
            TextField("Cosa conta adesso?", text: $draftTitle, axis: .vertical)
                .font(.system(.title3, design: .serif, weight: .medium))
                .focused($focusedField, equals: .title)
                .submitLabel(.next)
                .onSubmit { focusedField = .note }

            Divider().overlay(LumeTheme.hairline)

            TextField("Una nota, se serve", text: $draftNote, axis: .vertical)
                .font(.body)
                .focused($focusedField, equals: .note)
                .submitLabel(.done)
                .onSubmit(saveDraft)

            HStack {
                Button("Annulla", role: .cancel) { cancelDraft() }
                    .buttonStyle(.plain)
                    .foregroundStyle(LumeTheme.secondaryInk)

                Spacer()

                Button("Salva") { saveDraft() }
                    .buttonStyle(.borderedProminent)
                    .buttonBorderShape(.capsule)
                    .disabled(draftTitle.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
        .padding(LumeSpace.regular)
        .background(LumeTheme.raisedPaper)
        .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 18, style: .continuous)
                .stroke(LumeTheme.hairline, lineWidth: 1)
        }
    }

    private var emptyState: some View {
        VStack(alignment: .leading, spacing: LumeSpace.small) {
            Text("Niente da inseguire.")
                .font(.system(.title3, design: .serif, weight: .medium))
            Text("Aggiungi fino a dieci cose. Quando apri Priorità sull’X3, Lume te le porta sul vetro.")
                .font(.body)
                .foregroundStyle(LumeTheme.secondaryInk)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(.vertical, LumeSpace.medium)
    }

    private var syncHint: some View {
        HStack(alignment: .top, spacing: LumeSpace.small) {
            Image(systemName: isDeviceReady ? "arrow.triangle.2.circlepath" : "iphone.and.arrow.forward")
                .foregroundStyle(isDeviceReady ? LumeTheme.positive : LumeTheme.secondaryInk)
            Text(isDeviceReady
                 ? "Apri Priorità sull’X3: il dispositivo chiederà la copia aggiornata."
                 : "Le modifiche restano sull’iPhone finché Lume X3 non è collegato.")
                .fixedSize(horizontal: false, vertical: true)
        }
        .font(.footnote)
        .foregroundStyle(LumeTheme.secondaryInk)
        .accessibilityElement(children: .combine)
    }

    private func saveDraft() {
        guard store.add(title: draftTitle, note: draftNote) else { return }
        draftTitle = ""
        draftNote = ""
        focusedField = nil
        isComposing = false
    }

    private func cancelDraft() {
        draftTitle = ""
        draftNote = ""
        focusedField = nil
        isComposing = false
    }
}

private struct PriorityRow: View {
    let item: PriorityItem
    let onToggle: () -> Void
    let onDelete: () -> Void

    var body: some View {
        HStack(alignment: .top, spacing: LumeSpace.compact) {
            Button(action: onToggle) {
                Image(systemName: item.isDone ? "checkmark.circle.fill" : "circle")
                    .font(.title3)
                    .foregroundStyle(item.isDone ? LumeTheme.positive : LumeTheme.ink)
                    .frame(width: 32, height: 32)
            }
            .buttonStyle(.plain)
            .accessibilityLabel(item.isDone ? "Segna da fare" : "Segna completata")

            Button(action: onToggle) {
                VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                    Text(item.title)
                        .font(.body.weight(.medium))
                        .strikethrough(item.isDone, color: LumeTheme.secondaryInk)
                    if !item.note.isEmpty {
                        Text(item.note)
                            .font(.subheadline)
                            .foregroundStyle(LumeTheme.secondaryInk)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)

            Button(role: .destructive, action: onDelete) {
                Image(systemName: "trash")
                    .frame(width: 32, height: 32)
            }
            .buttonStyle(.plain)
            .foregroundStyle(LumeTheme.secondaryInk)
            .accessibilityLabel("Elimina \(item.title)")
        }
        .padding(.vertical, LumeSpace.compact)
        .opacity(item.isDone ? 0.62 : 1)
    }
}

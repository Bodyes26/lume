import SwiftUI
import UIKit

/// Prende il posto della vecchia `PrioritiesView`: da questa versione la fonte di
/// verità sono i Promemoria di Apple, quindi qui non si crea né si spunta nulla.
/// La spunta esiste solo sull'X3 (`reminder.toggle`) e in Promemoria/Siri.
struct RemindersView: View {
    @ObservedObject var store: RemindersStore
    let isDeviceReady: Bool
    let onSync: () -> Void

    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.openURL) private var openURL
    @State private var isChoosingLists = false
    // `RemindersStore` pubblica lo stato dei dati, non quello della richiesta in
    // corso: il progresso è locale alla vista, come il resto delle sue @State.
    @State private var isWorking = false

    var body: some View {
        VStack(alignment: .leading, spacing: LumeSpace.medium) {
            sectionHeader

            switch store.accessState {
            case .notDetermined:
                permissionPrompt
            case .denied, .restricted:
                deniedAccess
            case .available:
                authorizedContent
            }

            if let error = store.errorMessage {
                Text(error)
                    .font(.footnote)
                    .foregroundStyle(LumeTheme.secondaryInk)
                    .fixedSize(horizontal: false, vertical: true)
                    .accessibilityLabel("Errore: \(error)")
            }
        }
        .animation(reduceMotion ? nil : .easeOut(duration: 0.22), value: store.accessState)
        .sheet(isPresented: $isChoosingLists) {
            ReminderListSelectionView(store: store, onCommit: onSync)
        }
    }

    private var sectionHeader: some View {
        HStack(alignment: .firstTextBaseline, spacing: LumeSpace.regular) {
            VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                Text("Promemoria")
                    .font(.system(size: 30, weight: .medium, design: .serif))
                    .tracking(-0.6)
                Text(headerDetail)
                    .font(.subheadline)
                    .foregroundStyle(LumeTheme.secondaryInk)
            }

            Spacer(minLength: 0)

            if store.accessState == .available {
                // Un tap apre la scelta, il secondo tocca la lista: le schede
                // dell'X3 restano a due tap da qui, senza menu annidati.
                Button {
                    isChoosingLists = true
                } label: {
                    Label("Liste", systemImage: "line.3.horizontal.decrease")
                }
                .buttonStyle(.bordered)
                .buttonBorderShape(.capsule)
                .accessibilityLabel("Scegli le liste da mostrare sull’X3")

                Button(action: refresh) {
                    if isWorking {
                        ProgressView()
                            .controlSize(.small)
                    } else {
                        Label("Aggiorna", systemImage: "arrow.clockwise")
                            .labelStyle(.iconOnly)
                    }
                }
                .buttonStyle(.bordered)
                .buttonBorderShape(.capsule)
                .disabled(isWorking)
                .accessibilityLabel(isWorking ? "Aggiornamento promemoria in corso" : "Aggiorna promemoria")
            }
        }
    }

    private var headerDetail: String {
        guard store.accessState == .available else {
            return "I promemoria di Apple, sul vetro"
        }
        if store.selectedListIDs.isEmpty {
            return "Nessuna lista scelta"
        }
        return "\(openCountLabel) · \(listCountLabel) sul vetro"
    }

    private var openCountLabel: String {
        store.items.count == 1 ? "1 da fare" : "\(store.items.count) da fare"
    }

    private var listCountLabel: String {
        store.selectedListIDs.count == 1 ? "1 lista" : "\(store.selectedListIDs.count) liste"
    }

    private var permissionPrompt: some View {
        VStack(alignment: .leading, spacing: LumeSpace.regular) {
            Text("Le tue liste, senza copie.")
                .font(.system(.title3, design: .serif, weight: .medium))
            Text("Lume legge i promemoria ancora aperti dalle liste che scegli tu. Non tiene un elenco a parte: quello che vedi qui è quello che c’è in Promemoria.")
                .font(.body)
                .foregroundStyle(LumeTheme.secondaryInk)
                .fixedSize(horizontal: false, vertical: true)
            Button("Consenti accesso") {
                isWorking = true
                Task {
                    await store.requestAccessAndRefresh()
                    isWorking = false
                    if store.accessState == .available { onSync() }
                }
            }
            .buttonStyle(.borderedProminent)
            .buttonBorderShape(.capsule)
            .disabled(isWorking)
        }
        .padding(.vertical, LumeSpace.small)
    }

    private var deniedAccess: some View {
        VStack(alignment: .leading, spacing: LumeSpace.regular) {
            Text("I promemoria restano dove sono.")
                .font(.system(.title3, design: .serif, weight: .medium))
            Text(store.accessState == .restricted
                 ? "Questo iPhone limita l’accesso a Promemoria: senza quel permesso l’X3 non può mostrare le liste."
                 : "Per portare le liste sull’X3, abilita Promemoria nelle Impostazioni di Lume.")
                .font(.body)
                .foregroundStyle(LumeTheme.secondaryInk)
                .fixedSize(horizontal: false, vertical: true)
            Button("Apri Impostazioni") {
                guard let url = URL(string: UIApplication.openSettingsURLString) else { return }
                openURL(url)
            }
            .buttonStyle(.bordered)
            .buttonBorderShape(.capsule)
        }
        .padding(.vertical, LumeSpace.small)
    }

    @ViewBuilder
    private var authorizedContent: some View {
        if store.selectedListIDs.isEmpty {
            noListsChosen
        } else if store.items.isEmpty {
            nothingOpen
        } else {
            VStack(alignment: .leading, spacing: LumeSpace.medium) {
                ForEach(groups) { group in
                    listGroup(group)
                }
            }
            readOnlyNote
        }

        syncHint
    }

    private var noListsChosen: some View {
        VStack(alignment: .leading, spacing: LumeSpace.regular) {
            Text("Scegli quali liste meritano il vetro.")
                .font(.system(.title3, design: .serif, weight: .medium))
            Text("Fino a quattro liste di Promemoria diventano schede sull’X3, nell’ordine che decidi tu.")
                .font(.body)
                .foregroundStyle(LumeTheme.secondaryInk)
                .fixedSize(horizontal: false, vertical: true)
            Button("Scegli le liste") { isChoosingLists = true }
                .buttonStyle(.borderedProminent)
                .buttonBorderShape(.capsule)
        }
        .padding(.vertical, LumeSpace.small)
    }

    private var nothingOpen: some View {
        VStack(alignment: .leading, spacing: LumeSpace.small) {
            Text("Niente di aperto.")
                .font(.system(.title3, design: .serif, weight: .medium))
            Text("\(chosenListNames): tutto spuntato. Il prossimo promemoria che aggiungi in Promemoria o con Siri compare qui e sull’X3.")
                .font(.body)
                .foregroundStyle(LumeTheme.secondaryInk)
                .fixedSize(horizontal: false, vertical: true)
            Button("Cambia liste") { isChoosingLists = true }
                .buttonStyle(.bordered)
                .buttonBorderShape(.capsule)
        }
        .padding(.vertical, LumeSpace.small)
    }

    private var readOnlyNote: some View {
        HStack(alignment: .top, spacing: LumeSpace.small) {
            Image(systemName: "square.and.pencil")
                .foregroundStyle(LumeTheme.secondaryInk)
            Text("Qui si legge soltanto: si scrive da Promemoria o con Siri. Sull’X3 puoi spuntare, e la spunta torna in Promemoria.")
                .fixedSize(horizontal: false, vertical: true)
        }
        .font(.footnote)
        .foregroundStyle(LumeTheme.secondaryInk)
        .accessibilityElement(children: .combine)
    }

    private var syncHint: some View {
        HStack(alignment: .top, spacing: LumeSpace.small) {
            Image(systemName: isDeviceReady ? "checklist" : "iphone.and.arrow.forward")
                .foregroundStyle(isDeviceReady ? LumeTheme.positive : LumeTheme.secondaryInk)
            Text(isDeviceReady
                 ? "Apri Promemoria sull’X3: ogni lista scelta è una scheda."
                 : "Le liste restano pronte sull’iPhone finché Lume X3 non è collegato.")
                .fixedSize(horizontal: false, vertical: true)
        }
        .font(.footnote)
        .foregroundStyle(LumeTheme.secondaryInk)
        .accessibilityElement(children: .combine)
    }

    private func listGroup(_ group: ReminderGroup) -> some View {
        VStack(alignment: .leading, spacing: LumeSpace.small) {
            Text(group.title.uppercased(with: .current))
                .font(.caption.weight(.semibold))
                .foregroundStyle(LumeTheme.secondaryInk)
                .accessibilityLabel("Lista \(group.title)")

            VStack(spacing: 0) {
                ForEach(Array(group.items.enumerated()), id: \.element.id) { index, item in
                    reminderRow(item)
                    if index < group.items.count - 1 {
                        Divider().overlay(LumeTheme.hairline.opacity(0.75))
                    }
                }
            }
        }
    }

    private func reminderRow(_ item: ReminderItem) -> some View {
        HStack(alignment: .top, spacing: LumeSpace.compact) {
            // Cerchio vuoto e non toccabile: dice "aperto" senza promettere una
            // spunta che questa vista non fa.
            Image(systemName: "circle")
                .font(.subheadline)
                .foregroundStyle(item.isOverdue ? LumeTheme.light : LumeTheme.secondaryInk)
                .frame(width: 22, height: 22)
                .accessibilityHidden(true)

            VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                Text(item.title)
                    .font(.body.weight(.medium))
                    .fixedSize(horizontal: false, vertical: true)
                if !item.dueLabel.isEmpty {
                    Text(item.dueLabel)
                        .font(item.isOverdue ? .subheadline.weight(.semibold) : .subheadline)
                        .foregroundStyle(item.isOverdue ? LumeTheme.light : LumeTheme.secondaryInk)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.vertical, LumeSpace.compact)
        .accessibilityElement(children: .combine)
        .accessibilityLabel(accessibilityLabel(for: item))
    }

    private func accessibilityLabel(for item: ReminderItem) -> String {
        var parts = [item.title]
        if item.isOverdue { parts.append("in ritardo") }
        if !item.dueLabel.isEmpty { parts.append(item.dueLabel) }
        return parts.joined(separator: ", ")
    }

    /// `listIndex` è la posizione della lista in `selectedListIDs`, cioè l'ordine
    /// delle schede sul device: raggruppo e ordino con quello, non per nome.
    private var groups: [ReminderGroup] {
        let names = chosenTitles
        return Dictionary(grouping: store.items, by: \.listIndex)
            .sorted { $0.key < $1.key }
            .map { index, items in
                ReminderGroup(
                    id: index,
                    title: index < names.count ? names[index] : "Lista \(index + 1)",
                    items: items
                )
            }
    }

    private var chosenTitles: [String] {
        store.selectedListIDs.map { id in
            store.lists.first { $0.id == id }?.title ?? "Lista rimossa"
        }
    }

    private var chosenListNames: String {
        let titles = chosenTitles
        guard let last = titles.last else { return "Le liste scelte" }
        guard titles.count > 1 else { return last }
        return titles.dropLast().joined(separator: ", ") + " e " + last
    }

    private func refresh() {
        guard !isWorking else { return }
        isWorking = true
        Task {
            await store.refresh()
            isWorking = false
            onSync()
        }
    }
}

private struct ReminderGroup: Identifiable {
    let id: Int
    let title: String
    let items: [ReminderItem]
}

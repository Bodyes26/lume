import SwiftUI

/// Scelta e ordine delle liste che diventano schede sull'X3. È un foglio aperto
/// direttamente dalla `RemindersView` (un tap): il device tiene poche schede, e
/// l'ordine qui è l'ordine sul vetro perché `listIndex` della snapshot è la
/// posizione in `selectedListIDs`.
struct ReminderListSelectionView: View {
    @ObservedObject var store: RemindersStore
    let onCommit: () -> Void

    @Environment(\.dismiss) private var dismiss
    @State private var limitNotice: String?

    var body: some View {
        NavigationStack {
            List {
                chosenSection
                availableSection
            }
            .listStyle(.insetGrouped)
            .scrollContentBackground(.hidden)
            .background(LumeTheme.paper)
            .navigationTitle("Liste sull’X3")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    // Riordino e rimozione con l'affordance di sistema; le righe
                    // mostrano comunque il numero di scheda, così l'ordine è
                    // leggibile anche senza entrare in modifica.
                    EditButton()
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Fine") {
                        onCommit()
                        dismiss()
                    }
                    .fontWeight(.semibold)
                }
            }
        }
        .tint(LumeTheme.light)
        .foregroundStyle(LumeTheme.ink)
    }

    private var chosenSection: some View {
        Section {
            if chosen.isEmpty {
                Text("Nessuna lista scelta: l’X3 non mostra nessuna scheda.")
                    .font(.subheadline)
                    .foregroundStyle(LumeTheme.secondaryInk)
            } else {
                ForEach(Array(chosen.enumerated()), id: \.element.id) { index, list in
                    HStack(spacing: LumeSpace.compact) {
                        Text("\(index + 1)ª")
                            .font(.caption.weight(.semibold).monospacedDigit())
                            .foregroundStyle(LumeTheme.secondaryInk)
                            .frame(width: 24, alignment: .leading)
                        Text(list.title)
                            .font(.body.weight(.medium))
                    }
                    .accessibilityElement(children: .combine)
                    .accessibilityLabel("\(list.title), scheda numero \(index + 1)")
                }
                .onMove(perform: move)
                .onDelete(perform: remove)
            }
        } header: {
            Text("Schede sull’X3")
        } footer: {
            Text("L’ordine di questo elenco è l’ordine delle schede: la prima riga è la prima scheda. Tocca Modifica per trascinare, scorri una riga per toglierla.")
        }
    }

    private var availableSection: some View {
        Section {
            if available.isEmpty {
                Text(store.lists.isEmpty
                     ? "Non c’è nessuna lista in Promemoria: creane una nell’app Promemoria, poi torna qui."
                     : "Hai già scelto tutte le liste di Promemoria.")
                    .font(.subheadline)
                    .foregroundStyle(LumeTheme.secondaryInk)
            } else {
                ForEach(available) { list in
                    Button {
                        add(list)
                    } label: {
                        HStack {
                            Text(list.title)
                                .foregroundStyle(LumeTheme.ink)
                            Spacer(minLength: LumeSpace.compact)
                            Image(systemName: "plus.circle")
                                .foregroundStyle(LumeTheme.light)
                        }
                    }
                    .accessibilityLabel("Aggiungi \(list.title) alle schede dell’X3")
                }
            }
        } header: {
            Text("Altre liste di Promemoria")
        } footer: {
            // Il limite si racconta al tocco, non disabilitando la riga: così è
            // chiaro perché la lista non è entrata.
            if let limitNotice {
                Text(limitNotice)
                    .foregroundStyle(LumeTheme.light)
            } else {
                Text("Puoi portare sull’X3 al massimo \(LumeProtocol.maximumReminderLists) liste alla volta.")
            }
        }
    }

    private var chosen: [ReminderList] {
        store.selectedListIDs.compactMap { id in store.lists.first { $0.id == id } }
    }

    private var available: [ReminderList] {
        store.lists.filter { !store.selectedListIDs.contains($0.id) }
    }

    private func add(_ list: ReminderList) {
        guard store.selectedListIDs.count < LumeProtocol.maximumReminderLists else {
            limitNotice = "\(list.title) non entra: l’X3 tiene \(LumeProtocol.maximumReminderLists) schede. Togli una lista qui sopra e riprova."
            return
        }
        limitNotice = nil
        store.selectedListIDs.append(list.id)
        reload()
    }

    private func remove(at offsets: IndexSet) {
        store.selectedListIDs.remove(atOffsets: offsets)
        limitNotice = nil
        reload()
    }

    private func move(from source: IndexSet, to destination: Int) {
        store.selectedListIDs.move(fromOffsets: source, toOffset: destination)
        reload()
    }

    /// `selectedListIDs` persiste ma non rinfresca da sé: senza questo, `items`
    /// resterebbe con i `listIndex` della selezione precedente.
    private func reload() {
        Task { await store.refresh() }
    }
}

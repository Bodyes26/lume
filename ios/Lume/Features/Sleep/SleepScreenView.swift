import SwiftUI

struct SleepScreenView: View {
    @ObservedObject var store: SleepStore
    @ObservedObject var today: TodayStore
    @ObservedObject var reminders: RemindersStore
    var isDeviceReady: Bool
    var onSync: (SleepConfig) -> Void

    @State private var showingQuotePicker: Bool = false
    @State private var showSyncFeedback: Bool = false

    var body: some View {
        VStack(alignment: .leading, spacing: LumeSpace.medium) {
            sectionHeader

            // Live 1:1 E-Ink Preview
            SleepPreviewView(
                config: store.config,
                todayEntries: today.entries,
                reminderItems: reminders.items
            )
            .frame(maxWidth: .infinity)

            // Mode Selector Cards
            modeSelectionSection

            // Customization: Title & Quote
            customTextSection

            // Modular Toggles
            togglesSection

            // Sync Button
            syncButton
        }
        .sheet(isPresented: $showingQuotePicker) {
            quotePickerSheet
        }
    }

    // MARK: - Header
    private var sectionHeader: some View {
        VStack(alignment: .leading, spacing: LumeSpace.small) {
            HStack {
                Label("Schermata Standby", systemImage: "moon.stars")
                    .font(.system(size: 15, weight: .bold, design: .serif))
                    .foregroundStyle(LumeTheme.ink)

                Spacer()

                if let lastSync = store.lastSyncedAt {
                    Text("Sincronizzato \(lastSync, style: .time)")
                        .font(.caption2)
                        .foregroundStyle(LumeTheme.secondaryInk)
                }
            }

            Text("Scegli e personalizza cosa mostra il pannello e-ink a riposo. Il display trattiene l’immagine a consumo zero.")
                .font(.footnote)
                .foregroundStyle(LumeTheme.secondaryInk)
        }
    }

    // MARK: - Mode Selection
    private var modeSelectionSection: some View {
        VStack(alignment: .leading, spacing: LumeSpace.compact) {
            Text("MODALITÀ POSTER")
                .font(.caption2.weight(.bold))
                .foregroundStyle(LumeTheme.secondaryInk)
                .tracking(1.0)

            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: LumeSpace.compact) {
                ForEach(SleepMode.allCases) { mode in
                    let isSelected = store.config.mode == mode
                    Button {
                        withAnimation(.spring(duration: 0.25)) {
                            store.config.mode = mode
                        }
                    } label: {
                        VStack(alignment: .leading, spacing: 6) {
                            HStack {
                                Image(systemName: mode.iconName)
                                    .font(.system(size: 14, weight: .semibold))
                                    .foregroundStyle(isSelected ? LumeTheme.light : LumeTheme.ink)
                                Spacer()
                                if isSelected {
                                    Image(systemName: "checkmark.circle.fill")
                                        .font(.system(size: 13))
                                        .foregroundStyle(LumeTheme.light)
                                }
                            }

                            Text(mode.title)
                                .font(.system(size: 13, weight: .semibold))
                                .foregroundStyle(LumeTheme.ink)

                            Text(mode.subtitle)
                                .font(.system(size: 10))
                                .foregroundStyle(LumeTheme.secondaryInk)
                                .lineLimit(2)
                        }
                        .padding(LumeSpace.compact)
                        .frame(maxWidth: .infinity, minHeight: 90, alignment: .topLeading)
                        .background(isSelected ? LumeTheme.paper : Color.white.opacity(0.4))
                        .overlay(
                            RoundedRectangle(cornerRadius: 12, style: .continuous)
                                .stroke(isSelected ? LumeTheme.light : LumeTheme.hairline, lineWidth: isSelected ? 2 : 1)
                        )
                        .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }

    // MARK: - Custom Text
    private var customTextSection: some View {
        VStack(alignment: .leading, spacing: LumeSpace.compact) {
            Text("PERSONALIZZAZIONE TESTO")
                .font(.caption2.weight(.bold))
                .foregroundStyle(LumeTheme.secondaryInk)
                .tracking(1.0)

            VStack(alignment: .leading, spacing: LumeSpace.compact) {
                // Header Title
                VStack(alignment: .leading, spacing: 4) {
                    Text("Titolo intestazione")
                        .font(.caption)
                        .foregroundStyle(LumeTheme.secondaryInk)
                    TextField("Es. Scrivania di Maurizio, Lume X3", text: $store.config.customTitle)
                        .font(.subheadline)
                        .padding(10)
                        .background(LumeTheme.paper)
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                        .overlay(
                            RoundedRectangle(cornerRadius: 8)
                                .stroke(LumeTheme.hairline, lineWidth: 1)
                        )
                }

                // Quote / Mantra
                VStack(alignment: .leading, spacing: 4) {
                    HStack {
                        Text("Citazione o nota personale")
                            .font(.caption)
                            .foregroundStyle(LumeTheme.secondaryInk)
                        Spacer()
                        Button {
                            showingQuotePicker = true
                        } label: {
                            HStack(spacing: 3) {
                                Image(systemName: "sparkles")
                                Text("Scegli celebre")
                            }
                            .font(.caption.weight(.medium))
                            .foregroundStyle(LumeTheme.light)
                        }
                    }

                    TextField("Testo citazione o promemoria", text: $store.config.customQuote, axis: .vertical)
                        .lineLimit(2...4)
                        .font(.subheadline)
                        .padding(10)
                        .background(LumeTheme.paper)
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                        .overlay(
                            RoundedRectangle(cornerRadius: 8)
                                .stroke(LumeTheme.hairline, lineWidth: 1)
                        )

                    TextField("Autore o fonte", text: $store.config.customAuthor)
                        .font(.subheadline)
                        .padding(10)
                        .background(LumeTheme.paper)
                        .clipShape(RoundedRectangle(cornerRadius: 8))
                        .overlay(
                            RoundedRectangle(cornerRadius: 8)
                                .stroke(LumeTheme.hairline, lineWidth: 1)
                        )
                }
            }
            .padding(LumeSpace.compact)
            .background(Color.white.opacity(0.4))
            .clipShape(RoundedRectangle(cornerRadius: 12))
            .overlay(
                RoundedRectangle(cornerRadius: 12)
                    .stroke(LumeTheme.hairline, lineWidth: 1)
            )
        }
    }

    // MARK: - Modular Toggles
    private var togglesSection: some View {
        VStack(alignment: .leading, spacing: LumeSpace.compact) {
            Text("ELEMENTI MODULARI")
                .font(.caption2.weight(.bold))
                .foregroundStyle(LumeTheme.secondaryInk)
                .tracking(1.0)

            VStack(spacing: 0) {
                Toggle(isOn: $store.config.showBattery) {
                    Label("Percentuale e icona batteria", systemImage: "battery.75")
                        .font(.subheadline)
                }
                .padding(.vertical, 8)

                Divider().overlay(LumeTheme.hairline)

                Toggle(isOn: $store.config.showTemperature) {
                    Label("Temperatura sensore hardware DS3231 (°C)", systemImage: "thermometer.medium")
                        .font(.subheadline)
                }
                .padding(.vertical, 8)

                Divider().overlay(LumeTheme.hairline)

                Toggle(isOn: $store.config.showNextEvent) {
                    Label("Prossimo impegno del calendario", systemImage: "calendar")
                        .font(.subheadline)
                }
                .padding(.vertical, 8)

                Divider().overlay(LumeTheme.hairline)

                Toggle(isOn: $store.config.showReadingStats) {
                    Label("Statistiche di lettura (pagine e serie)", systemImage: "book")
                        .font(.subheadline)
                }
                .padding(.vertical, 8)

                Divider().overlay(LumeTheme.hairline)

                Toggle(isOn: $store.config.showSleepTime) {
                    Label("Timbro orario di sonno (dorme dalle...)", systemImage: "clock")
                        .font(.subheadline)
                }
                .padding(.vertical, 8)
            }
            .padding(.horizontal, LumeSpace.compact)
            .background(Color.white.opacity(0.4))
            .clipShape(RoundedRectangle(cornerRadius: 12))
            .overlay(
                RoundedRectangle(cornerRadius: 12)
                    .stroke(LumeTheme.hairline, lineWidth: 1)
            )
        }
    }

    // MARK: - Sync Button
    private var syncButton: some View {
        Button {
            onSync(store.config)
            withAnimation {
                showSyncFeedback = true
            }
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.5) {
                withAnimation {
                    showSyncFeedback = false
                }
            }
        } label: {
            HStack(spacing: 8) {
                Image(systemName: showSyncFeedback ? "checkmark" : "arrow.triangle.2.circlepath")
                    .font(.system(size: 14, weight: .bold))
                Text(showSyncFeedback ? "Configurazione Inviata a Lume X3" : "Sincronizza Schermata Standby")
                    .font(.system(size: 14, weight: .bold))
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
            .background(isDeviceReady ? LumeTheme.light : Color.gray.opacity(0.3))
            .foregroundStyle(isDeviceReady ? Color.white : LumeTheme.secondaryInk)
            .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        }
        .disabled(!isDeviceReady)
        .sensoryFeedback(.success, trigger: showSyncFeedback)
    }

    // MARK: - Quote Picker Sheet
    private var quotePickerSheet: some View {
        NavigationStack {
            List {
                Section {
                    ForEach(SleepPresetQuotes.presets) { preset in
                        Button {
                            store.applyPreset(preset)
                            showingQuotePicker = false
                        } label: {
                            VStack(alignment: .leading, spacing: 4) {
                                Text("«\(preset.text)»")
                                    .font(.subheadline.weight(.medium))
                                    .foregroundStyle(LumeTheme.ink)

                                HStack {
                                    Text(preset.author)
                                        .font(.caption)
                                        .foregroundStyle(LumeTheme.light)
                                    Spacer()
                                    Text(preset.category)
                                        .font(.caption2)
                                        .foregroundStyle(LumeTheme.secondaryInk)
                                }
                            }
                            .padding(.vertical, 4)
                        }
                        .buttonStyle(.plain)
                    }
                } header: {
                    Text("Citazioni e Pensieri Curati")
                }
            }
            .navigationTitle("Scegli Citazione")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Chiudi") {
                        showingQuotePicker = false
                    }
                }
            }
        }
    }
}

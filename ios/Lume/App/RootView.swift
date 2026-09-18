import SwiftUI

struct RootView: View {
    @ObservedObject var bluetooth: LumeBluetoothManager
    @ObservedObject var reminders: RemindersStore
    @ObservedObject var sleep: SleepStore
    @ObservedObject var today: TodayStore
    @ObservedObject var trail: TrailStore
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        ZStack {
            LumeTheme.paper.ignoresSafeArea()

            ScrollView {
                VStack(alignment: .leading, spacing: LumeSpace.large) {
                    brandHeader
                    ConnectionStatusView(bluetooth: bluetooth)
                    Divider().overlay(LumeTheme.hairline)
                    TodayView(
                        store: today,
                        isDeviceReady: bluetooth.phase.isReady,
                        onSync: bluetooth.syncToday
                    )
                    Divider().overlay(LumeTheme.hairline)
                    RemindersView(
                        store: reminders,
                        isDeviceReady: bluetooth.phase.isReady,
                        onSync: bluetooth.syncReminders
                    )
                    Divider().overlay(LumeTheme.hairline)
                    SleepScreenView(
                        store: sleep,
                        today: today,
                        reminders: reminders,
                        isDeviceReady: bluetooth.phase.isReady,
                        onSync: { config in
                            bluetooth.syncSleepConfig(config)
                            sleep.markSyncCompleted()
                        }
                    )
                    Divider().overlay(LumeTheme.hairline)
                    TrailView(
                        store: trail,
                        isDeviceReady: bluetooth.phase.isReady,
                        onSyncCatalog: bluetooth.syncTrailCatalog
                    )
                }
                .padding(.horizontal, LumeSpace.medium)
                .padding(.top, LumeSpace.compact)
                .padding(.bottom, 56)
            }
            .scrollDismissesKeyboard(.interactively)
        }
        .tint(LumeTheme.light)
        .foregroundStyle(LumeTheme.ink)
        .onAppear {
            bluetooth.start()
            Task {
                await today.refreshIfAuthorized()
                await reminders.refreshIfAuthorized()
            }
        }
        .onChange(of: scenePhase) { _, phase in
            guard phase == .active else { return }
            bluetooth.start()
            Task { await today.refreshIfAuthorized() }
        }
        .sensoryFeedback(.success, trigger: bluetooth.phase.isReady)
    }

    private var brandHeader: some View {
        HStack(spacing: LumeSpace.compact) {
            LumeMark()
                .frame(width: 44, height: 44)

            VStack(alignment: .leading, spacing: 0) {
                Text("Lume")
                    .font(.system(size: 38, weight: .medium, design: .serif))
                    .tracking(-1.1)
                Text("Il telefono prepara. L’X3 resta carta.")
                    .font(.subheadline)
                    .foregroundStyle(LumeTheme.secondaryInk)
            }

            Spacer(minLength: 0)
        }
        .accessibilityElement(children: .combine)
    }
}

private struct ConnectionStatusView: View {
    @ObservedObject var bluetooth: LumeBluetoothManager

    private var copy: (title: String, detail: String, color: Color) {
        switch bluetooth.phase {
        case .unavailable(let reason):
            return ("Bluetooth non disponibile", reason, LumeTheme.secondaryInk)
        case .idle:
            return ("X3 non collegato", "Cerco Lume X3 quando Bluetooth è pronto.", LumeTheme.secondaryInk)
        case .searching:
            return ("Cerco Lume X3", "Tieni il dispositivo acceso e vicino all’iPhone.", LumeTheme.light)
        case .found(let name):
            return (name, "Dispositivo trovato. È pronto per il collegamento.", LumeTheme.light)
        case .connecting(let name):
            return ("Collego \(name)", "Il primo collegamento può mostrare la richiesta di abbinamento.", LumeTheme.light)
        case .preparing(let name):
            return (name, "Preparo i canali di sincronizzazione…", LumeTheme.light)
        case .ready(let name):
            return (name, "Collegato e pronto a sincronizzare.", LumeTheme.positive)
        case .disconnected(let reason):
            return ("X3 disconnesso", reason, LumeTheme.secondaryInk)
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: LumeSpace.regular) {
            HStack(alignment: .firstTextBaseline, spacing: LumeSpace.compact) {
                Circle()
                    .fill(copy.color)
                    .frame(width: 9, height: 9)
                    .accessibilityHidden(true)

                VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                    Text(copy.title)
                        .font(.headline)
                    Text(copy.detail)
                        .font(.subheadline)
                        .foregroundStyle(LumeTheme.secondaryInk)
                        .fixedSize(horizontal: false, vertical: true)
                }

                Spacer(minLength: LumeSpace.small)

                trailingControl
            }

            if let error = bluetooth.lastError {
                Text(error)
                    .font(.footnote)
                    .foregroundStyle(LumeTheme.secondaryInk)
                    .accessibilityLabel("Errore: \(error)")
            } else if let synced = bluetooth.lastSyncDate, bluetooth.phase.isReady {
                Text("Ultimo invio \(synced, style: .relative)")
                    .font(.footnote)
                    .foregroundStyle(LumeTheme.secondaryInk)
            }
        }
        .padding(.vertical, LumeSpace.small)
    }

    @ViewBuilder
    private var trailingControl: some View {
        switch bluetooth.phase {
        case .searching, .connecting, .preparing:
            ProgressView()
                .controlSize(.small)
                .accessibilityLabel("Connessione in corso")
        case .found:
            Button("Collega") { bluetooth.connect() }
                .buttonStyle(.borderedProminent)
                .buttonBorderShape(.capsule)
        case .disconnected, .idle:
            Button("Riprova") { bluetooth.retry() }
                .buttonStyle(.bordered)
                .buttonBorderShape(.capsule)
        default:
            EmptyView()
        }
    }
}

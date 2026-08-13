import SwiftUI
import UIKit

struct TodayView: View {
    @ObservedObject var store: TodayStore
    let isDeviceReady: Bool
    let onSync: () -> Void

    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.openURL) private var openURL

    var body: some View {
        VStack(alignment: .leading, spacing: LumeSpace.medium) {
            sectionHeader

            switch store.accessState {
            case .notDetermined:
                permissionPrompt
            case .requesting:
                loadingAccess
            case .denied, .restricted:
                deniedAccess
            case .available, .partial:
                agenda
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
    }

    private var sectionHeader: some View {
        HStack(alignment: .firstTextBaseline, spacing: LumeSpace.regular) {
            VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                Text("Oggi")
                    .font(.system(size: 30, weight: .medium, design: .serif))
                    .tracking(-0.6)
                Text(headerDetail)
                    .font(.subheadline)
                    .foregroundStyle(LumeTheme.secondaryInk)
            }

            Spacer(minLength: 0)

            if store.canSync {
                Button(action: onSync) {
                    if store.isRefreshing {
                        ProgressView()
                            .controlSize(.small)
                    } else {
                        Label("Aggiorna", systemImage: "arrow.clockwise")
                    }
                }
                .buttonStyle(.bordered)
                .buttonBorderShape(.capsule)
                .disabled(store.isRefreshing)
                .accessibilityLabel(store.isRefreshing ? "Aggiornamento agenda in corso" : "Aggiorna agenda")
            }
        }
    }

    private var headerDetail: String {
        switch store.accessState {
        case .available:
            return "Calendario e promemoria · prossime 24 ore"
        case .partial:
            return "Accesso parziale · prossime 24 ore"
        default:
            return "La giornata essenziale, sul vetro"
        }
    }

    private var permissionPrompt: some View {
        VStack(alignment: .leading, spacing: LumeSpace.regular) {
            Text("Scegli cosa merita spazio.")
                .font(.system(.title3, design: .serif, weight: .medium))
            Text("Lume legge gli appuntamenti delle prossime 24 ore e i promemoria ancora aperti. I dati restano tra questo iPhone e il tuo X3.")
                .font(.body)
                .foregroundStyle(LumeTheme.secondaryInk)
                .fixedSize(horizontal: false, vertical: true)
            Button("Consenti accesso") {
                Task {
                    await store.requestAccessAndRefresh()
                    if store.canSync { onSync() }
                }
            }
            .buttonStyle(.borderedProminent)
            .buttonBorderShape(.capsule)
        }
        .padding(.vertical, LumeSpace.small)
    }

    private var loadingAccess: some View {
        HStack(spacing: LumeSpace.compact) {
            ProgressView()
                .controlSize(.small)
            Text("Preparo Calendario e Promemoria…")
                .foregroundStyle(LumeTheme.secondaryInk)
        }
        .font(.body)
        .padding(.vertical, LumeSpace.regular)
    }

    private var deniedAccess: some View {
        VStack(alignment: .leading, spacing: LumeSpace.regular) {
            Text("Oggi resta privato finché vuoi.")
                .font(.system(.title3, design: .serif, weight: .medium))
            Text(store.accessState == .restricted
                 ? "Questo iPhone limita l’accesso a Calendario e Promemoria."
                 : "Per mostrare l’agenda sull’X3, abilita Calendario e Promemoria nelle Impostazioni di Lume.")
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
    private var agenda: some View {
        if store.entries.isEmpty {
            VStack(alignment: .leading, spacing: LumeSpace.small) {
                Text("Le prossime ore sono libere.")
                    .font(.system(.title3, design: .serif, weight: .medium))
                Text("Quando compare un appuntamento o un promemoria, Lume lo porterà qui e sull’X3.")
                    .font(.body)
                    .foregroundStyle(LumeTheme.secondaryInk)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.vertical, LumeSpace.small)
        } else {
            VStack(spacing: 0) {
                ForEach(Array(store.entries.enumerated()), id: \.element.id) { index, entry in
                    entryRow(entry)
                    if index < store.entries.count - 1 {
                        Divider().overlay(LumeTheme.hairline.opacity(0.75))
                    }
                }
            }
        }

        HStack(alignment: .top, spacing: LumeSpace.small) {
            Image(systemName: isDeviceReady ? "calendar.badge.checkmark" : "iphone.and.arrow.forward")
                .foregroundStyle(isDeviceReady ? LumeTheme.positive : LumeTheme.secondaryInk)
            Text(isDeviceReady
                 ? "Apri Today sull’X3 oppure premi Aggiorna per inviare questa agenda."
                 : "L’agenda resta pronta sull’iPhone finché Lume X3 non è collegato.")
                .fixedSize(horizontal: false, vertical: true)
        }
        .font(.footnote)
        .foregroundStyle(LumeTheme.secondaryInk)
        .accessibilityElement(children: .combine)
    }

    private func entryRow(_ entry: TodayEntry) -> some View {
        HStack(alignment: .top, spacing: LumeSpace.compact) {
            Image(systemName: entry.kind == .event ? "calendar" : "bell")
                .font(.subheadline)
                .foregroundStyle(entry.kind == .event ? LumeTheme.light : LumeTheme.secondaryInk)
                .frame(width: 22, height: 22)
                .accessibilityHidden(true)

            VStack(alignment: .leading, spacing: LumeSpace.xSmall) {
                HStack(alignment: .firstTextBaseline, spacing: LumeSpace.small) {
                    Text(localizedBucket(entry.subtitle))
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(LumeTheme.secondaryInk)
                    Text(entry.time)
                        .font(.subheadline)
                        .foregroundStyle(LumeTheme.secondaryInk)
                }
                Text(entry.title)
                    .font(.body.weight(.medium))
                    .fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.vertical, LumeSpace.compact)
        .accessibilityElement(children: .combine)
    }

    private func localizedBucket(_ bucket: String) -> String {
        switch bucket {
        case "TODAY": return "OGGI"
        case "TONIGHT": return "STASERA"
        case "TOMORROW": return "DOMANI"
        case "": return "PROMEMORIA"
        default: return bucket
        }
    }
}

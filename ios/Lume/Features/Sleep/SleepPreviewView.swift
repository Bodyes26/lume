import SwiftUI

struct SleepPreviewView: View {
    let config: SleepConfig
    var todayEntries: [TodayEntry] = []
    var reminderItems: [ReminderItem] = []

    private let screenAspect: CGFloat = 528.0 / 792.0

    var body: some View {
        VStack(spacing: LumeSpace.compact) {
            HStack {
                Text("ANTEPRIMA SU SCHERMO E-INK (528×792)")
                    .font(.system(size: 11, weight: .bold, design: .monospaced))
                    .foregroundStyle(LumeTheme.secondaryInk)
                    .tracking(1.2)
                Spacer()
                Text(config.mode.title.uppercased())
                    .font(.system(size: 11, weight: .semibold))
                    .padding(.horizontal, 6)
                    .padding(.vertical, 2)
                    .background(LumeTheme.ink.opacity(0.08))
                    .clipShape(Capsule())
            }

            // E-Ink Device Canvas
            ZStack {
                // E-Ink Paper Canvas
                RoundedRectangle(cornerRadius: 12, style: .continuous)
                    .fill(Color(red: 0.95, green: 0.94, blue: 0.91))
                    .overlay(
                        RoundedRectangle(cornerRadius: 12, style: .continuous)
                            .stroke(Color(red: 0.2, green: 0.2, blue: 0.2), lineWidth: 2)
                    )
                    .shadow(color: Color.black.opacity(0.06), radius: 8, x: 0, y: 4)

                // Render Active Face
                VStack(spacing: 0) {
                    headerView
                    Divider().background(Color.black.opacity(0.6))
                        .padding(.horizontal, 12)
                        .padding(.bottom, 6)

                    faceContentView
                        .frame(maxHeight: .infinity, alignment: .top)

                    footerView
                }
                .foregroundStyle(Color(red: 0.1, green: 0.09, blue: 0.08))
                .padding(.vertical, 10)
                .padding(.horizontal, 10)
            }
            .aspectRatio(screenAspect, contentMode: .fit)
            .frame(maxWidth: 320)
        }
        .padding(LumeSpace.compact)
        .background(LumeTheme.paper)
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
    }

    // MARK: - Header
    private var headerView: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(config.customTitle.isEmpty ? "LUME" : config.customTitle.uppercased())
                .font(.system(size: 9, weight: .bold, design: .serif))
                .tracking(1.0)

            Spacer()

            HStack(spacing: 6) {
                if config.showTemperature {
                    Text("23,8 °C")
                        .font(.system(size: 8, weight: .medium, design: .monospaced))
                }
                if config.showBattery {
                    HStack(spacing: 2) {
                        Image(systemName: "battery.75")
                            .font(.system(size: 8))
                        Text("84%")
                            .font(.system(size: 8, weight: .semibold, design: .monospaced))
                    }
                }
                if config.showSleepTime {
                    Text("dorme 14:30")
                        .font(.system(size: 8, weight: .regular, design: .serif))
                }
            }
        }
        .padding(.horizontal, 12)
        .padding(.bottom, 4)
    }

    // MARK: - Face Content
    @ViewBuilder
    private var faceContentView: some View {
        switch config.mode {
        case .auto, .dashboard:
            dashboardFace
        case .reader:
            readerFace
        case .reminders:
            remindersFace
        case .quote:
            quoteFace
        case .minimal:
            minimalFace
        }
    }

    // MARK: - Dashboard Face
    private var dashboardFace: some View {
        VStack(spacing: 6) {
            // Date Banner
            VStack(spacing: 2) {
                Text(formattedDate.uppercased())
                    .font(.system(size: 13, weight: .bold, design: .serif))
                    .tracking(0.5)
                Rectangle()
                    .fill(Color.black)
                    .frame(width: 36, height: 1.5)
            }
            .padding(.top, 2)

            // Next Event Box
            if config.showNextEvent {
                HStack(spacing: 6) {
                    Text(todayEntries.first?.time.isEmpty == false ? todayEntries.first!.time : "16:30")
                        .font(.system(size: 8, weight: .bold, design: .monospaced))
                        .padding(.horizontal, 5)
                        .padding(.vertical, 2)
                        .background(Color.black)
                        .foregroundStyle(Color.white)
                        .clipShape(RoundedRectangle(cornerRadius: 3))

                    VStack(alignment: .leading, spacing: 1) {
                        Text(todayEntries.first?.title.isEmpty == false ? todayEntries.first!.title : "Riunione di Progetto")
                            .font(.system(size: 9, weight: .bold))
                            .lineLimit(1)
                        Text(todayEntries.first?.subtitle.isEmpty == false ? todayEntries.first!.subtitle : "Ufficio · Calendario Lavoro")
                            .font(.system(size: 7, weight: .regular))
                            .foregroundStyle(Color.black.opacity(0.7))
                            .lineLimit(1)
                    }
                    Spacer()
                }
                .padding(6)
                .background(
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(Color.black.opacity(0.8), lineWidth: 1)
                )
                .padding(.horizontal, 8)
            }

            // Reminders Section
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("PROMEMORIA PRIORITARI")
                        .font(.system(size: 8, weight: .bold, design: .monospaced))
                        .tracking(0.8)
                    Spacer()
                    Text("\(max(reminderItems.count, 3)) DA FARE")
                        .font(.system(size: 7, weight: .semibold))
                }

                let itemsToShow = reminderItems.isEmpty ? [
                    "Rivedere bozza documento",
                    "Acquistare caffè in grani",
                    "Chiamata di allineamento"
                ] : reminderItems.prefix(4).map(\.title)

                ForEach(Array(itemsToShow.enumerated()), id: \.offset) { _, title in
                    HStack(spacing: 6) {
                        RoundedRectangle(cornerRadius: 2)
                            .stroke(Color.black, lineWidth: 1.2)
                            .frame(width: 9, height: 9)
                        Text(title)
                            .font(.system(size: 8, weight: .regular))
                            .lineLimit(1)
                        Spacer()
                    }
                }
            }
            .padding(6)
            .padding(.horizontal, 6)

            Spacer(minLength: 0)

            // Ambient Band
            if config.showReadingStats {
                HStack(spacing: 4) {
                    Text("📖 38 pag oggi · 5 gg di fila")
                        .font(.system(size: 7.5, weight: .medium))
                        .foregroundStyle(Color.black.opacity(0.8))
                }
                .padding(.bottom, 2)
            }
        }
    }

    // MARK: - Reader Face
    private var readerFace: some View {
        VStack(spacing: 6) {
            Text("✦ IN LETTURA ✦")
                .font(.system(size: 8, weight: .bold, design: .monospaced))
                .tracking(1.2)
                .padding(.top, 2)

            Text("I Promessi Sposi")
                .font(.system(size: 13, weight: .bold, design: .serif))
                .lineLimit(1)

            Text("Alessandro Manzoni")
                .font(.system(size: 8.5, weight: .regular, design: .serif))
                .italic()

            // Progress Bar
            VStack(spacing: 2) {
                GeometryReader { geo in
                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 3)
                            .stroke(Color.black, lineWidth: 1)
                        RoundedRectangle(cornerRadius: 3)
                            .fill(Color.black)
                            .frame(width: geo.size.width * 0.68)
                    }
                }
                .frame(height: 7)
                .padding(.horizontal, 24)

                Text("68% completato · Cap. XVIII")
                    .font(.system(size: 7.5, weight: .semibold, design: .monospaced))
            }
            .padding(.vertical, 2)

            // Spark Dots
            HStack(spacing: 12) {
                ForEach(["L", "M", "M", "G", "V", "S", "D"], id: \.self) { day in
                    VStack(spacing: 2) {
                        Text(day)
                            .font(.system(size: 6.5, weight: .bold))
                        Circle()
                            .fill(day == "D" ? Color.clear : Color.black)
                            .overlay(Circle().stroke(Color.black, lineWidth: 0.8))
                            .frame(width: 5, height: 5)
                    }
                }
            }
            .padding(.vertical, 2)

            // Quote Box
            VStack(alignment: .leading, spacing: 3) {
                HStack {
                    Text("«")
                        .font(.system(size: 12, weight: .bold, design: .serif))
                    Spacer()
                }
                Text(config.customQuote.isEmpty ? "Un lettore vive mille vite prima di morire. Chi non legge mai ne vive una sola." : config.customQuote)
                    .font(.system(size: 8, weight: .regular, design: .serif))
                    .lineLimit(4)
                    .padding(.horizontal, 4)
                HStack {
                    Spacer()
                    Text("— \(config.customAuthor.isEmpty ? "George R.R. Martin" : config.customAuthor)")
                        .font(.system(size: 7, weight: .medium, design: .serif))
                }
            }
            .padding(6)
            .background(
                RoundedRectangle(cornerRadius: 6)
                    .stroke(Color.black.opacity(0.8), lineWidth: 0.8)
            )
            .padding(.horizontal, 8)

            Spacer(minLength: 0)
        }
    }

    // MARK: - Reminders Face
    private var remindersFace: some View {
        VStack(spacing: 6) {
            Text("PROMEMORIA & FOCUS")
                .font(.system(size: 9, weight: .bold, design: .monospaced))
                .tracking(1.0)
                .padding(.top, 2)

            VStack(alignment: .leading, spacing: 6) {
                let items = reminderItems.isEmpty ? [
                    "Completare revisione documento",
                    "Pianificare sessione di lettura",
                    "Acquistare filtro luce calda",
                    "Aggiornare firmware Lume X3",
                    "Preparare scaletta settimanale"
                ] : reminderItems.prefix(6).map(\.title)

                ForEach(items, id: \.self) { title in
                    HStack(spacing: 6) {
                        RoundedRectangle(cornerRadius: 2)
                            .stroke(Color.black, lineWidth: 1.2)
                            .frame(width: 9, height: 9)
                        Text(title)
                            .font(.system(size: 8.5, weight: .medium))
                            .lineLimit(1)
                        Spacer()
                    }
                }
            }
            .padding(6)
            .padding(.horizontal, 8)

            Spacer(minLength: 0)
        }
    }

    // MARK: - Quote Face
    private var quoteFace: some View {
        VStack(spacing: 8) {
            Text(formattedDate.uppercased())
                .font(.system(size: 9, weight: .bold, design: .serif))
                .tracking(0.8)
                .padding(.top, 4)

            Spacer(minLength: 0)

            VStack(spacing: 6) {
                Text("«")
                    .font(.system(size: 20, weight: .bold, design: .serif))
                Text(config.customQuote.isEmpty ? "La semplicità è la suprema sofisticazione." : config.customQuote)
                    .font(.system(size: 11, weight: .bold, design: .serif))
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 10)
                Text("»")
                    .font(.system(size: 20, weight: .bold, design: .serif))

                Text("— \(config.customAuthor.isEmpty ? "Leonardo da Vinci" : config.customAuthor)")
                    .font(.system(size: 8.5, weight: .medium, design: .serif))
                    .padding(.top, 4)
            }

            Spacer(minLength: 0)
        }
    }

    // MARK: - Minimal Face
    private var minimalFace: some View {
        VStack(spacing: 8) {
            Spacer(minLength: 0)

            LumeMark()
                .frame(width: 52, height: 52)
                .foregroundStyle(Color.black)

            Text("lume")
                .font(.system(size: 16, weight: .bold, design: .serif))

            Rectangle()
                .fill(Color.black)
                .frame(width: 32, height: 1.5)

            Text(formattedDate.uppercased())
                .font(.system(size: 8.5, weight: .medium, design: .monospaced))

            Spacer(minLength: 0)
        }
    }

    // MARK: - Footer
    private var footerView: some View {
        Text("premi accensione per risvegliare")
            .font(.system(size: 7, weight: .regular, design: .serif))
            .foregroundStyle(Color.black.opacity(0.7))
            .padding(.bottom, 2)
    }

    private var formattedDate: String {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "it_IT")
        formatter.dateFormat = "EEEE d MMMM"
        return formatter.string(from: .now)
    }
}

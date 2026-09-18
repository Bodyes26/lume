import Foundation

/// Rappresenta i metadati e lo stato di una storia per il sistema Trail di Lume.
struct TrailStoryInfo: Identifiable, Codable, Sendable, Equatable {
    let id: String
    let title: String
    let description: String
    let chapterCount: Int
    let locale: String
    var currentChapter: Int     // 0 = non iniziata, 1..N = capitolo corrente
    var currentDay: Int
    var isInstalled: Bool       // presente sul device
    var isCompleted: Bool
    var fileSize: Int           // dimensione in byte del file .story

    var progressText: String {
        if isCompleted {
            return "Completata ✓"
        }
        if currentChapter == 0 {
            return "Nuova partita (\(chapterCount) capitoli)"
        }
        return "Cap. \(currentChapter)/\(chapterCount) · Giorno \(currentDay)"
    }

    var progressFraction: Double {
        if isCompleted { return 1.0 }
        if chapterCount <= 0 { return 0.0 }
        return Double(currentChapter) / Double(chapterCount)
    }

    static let defaults: [TrailStoryInfo] = [
        TrailStoryInfo(
            id: "silk_road",
            title: "La Via della Seta",
            description: "Dalle lagune di Venezia fino alla corte del Gran Khan a Pechino, anno 1271.",
            chapterCount: 8,
            locale: "it",
            currentChapter: 0,
            currentDay: 1,
            isInstalled: true,
            isCompleted: false,
            fileSize: 19417
        ),
        TrailStoryInfo(
            id: "stellar",
            title: "Orizzonti Stellari",
            description: "Il viaggio della nave generazionale Esperanza verso il sistema di Kepler-442b.",
            chapterCount: 8,
            locale: "it",
            currentChapter: 0,
            currentDay: 1,
            isInstalled: false,
            isCompleted: false,
            fileSize: 15047
        ),
        TrailStoryInfo(
            id: "lighthouse",
            title: "L'Ultimo Faro",
            description: "Il guardiano dell'ultimo faro costiero in un mondo dopo il crollo della civilta.",
            chapterCount: 8,
            locale: "it",
            currentChapter: 0,
            currentDay: 1,
            isInstalled: false,
            isCompleted: false,
            fileSize: 15080
        )
    ]
}

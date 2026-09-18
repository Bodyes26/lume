import Foundation

struct PresetQuote: Identifiable, Sendable, Hashable {
    let id: String
    let text: String
    let author: String
    let category: String

    init(text: String, author: String, category: String) {
        self.id = "\(author)-\(text.prefix(20))"
        self.text = text
        self.author = author
        self.category = category
    }
}

enum SleepPresetQuotes {
    static let presets: [PresetQuote] = [
        PresetQuote(
            text: "La semplicità è la suprema sofisticazione.",
            author: "Leonardo da Vinci",
            category: "Semplicità & Design"
        ),
        PresetQuote(
            text: "Un lettore vive mille vite prima di morire. Chi non legge mai ne vive una sola.",
            author: "George R.R. Martin",
            category: "Lettura"
        ),
        PresetQuote(
            text: "Prendete la vita con leggerezza, che leggerezza non è superficialità, ma planare sulle cose dall’alto.",
            author: "Italo Calvino",
            category: "Riflessione"
        ),
        PresetQuote(
            text: "Ciò che conta è la direzione, non la velocità.",
            author: "Seneca",
            category: "Focus & Stoicismo"
        ),
        PresetQuote(
            text: "La felicità della tua vita dipende dalla qualità dei tuoi pensieri.",
            author: "Marco Aurelio",
            category: "Stoicismo"
        ),
        PresetQuote(
            text: "I libri sono specchi: riflettono solo ciò che abbiamo dentro.",
            author: "Carlos Ruiz Zafón",
            category: "Lettura"
        ),
        PresetQuote(
            text: "Fatti non foste a viver come bruti, ma per seguir virtute e canoscenza.",
            author: "Dante Alighieri",
            category: "Conoscenza"
        ),
        PresetQuote(
            text: "Il segreto per andare avanti è iniziare.",
            author: "Mark Twain",
            category: "Azione"
        ),
        PresetQuote(
            text: "L'essenziale è invisibile agli occhi.",
            author: "Antoine de Saint-Exupéry",
            category: "Riflessione"
        ),
        PresetQuote(
            text: "Chi ha un perché per vivere può sopportare quasi ogni come.",
            author: "Friedrich Nietzsche",
            category: "Determinazione"
        )
    ]
}

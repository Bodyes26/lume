import SwiftUI

@MainActor
enum LumeTheme {
    static let paper = Color(uiColor: UIColor { traits in
        traits.userInterfaceStyle == .dark
            ? UIColor(red: 0.090, green: 0.086, blue: 0.073, alpha: 1)
            : UIColor(red: 0.953, green: 0.929, blue: 0.859, alpha: 1)
    })

    static let raisedPaper = Color(uiColor: UIColor { traits in
        traits.userInterfaceStyle == .dark
            ? UIColor(red: 0.135, green: 0.129, blue: 0.108, alpha: 1)
            : UIColor(red: 0.980, green: 0.965, blue: 0.918, alpha: 1)
    })

    static let ink = Color(uiColor: UIColor { traits in
        traits.userInterfaceStyle == .dark
            ? UIColor(red: 0.925, green: 0.902, blue: 0.832, alpha: 1)
            : UIColor(red: 0.102, green: 0.098, blue: 0.078, alpha: 1)
    })

    static let secondaryInk = Color(uiColor: UIColor { traits in
        traits.userInterfaceStyle == .dark
            ? UIColor(red: 0.682, green: 0.655, blue: 0.574, alpha: 1)
            : UIColor(red: 0.350, green: 0.329, blue: 0.269, alpha: 1)
    })

    static let hairline = Color(uiColor: UIColor { traits in
        traits.userInterfaceStyle == .dark
            ? UIColor(red: 0.252, green: 0.237, blue: 0.194, alpha: 1)
            : UIColor(red: 0.775, green: 0.729, blue: 0.620, alpha: 1)
    })

    static let light = Color(red: 0.780, green: 0.443, blue: 0.098)
    static let positive = Color(red: 0.239, green: 0.486, blue: 0.318)
}

enum LumeSpace {
    static let xSmall: CGFloat = 4
    static let small: CGFloat = 8
    static let compact: CGFloat = 12
    static let regular: CGFloat = 16
    static let medium: CGFloat = 24
    static let large: CGFloat = 32
    static let xLarge: CGFloat = 48
}

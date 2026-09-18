import XCTest
@testable import Lume

final class SleepConfigTests: XCTestCase {
    func testSleepConfigCardEncodingMatchesFirmwareContract() throws {
        let config = SleepConfig(
            mode: .dashboard,
            customTitle: "Scrivania di Maurizio",
            customQuote: "La semplicità è la suprema sofisticazione.",
            customAuthor: "Leonardo da Vinci",
            showBattery: true,
            showTemperature: true,
            showNextEvent: true,
            showReadingStats: false,
            showSleepTime: true
        )

        let data = try LumeProtocol.makeSleepConfig(config: config, maximumPayloadBytes: 512)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])

        XCTAssertEqual(json["schemaVersion"] as? Int, 1)
        XCTAssertEqual(json["type"] as? String, "sleep.config")
        XCTAssertEqual(json["face"] as? Int, 1)
        XCTAssertEqual(json["title"] as? String, "Scrivania di Maurizio")
        XCTAssertEqual(json["quote"] as? String, "La semplicità è la suprema sofisticazione.")
        XCTAssertEqual(json["author"] as? String, "Leonardo da Vinci")
        XCTAssertEqual(json["battery"] as? Bool, true)
        XCTAssertEqual(json["temp"] as? Bool, true)
        XCTAssertEqual(json["event"] as? Bool, true)
        XCTAssertEqual(json["stats"] as? Bool, false)
        XCTAssertEqual(json["stamp"] as? Bool, true)
    }

    func testSleepConfigAllModesEncoding() throws {
        for mode in SleepMode.allCases {
            let config = SleepConfig(mode: mode)
            let data = try LumeProtocol.makeSleepConfig(config: config)
            let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
            XCTAssertEqual(json["face"] as? Int, mode.rawValue)
        }
    }

    func testSleepConfigTruncatesLongFields() throws {
        let longTitle = String(repeating: "A", count: 100)
        let longQuote = String(repeating: "Q", count: 300)
        let longAuthor = String(repeating: "B", count: 80)

        let config = SleepConfig(
            mode: .quote,
            customTitle: longTitle,
            customQuote: longQuote,
            customAuthor: longAuthor
        )

        let data = try LumeProtocol.makeSleepConfig(config: config, maximumPayloadBytes: 512)
        let json = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])

        let title = try XCTUnwrap(json["title"] as? String)
        let quote = try XCTUnwrap(json["quote"] as? String)
        let author = try XCTUnwrap(json["author"] as? String)

        XCTAssertLessThanOrEqual(title.utf8.count, 48)
        XCTAssertLessThanOrEqual(quote.utf8.count, 160)
        XCTAssertLessThanOrEqual(author.utf8.count, 48)
    }

    func testSleepConfigPayloadLimitGuard() {
        let config = SleepConfig()
        XCTAssertThrowsError(try LumeProtocol.makeSleepConfig(config: config, maximumPayloadBytes: 64)) { error in
            XCTAssertEqual(error as? LumeProtocolError, .payloadLimitTooSmall(64))
        }
    }
}

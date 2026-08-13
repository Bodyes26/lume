@preconcurrency import CoreBluetooth
import Combine
import Foundation

@MainActor
enum LumeConnectionPhase: Equatable {
    case unavailable(String)
    case idle
    case searching
    case found(String)
    case connecting(String)
    case preparing(String)
    case ready(String)
    case disconnected(String)

    var isReady: Bool {
        if case .ready = self { return true }
        return false
    }
}

@MainActor
final class LumeBluetoothManager: NSObject, ObservableObject {
    @Published private(set) var phase: LumeConnectionPhase = .idle
    @Published private(set) var isWriting = false
    @Published private(set) var lastSyncDate: Date?
    @Published private(set) var lastError: String?

    private let priorities: PrioritiesStore
    private let defaults: UserDefaults
    private let lastPeripheralKey = "lume.lastPeripheralIdentifier"

    private var central: CBCentralManager!
    private var nearbyPeripheral: CBPeripheral?
    private var connectedPeripheral: CBPeripheral?
    private var cardWriteCharacteristic: CBCharacteristic?
    private var actionNotifyCharacteristic: CBCharacteristic?
    private var becameReady = false

    private struct PendingWrite {
        let sequence: UInt64
        let data: Data
        let completesSync: Bool
    }

    private var pendingWrites: [PendingWrite] = []
    private var activeWrite: PendingWrite?
    private var nextWriteSequence: UInt64 = 0
    private var writeTimeoutTask: Task<Void, Never>?
    private var notificationRetryTask: Task<Void, Never>?
    private var notificationRetryCount = 0

    private let serviceUUID = CBUUID(string: LumeProtocol.serviceUUID)
    private let cardWriteUUID = CBUUID(string: LumeProtocol.cardWriteUUID)
    private let actionNotifyUUID = CBUUID(string: LumeProtocol.actionNotifyUUID)

    init(priorities: PrioritiesStore, defaults: UserDefaults = .standard) {
        self.priorities = priorities
        self.defaults = defaults
        super.init()
        central = CBCentralManager(
            delegate: self,
            queue: .main,
            options: [
                CBCentralManagerOptionShowPowerAlertKey: true,
                CBCentralManagerOptionRestoreIdentifierKey: "com.maurizio.lume.central"
            ]
        )
    }

    func start() {
        guard central.state == .poweredOn else {
            updatePhase(for: central.state)
            return
        }
        guard connectedPeripheral == nil else { return }
        restoreOrScan()
    }

    func connect() {
        guard let peripheral = nearbyPeripheral else {
            startScanning()
            return
        }
        central.stopScan()
        phase = .connecting(peripheral.name ?? "Lume X3")
        lastError = nil
        central.connect(
            peripheral,
            options: [
                CBConnectPeripheralOptionNotifyOnConnectionKey: true,
                CBConnectPeripheralOptionNotifyOnDisconnectionKey: true
            ]
        )
    }

    func retry() {
        lastError = nil
        startScanning()
    }

    private func restoreOrScan() {
        if
            let rawIdentifier = defaults.string(forKey: lastPeripheralKey),
            let identifier = UUID(uuidString: rawIdentifier),
            let peripheral = central.retrievePeripherals(withIdentifiers: [identifier]).first
        {
            nearbyPeripheral = peripheral
            connect()
        } else {
            startScanning()
        }
    }

    private func startScanning() {
        guard central.state == .poweredOn else { return }
        central.stopScan()
        nearbyPeripheral = nil
        phase = .searching
        central.scanForPeripherals(
            withServices: [serviceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
    }

    private func prepare(_ peripheral: CBPeripheral) {
        connectedPeripheral = peripheral
        nearbyPeripheral = peripheral
        peripheral.delegate = self
        notificationRetryTask?.cancel()
        notificationRetryTask = nil
        notificationRetryCount = 0
        cardWriteCharacteristic = nil
        actionNotifyCharacteristic = nil
        becameReady = false
        phase = .preparing(peripheral.name ?? "Lume X3")
        peripheral.discoverServices([serviceUUID])
    }

    private func becomeReady() {
        guard
            !becameReady,
            let peripheral = connectedPeripheral,
            cardWriteCharacteristic != nil,
            actionNotifyCharacteristic?.isNotifying == true
        else { return }

        becameReady = true
        let name = peripheral.name ?? "Lume X3"
        phase = .ready(name)
        actionNotifyCharacteristic.map { peripheral.readValue(for: $0) }
        sendTimeSync()
    }

    private func sendTimeSync() {
        do {
            enqueue([try LumeProtocol.makeTimeSync()], marksLastAsSynced: true)
        } catch {
            lastError = error.localizedDescription
        }
    }

    private func sendPriorities() {
        guard let peripheral = connectedPeripheral else { return }
        do {
            let maximum = peripheral.maximumWriteValueLength(for: .withResponse)
            let payloads = try LumeProtocol.makePrioritySnapshots(
                items: priorities.items,
                maximumPayloadBytes: maximum
            )
            enqueue(payloads, marksLastAsSynced: true)
        } catch {
            lastError = error.localizedDescription
        }
    }

    private func enqueue(_ payloads: [Data], marksLastAsSynced: Bool) {
        guard !payloads.isEmpty else { return }
        for (index, data) in payloads.enumerated() {
            nextWriteSequence &+= 1
            pendingWrites.append(PendingWrite(
                sequence: nextWriteSequence,
                data: data,
                completesSync: marksLastAsSynced && index == payloads.index(before: payloads.endIndex)
            ))
        }
        isWriting = true
        pumpWrites()
    }

    private func pumpWrites() {
        guard
            activeWrite == nil,
            !pendingWrites.isEmpty,
            let peripheral = connectedPeripheral,
            let characteristic = cardWriteCharacteristic
        else {
            isWriting = activeWrite != nil || !pendingWrites.isEmpty
            return
        }

        let next = pendingWrites.removeFirst()
        activeWrite = next
        peripheral.writeValue(next.data, for: characteristic, type: .withResponse)
        let sequence = next.sequence
        writeTimeoutTask?.cancel()
        writeTimeoutTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .seconds(8))
            guard !Task.isCancelled else { return }
            self?.handleWriteTimeout(sequence: sequence)
        }
    }
    private func handleWriteTimeout(sequence: UInt64) {
        guard activeWrite?.sequence == sequence else { return }
        activeWrite = nil
        pendingWrites.removeAll()
        isWriting = false
        lastError = "La sincronizzazione non ha ricevuto risposta. Riconnessione in corso…"
        if let peripheral = connectedPeripheral {
            central.cancelPeripheralConnection(peripheral)
        }
    }
    private func retryEncryptedSubscription(
        peripheral: CBPeripheral,
        characteristic: CBCharacteristic,
        error: Error
    ) -> Bool {
        let code = (error as NSError).code
        let retryableCodes = [
            CBATTError.Code.insufficientAuthentication.rawValue,
            CBATTError.Code.insufficientEncryption.rawValue
        ]
        guard
            (error as NSError).domain == CBATTErrorDomain,
            retryableCodes.contains(code),
            notificationRetryCount < 3
        else { return false }

        notificationRetryCount += 1
        phase = .preparing("Completa il pairing con Lume X3…")
        notificationRetryTask?.cancel()
        notificationRetryTask = Task { @MainActor [weak self, weak peripheral, weak characteristic] in
            try? await Task.sleep(for: .seconds(2))
            guard
                !Task.isCancelled,
                let self,
                let peripheral,
                let characteristic,
                self.connectedPeripheral === peripheral
            else { return }
            peripheral.setNotifyValue(true, for: characteristic)
        }
        return true
    }

    private func handleAction(_ action: LumeDeviceAction) {
        guard action.schemaVersion == LumeProtocol.schemaVersion else { return }

        switch action.type {
        case "ready":
            break
        case "priorities.sync.request":
            sendPriorities()
        case "priority.toggle":
            guard let id = action.id, let done = action.done else { return }
            priorities.setDone(id: id, done: done)
            sendPriorities()
        default:
            break
        }
    }

    private func fail(_ message: String) {
        lastError = message
        phase = .disconnected(message)
    }

    private func updatePhase(for state: CBManagerState) {
        switch state {
        case .poweredOn:
            break
        case .poweredOff:
            phase = .unavailable("Attiva Bluetooth per collegare Lume X3.")
        case .unauthorized:
            phase = .unavailable("Autorizza Bluetooth nelle Impostazioni di iOS.")
        case .unsupported:
            phase = .unavailable("Bluetooth non è disponibile su questo dispositivo.")
        case .resetting:
            phase = .unavailable("Bluetooth si sta riavviando…")
        case .unknown:
            phase = .idle
        @unknown default:
            phase = .idle
        }
    }
}

@MainActor
extension LumeBluetoothManager: @preconcurrency CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        updatePhase(for: central.state)
        if central.state == .poweredOn { restoreOrScan() }
    }

    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        guard let peripheral = (dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral])?.first else {
            return
        }
        nearbyPeripheral = peripheral
        if peripheral.state == .connected {
            prepare(peripheral)
        } else {
            connect()
        }
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        guard nearbyPeripheral == nil else { return }
        nearbyPeripheral = peripheral
        central.stopScan()
        phase = .found(peripheral.name ?? "Lume X3")
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        defaults.set(peripheral.identifier.uuidString, forKey: lastPeripheralKey)
        prepare(peripheral)
    }

    func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        connectedPeripheral = nil
        fail(error?.localizedDescription ?? "Connessione a Lume X3 non riuscita.")
    }

    func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        timestamp: CFAbsoluteTime,
        isReconnecting: Bool,
        error: Error?
    ) {
        connectedPeripheral = nil
        cardWriteCharacteristic = nil
        actionNotifyCharacteristic = nil
        pendingWrites.removeAll()
        writeTimeoutTask?.cancel()
        writeTimeoutTask = nil
        notificationRetryTask?.cancel()
        notificationRetryTask = nil
        notificationRetryCount = 0
        activeWrite = nil
        isWriting = false
        becameReady = false
        phase = .disconnected(error?.localizedDescription ?? "Lume X3 è disconnesso.")

        guard !isReconnecting, central.state == .poweredOn else { return }
        nearbyPeripheral = peripheral
        central.connect(peripheral, options: nil)
    }
}

@MainActor
extension LumeBluetoothManager: @preconcurrency CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error {
            fail(error.localizedDescription)
            return
        }
        guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
            fail("Servizio Lume non trovato sul dispositivo.")
            return
        }
        peripheral.discoverCharacteristics([cardWriteUUID, actionNotifyUUID], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error {
            fail(error.localizedDescription)
            return
        }
        for characteristic in service.characteristics ?? [] {
            switch characteristic.uuid {
            case cardWriteUUID:
                cardWriteCharacteristic = characteristic
            case actionNotifyUUID:
                actionNotifyCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            default:
                break
            }
        }
        guard cardWriteCharacteristic != nil, actionNotifyCharacteristic != nil else {
            fail("Canali di sincronizzazione Lume incompleti.")
            return
        }
        becomeReady()
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if let error {
            if retryEncryptedSubscription(
                peripheral: peripheral,
                characteristic: characteristic,
                error: error
            ) {
                return
            }
            fail(error.localizedDescription)
            return
        }
        notificationRetryTask?.cancel()
        notificationRetryTask = nil
        notificationRetryCount = 0
        becomeReady()
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error {
            lastError = error.localizedDescription
            return
        }
        guard characteristic.uuid == actionNotifyUUID, let data = characteristic.value else { return }
        do {
            handleAction(try LumeProtocol.decodeAction(data))
        } catch {
            lastError = "Messaggio non riconosciuto da Lume X3."
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == cardWriteUUID, let completed = activeWrite else { return }
        writeTimeoutTask?.cancel()
        writeTimeoutTask = nil
        activeWrite = nil

        if let error {
            pendingWrites.removeAll()
            isWriting = false
            lastError = error.localizedDescription
            return
        }

        if completed.completesSync {
            lastSyncDate = .now
        }
        pumpWrites()
    }
}

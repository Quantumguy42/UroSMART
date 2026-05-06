#include "Tasks/BleTask.h"

// Definitions for externs declared in BleTask.h
// NOTE: CLIENT_READY_RESPONSE is misspelled in the header ("RECIEVE") —
//       leave it unchanged as it must match the client implementation.
bool     ackReceived    = false;
bool     deviceConnected = false;
bool     txReady        = true;
uint16_t conn_id        = 0;

String respValue;


class AckCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
        respValue   = characteristic->getValue();
        ackReceived = true;
    }
};


class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        conn_id         = connInfo.getConnHandle();
        deviceConnected = true;
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        deviceConnected = false;
    }
};


// Module-level BLE handles, initialized in bleTaskSetup()
static NimBLEServer*         pServer      = nullptr;
static NimBLECharacteristic* txChar       = nullptr;
static NimBLECharacteristic* rxChar       = nullptr;
static NimBLEAdvertising*    pAdvertising = nullptr;


void bleTaskSetup() {
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setMTU(MAX_PACKET_SIZE);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    safeSerial("BLE TASK: Struct size: ");
    safeSerialn(sizeof(sample_s));

    safeSerial("BLE TASK: BLE MAC: ");
    safeSerialn(NimBLEDevice::getAddress().toString().c_str());

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    txChar = pService->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    rxChar = pService->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rxChar->setCallbacks(new AckCallback());

    txChar->createDescriptor("2901")->setValue("Sample Packet from ESP32");
    txChar->createDescriptor("2902");  // CCCD

    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->enableScanResponse(true);
    pAdvertising->setName(DEVICE_NAME);

    pService->start();

    safeSerialn("BLE TASK: Setup complete — idle, waiting for SD Card Manager notification");
}


void bleTask(void* pvParameters) {

    bleTaskSetup();

    uint8_t*     bleBuffer;
    ble_data_t   bleData;
    sd_request_t sdReadRequest;

    BLE_SM bleState = Idle;

    uint16_t mtu;

    const char endTransmissionMessage[MESSAGE_SIZE] = "end_trans";

    // --- Timing ---
    int      waitRespStart;
    int      dataTransferStart;
    uint32_t dataTransferTimeMs;

    uint32_t samplesSent = 0;

    SamplePacket samplePacket;

    while (true) {

        switch (bleState) {

            case Idle:
                // Drain any stale BLE buffers left over from a previous transmission
                if (xQueueReceive(bleDataQueue, &bleData, 0)) {
                    xQueueSend(freeBLEBufferQueue, &bleData.buffer, 0);
                    safeSerialn("BLE TASK: Freed stale BLE buffer");

                } else if (ulTaskNotifyTake(pdTRUE, 10)) {
                    safeSerialn("BLE TASK: Notification received — beginning advertising");
                    pAdvertising->start();
                    bleState = Advertising;
                }
                break;

            case Advertising:
                safeSerialn("BLE TASK: Advertising — waiting for client connection");
                bleState = WaitConnection;
                break;

            case WaitConnection:
                if (deviceConnected) {
                    bleState = Connecting;
                }
                break;

            case Connecting:
                safeSerial("BLE TASK: Connected. conn_id = ");
                safeSerialn(conn_id);

                delay(200);  // Allow MTU negotiation to complete

                mtu = pServer->getPeerMTU(conn_id);
                safeSerial("BLE TASK: Negotiated MTU: ");
                safeSerialn(mtu);

                samplePacket.init(mtu);

                sdReadRequest.bytes  = mtu - BLE_HEADER_SIZE;
                sdReadRequest.bytes -= sdReadRequest.bytes % Sample::getSampleBytes();

                safeSerial("BLE TASK: Samples per packet: ");
                safeSerialn(sdReadRequest.bytes / Sample::getSampleBytes());
                safeSerial("BLE TASK: Total packet size: ");
                safeSerialn(sdReadRequest.bytes);

                waitRespStart = millis();
                safeSerialn("BLE TASK: Waiting for client ready signal...");
                bleState = WaitClientReady;
                break;

            case WaitClientReady:
                if (ackReceived && respValue == CLIENT_READY_RESPONSE) {
                    safeSerialn("BLE TASK: Client ready — beginning transmission");
                    ackReceived = false;
                    bleState = BeginTransmission;
                } else if ((millis() - waitRespStart) > CLIENT_READY_TIMEOUT_MS) {
                    safeSerialn("BLE TASK: Client ready timeout — disconnecting");
                    ackReceived = false;
                    bleState = Disconnecting;
                }
                break;

            case BeginTransmission: {
                safeSerialn("BLE TASK: Pre-requesting data from SD Card Manager");

                // Request header first
                xQueueReceive(freeBLEBufferQueue, &bleBuffer, portMAX_DELAY);
                sdReadRequest.command = SD_READ_HEADER;
                sdReadRequest.buffer  = bleBuffer;
                sdReadRequest.bytes   = HEADER_SIZE;
                xQueueSend(sdRequestsQueue, &sdReadRequest, portMAX_DELAY);
                safeSerialn("BLE TASK: Sent header request");

                // Fill remaining free buffers with data read requests
                sdReadRequest.command = SD_READ_BLOCK;
                sdReadRequest.bytes   = BLE_BUFFER_SIZE;
                int bufCount = 1;
                while (xQueueReceive(freeBLEBufferQueue, &bleBuffer, 0) == pdTRUE) {
                    sdReadRequest.buffer = bleBuffer;
                    xQueueSend(sdRequestsQueue, &sdReadRequest, portMAX_DELAY);
                    safeSerialn("BLE TASK: Sent data request " + String(bufCount++));
                }

                dataTransferStart = millis();
                bleState = GetData;
                break;
            }

            case GetData:
                xQueueReceive(bleDataQueue, &bleData, portMAX_DELAY);

                #ifdef PRINT_TRANSMISSION_DEBUG
                    safeSerialn("BLE TASK: GetData — size: " + String(bleData.bytes)
                                + " fileStatus: " + String(bleData.fileStatus));
                #endif

                if (bleData.fileStatus == DATA_READ_ERROR) {
                    // Re-queue the failed read and retry
                    sdReadRequest.buffer = bleData.buffer;
                    xQueueSend(sdRequestsQueue, &sdReadRequest, portMAX_DELAY);
                    bleState = GetData;

                } else if (bleData.fileStatus == INVALID_DATA) {
                    safeSerialn("BLE TASK: **ERROR** Invalid data received before end of transmission");
                    bleState = GetData;

                } else if (deviceConnected) {
                    bleState = Transmit;

                } else {
                    bleState = FailureCache;
                }
                break;

            case Transmit:
                #ifdef PRINT_TRANSMISSION_DEBUG
                    safeSerialn("BLE TASK: Transmitting " + String(bleData.bytes) + " bytes");
                #endif

                txChar->setValue((uint8_t*)bleData.buffer, bleData.bytes);
                txChar->notify();

                if (deviceConnected) {
                    waitRespStart = millis();
                    bleState = WaitResponse;
                } else {
                    bleState = FailureCache;
                }
                break;

            case WaitResponse:
                #ifdef BLE_WAIT_FOR_CLIENT_ACK
                    if (ackReceived && respValue == ACK_RESPONSE) {
                        ackReceived = false;
                        bleState = TransferStatus;
                    } else if ((millis() - waitRespStart) > ACK_TIMEOUT_MS) {
                        ackReceived = false;
                        bleState = Transmit;  // Retry on timeout
                    }
                #else
                    bleState = TransferStatus;
                #endif
                break;

            case TransferStatus:
                #ifdef PRINT_TRANSMISSION_DEBUG
                    safeSerialn("BLE TASK: TransferStatus — fileStatus: " + String(bleData.fileStatus));
                #endif

                if (bleData.fileStatus == LAST_PACKET) {
                    txChar->setValue((uint8_t*)endTransmissionMessage, MESSAGE_SIZE);
                    txChar->notify();

                    dataTransferTimeMs    = millis() - dataTransferStart;
                    double transferSecs   = dataTransferTimeMs / 1000.0;
                    double throughputKBps = (samplesSent * Sample::getSampleBytes()) / (transferSecs * 1000.0);

                    safeSerialn("BLE TASK: ------ Transfer Complete ------");
                    safeSerial("BLE TASK: Samples sent:      "); safeSerialn(samplesSent);
                    safeSerial("BLE TASK: Duration (s):      "); safeSerialn(transferSecs);
                    safeSerial("BLE TASK: Rate (samples/s):  "); safeSerialn(samplesSent / transferSecs);
                    safeSerial("BLE TASK: Throughput (KB/s): "); safeSerialn(throughputKBps);
                    safeSerialn("BLE TASK: ---------------------------------");

                    samplesSent = 0;
                    bleState = Disconnecting;

                } else {
                    if (bleData.fileStatus == DATA) {
                        samplesSent += bleData.bytes / Sample::getSampleBytes();
                    }
                    // Return buffer to the read pipeline
                    sdReadRequest.buffer = bleData.buffer;
                    xQueueSend(sdRequestsQueue, &sdReadRequest, portMAX_DELAY);
                    bleState = GetData;
                }
                break;

            case Disconnecting: {
                // Return all in-flight BLE buffers to the free pool
                int bufCount = 1;
                do {
                    xQueueSend(freeBLEBufferQueue, &bleData.buffer, 0);
                    safeSerialn("BLE TASK: Freed BLE buffer " + String(bufCount++));
                } while (xQueueReceive(bleDataQueue, &bleData, 0));

                pServer->disconnect(conn_id);
                pAdvertising->stop();

                safeSerialn("BLE TASK: Idle — waiting for next transmission notification");
                bleState = Idle;
                break;
            }

            case FailureCache:
                safeSerialn("BLE TASK: Packet failed to send — returning to Idle");
                bleState = Idle;
                break;
        }

        vTaskDelay(1);
    }
}
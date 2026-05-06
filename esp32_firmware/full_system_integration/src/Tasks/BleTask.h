#ifndef BLE_TASK_H
#define BLE_TASK_H

#include "System/SystemQueues.h"
#include "System/SystemConfig.h"
#include "System/HelperFunctions.h"

#include <NimBLEDevice.h>
#include "DataSample.h"

#define MAX_PACKET_SIZE 247
#define MIN_MTU_SIZE 23
#define BLE_HEADER_SIZE 3

#define SERVICE_UUID            "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define RX_UUID  "12345678-1234-1234-1234-123456789abc"
#define TX_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"




enum BLE_SM {
    Idle,               // Default state of BLE
    Advertising,        // Begins advertizing to application
    WaitConnection,     // State to wait for connection
    Connecting,         // Runs initial connection process and initializes datapacket with negotiated MTU Size
    WaitClientReady,    // Waits for client to send the message that it is ready to recieve data
    BeginTransmission,  // Initialization before transmision
    GetData,            // Gets data from SD Card
    Transmit,           // Transmits data
    WaitResponse,       // "Do nothing" State while waiting for response
    TransferStatus,     // Checks if all data from log has been successfully transmitted
    FailureCache,       // If disconnect happense during transmition, store relevant info
    Disconnecting       // Disconnection actions
};

extern bool ackReceived;
extern bool deviceConnected;
extern bool txReady;
extern String respValue;
extern uint16_t conn_id;

#define MESSAGE_SIZE 16

#define ACK_RESPONSE "ACK"
#define CLIENT_READY_RESPONSE "READY_TO_RECIEVE"

#define ACK_TIMEOUT_MS 2000
#define CLIENT_READY_TIMEOUT_MS 10000


void bleTaskSetup();
void bleTask(void* pvParameters);

#endif


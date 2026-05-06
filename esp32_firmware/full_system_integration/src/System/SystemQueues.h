#ifndef SYSTEM_QUEUES_H
#define SYSTEM_QUEUES_H

#include "System/SystemConfig.h"

// ----- Request Commands -----
#define SD_READ_HEADER 2
#define SD_READ_BLOCK 1
#define SD_WRITE_BLOCK 0

#define HEADER 0
#define DATA 1
#define LAST_PACKET -1
#define DATA_READ_ERROR -2
#define INVALID_DATA -3

typedef struct
{
    uint8_t command;  // Command request
    uint8_t *buffer;  // x
    int bytes;
    // TaskHandle_t requester;
} sd_request_t;

typedef struct
{
    uint8_t *buffer;  // holds the 
    uint16_t bytes;        // number of bytes in the ble data packet
    int8_t fileStatus; 
} ble_data_t;

extern QueueHandle_t freeSampleBufferQueue; // Handles the pointers to each of the free buffers
extern QueueHandle_t freeBLEBufferQueue;    // Handles the pointers to each of the free ble buffers
extern QueueHandle_t sdRequestsQueue;       // Handles requests to access the SD card
extern QueueHandle_t bleDataQueue;          //Handles the data that was collected from the SD card

#endif


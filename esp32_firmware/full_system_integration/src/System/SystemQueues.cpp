
#include "System/SystemQueues.h"

QueueHandle_t freeSampleBufferQueue;  // Handles the pointers to each of the free buffers
QueueHandle_t freeBLEBufferQueue;
QueueHandle_t sdRequestsQueue;        // Handles requests to access the SD card
QueueHandle_t bleDataQueue;           //Handles the data that was collected from the SD card


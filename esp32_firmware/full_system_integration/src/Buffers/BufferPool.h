  
#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include "System/SystemConfig.h"

// Buffer pools
uint8_t sampleBuffers[NUM_SAMPLE_BUFFERS][SAMPLE_BUFFER_BYTES];
uint8_t bleBuffers[NUM_BLE_BUFFERS][BLE_BUFFER_SIZE];
// uint8_t fileHeaderBuffer[HEADER_SIZE];

// void sampleBufferPool(){
//     for (int i = 0; i < NUM_SAMPLE_BUFFERS; i++) {
//         uint8_t* ptr = sampleBuffers[i];
//         xQueueSend(freeSampleBufferQueue, &ptr, portMAX_DELAY);
//     }
// }


#endif




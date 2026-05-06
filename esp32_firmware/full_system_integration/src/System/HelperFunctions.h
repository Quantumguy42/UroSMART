#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include "System/SystemConfig.h"

extern SemaphoreHandle_t serialMutex;

template<typename T>
void safeSerial(T msg){
    if (xSemaphoreTake(serialMutex, portMAX_DELAY)){
        Serial.print(msg);
        xSemaphoreGive(serialMutex);
    }
}

template<typename T>
void safeSerialn(T msg){
    if (xSemaphoreTake(serialMutex, portMAX_DELAY)){
        Serial.println(msg);
        xSemaphoreGive(serialMutex);
    }
}

uint32_t positivePart(long int value);

// void safeSeriaBinln(int msg){
//     if (xSemaphoreTake(serialMutex, portMAX_DELAY)){
//         Serial.println(msg, BIN);
//         xSemaphoreGive(serialMutex);
//     }
// }

#endif


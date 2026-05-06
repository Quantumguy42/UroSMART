#include "HelperFunctions.h"

// Global variable for serialMutex
SemaphoreHandle_t serialMutex = xSemaphoreCreateMutex();

uint32_t positivePart(long int value){
    if (value < 0)
        return 0;
    return value;
}


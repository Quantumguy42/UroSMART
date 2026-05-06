#ifndef SAMPLING_TASK_H
#define SAMPLING_TASK_H

#include "DataSample.h"
#include "SignalFiltering.h"
#include "HX711.h"
#include "RTC.h"


#include "System/SystemConfig.h"
#include "System/SystemQueues.h"
#include "System/HelperFunctions.h"

void samplingTask(void* pvParameters);

#endif



  // #include "DataSample.h"
  // #include "SignalFiltering.h"

  #include "System/SystemConfig.h"
  #include "System/SystemQueues.h"

  #include "Buffers/BufferPool.h"

  #include "Tasks/BleTask.h"
  #include "Tasks/SamplingTask.h"
  #include "Tasks/SDManagerTask.h"

  TaskHandle_t samplingTaskHandle;
  TaskHandle_t sdManagerTaskHandle;
  TaskHandle_t bleTaskHandle;

  // For the ISR to triggern
  volatile bool transmitTime = false;

  // ISR 
  void IRAM_ATTR transmitEventISR(){

      // Tell the SD Manager task to archive current log file
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      vTaskNotifyGiveFromISR(sdManagerTaskHandle, &xHigherPriorityTaskWoken);

  }



  void setup() {
    
    Serial.begin(115200);

    // Attach the ISR
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(INTERRUPT_PIN),
        transmitEventISR,
        FALLING
    );

    // Create queues
    freeSampleBufferQueue = xQueueCreate(NUM_SAMPLE_BUFFERS, sizeof(uint8_t*));
    freeBLEBufferQueue = xQueueCreate(NUM_BLE_BUFFERS, sizeof(uint8_t*));

    bleDataQueue = xQueueCreate(NUM_BLE_BUFFERS, sizeof(ble_data_t));
    sdRequestsQueue = xQueueCreate(NUM_REQUEST_SLOTS, sizeof(sd_request_t));



    // Initialize sample buffer pool queue
    for (int i = 0; i < NUM_SAMPLE_BUFFERS; i++) {
        uint8_t* ptr = sampleBuffers[i];
        xQueueSend(freeSampleBufferQueue, &ptr, portMAX_DELAY);
    }

    // Initialize ble buffer pool queue
    for (int i = 0; i < NUM_BLE_BUFFERS; i++) {
        uint8_t* ptr = bleBuffers[i];
        xQueueSend(freeBLEBufferQueue, &ptr, portMAX_DELAY);
    }


    // Create tasks
    xTaskCreatePinnedToCore(
        samplingTask,           // task function
        "Sampling Task",        // name
        4096,                   // stack size
        NULL,                   // parameters
        0,                      // priority
        &samplingTaskHandle,    // task handle
        SAMPLING_TASK_CORE);

    xTaskCreatePinnedToCore(
        sdManagerTask,          // task function
        "SD Manager Task",      // name
        4096,                   // stack size
        NULL,                   // parameters
        1,                      // priority
        &sdManagerTaskHandle,   // task handle
        SD_MANAGER_TASK_CORE);


    xTaskCreatePinnedToCore(
        bleTask,                // task function
        "BLE Task",             // name
        8192,                   // stack size
        NULL,                   // parameters
        2,                      // priority
        &bleTaskHandle,         // task handle
        BLE_TASK_CORE);

    // xTaskCreatePinnedToCore(
    //     debugTask,
    //     "Debug Task",
    //     4096,
    //     NULL,
    //     1,
    //     NULL,
    //     BLE_TASK_CORE);
  }

  void loop() {
    // put your main code here, to run repeatedly:
    vTaskDelay(portMAX_DELAY);
  }


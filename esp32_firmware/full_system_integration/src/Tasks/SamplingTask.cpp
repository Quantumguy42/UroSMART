#include "Tasks/SamplingTask.h"

void samplingTask(void* pvParameters) {
    uint8_t*    buffer;
    sd_request_t request;

    Sample      curr_sample;
    Sample      prev_sample;

    Regression  reg;
    HX711       scale;
    RTC_DS3231  rtc;

    uint32_t calibrationValue;

    int sampleBegin;
    int intermediateTimeAnchor;
    int sampleDuration;

    prev_sample.data = {0, 0, 0};
    curr_sample.data = {0, 0, 0};

    // Preset the write request packet
    request.command = SD_WRITE_BLOCK;
    request.bytes   = SAMPLE_BUFFER_BYTES;

    // Setup the load cell
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

#ifdef TARE_ON_STARTUP
    calibrationValue = scale.read() >> BITS_TO_THROW_OUT;
#else
    calibrationValue = 0;
#endif

    reg.init(SAMPLE_RATE_HZ, REG_WINDOW_SIZE);
    rtc.begin();

    // Initialize the base time that all sampling rate timing will be based off of
    const TickType_t samplePeriod  = pdMS_TO_TICKS(1000 / SAMPLE_RATE_HZ);
    TickType_t       lastWakeTime  = xTaskGetTickCount();

    int byteArrayIndex = 0;

    // Sampling loop
    while (true) {

        sampleBegin = millis();

#ifdef PRINT_SAMPLING_TASK_TIMING
        intermediateTimeAnchor = millis();
#endif

        if (byteArrayIndex == 0) {
            // Grab a free buffer to use
            xQueueReceive(freeSampleBufferQueue, &buffer, portMAX_DELAY);
            request.buffer = buffer;
        }

#ifdef PRINT_SAMPLING_TASK_TIMING
        sampleDuration = millis() - intermediateTimeAnchor;
        safeSerialn("SAMPLING TASK: ---------------------------------------------------- ");
        safeSerialn("SAMPLING TASK: Queue retrieve time:\t" + String(sampleDuration));
        intermediateTimeAnchor = millis();
#endif

#ifdef USE_RTC
        curr_sample.setTimeStamp(rtc.now());
#else
        curr_sample.data.timeStamp += 1;
#endif

#ifdef PRINT_SAMPLING_TASK_TIMING
        sampleDuration = millis() - intermediateTimeAnchor;
        safeSerialn("SAMPLING TASK: RTC time:\t" + String(sampleDuration));
        intermediateTimeAnchor = millis();
#endif

        // GET DATA
#ifdef USE_SENSOR_DATA
#ifdef CUTOFF_NEGATIVE_VOLUME
        curr_sample.data.volume = positivePart((scale.read() >> BITS_TO_THROW_OUT) - calibrationValue) * LOADCELL_SCALING_FACTOR;
#else
        curr_sample.data.volume = (int32_t)((scale.read() >> BITS_TO_THROW_OUT) - calibrationValue) * LOADCELL_SCALING_FACTOR;
#endif

        curr_sample.data.flow = reg.update_regression(curr_sample.data.volume);
        prev_sample = curr_sample;
#else
        curr_sample.data.volume += 0.5;
        curr_sample.data.flow = reg.update_regression(curr_sample.data.volume);
#endif

#ifdef PRINT_SAMPLING_TASK_TIMING
        sampleDuration = millis() - intermediateTimeAnchor;
        safeSerialn("SAMPLING TASK: Loadcell time:\t" + String(sampleDuration));
        intermediateTimeAnchor = millis();
#endif

        // Write sample to the byte array buffer
        memcpy(&buffer[byteArrayIndex], &curr_sample.data, Sample::getSampleBytes());

#ifdef PRINT_SAMPLING_TASK_TIMING
        sampleDuration = millis() - intermediateTimeAnchor;
        safeSerialn("SAMPLING TASK: Copy data time:\t" + String(sampleDuration));
        intermediateTimeAnchor = millis();
#endif

        sampleDuration = millis() - sampleBegin;

#ifdef PRINT_SAMPLING_TASK_TIMING
        safeSerialn("SAMPLING TASK: Sample loop duration: " + String(sampleDuration));
        safeSerialn("SAMPLING TASK: ---------------------------------------------------- ");
#endif

#ifdef PRINT_LOADCELL_DATA
        safeSerialn("SAMPLING TASK: " + curr_sample.toCSVString());
#endif

        byteArrayIndex = (byteArrayIndex + Sample::getSampleBytes()) % SAMPLE_BUFFER_BYTES;

        // If the buffer is full, send it to the SD card write queue
        if (byteArrayIndex == 0) {
            xQueueSend(sdRequestsQueue, &request, portMAX_DELAY);
        }

        // Wait for the next sample period (non-blocking)
        vTaskDelayUntil(&lastWakeTime, samplePeriod);
    }
}
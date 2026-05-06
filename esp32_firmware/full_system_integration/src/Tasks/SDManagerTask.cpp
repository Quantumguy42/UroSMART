#include "Tasks/SDManagerTask.h"

void sdManagerTask(void* pvParameters) {
    sd_request_t  request;
    ble_data_t    bleData;
    Sample        sample;
    fileheader_s  fileHeader;

    UroFlowLogger logger(DEVICE_NAME, SAMPLE_RATE_HZ, Binary);

    bool transmissionInProgress = false;
    bool isFirstWriteReq        = true;

#if defined(READ_FROM_SD_CARD) || defined(WRITE_TO_SD_CARD)
    logger.beginSD();
#endif

    while (true) {

        // Block until a request is received
        if (!xQueueReceive(sdRequestsQueue, &request, portMAX_DELAY)) continue;

        switch (request.command) {

            // ----------------------------------------------------------------
            case SD_WRITE_BLOCK:
            // ----------------------------------------------------------------

                #ifdef PRINT_DATA_WRITTEN_TO_SD
                if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
                    Serial.println("SD TASK: -------- WRITE REQUEST RECEIVED --------");
                    for (int i = 0; i < request.bytes; i += Sample::getSampleBytes()) {
                        memcpy(&sample.data, &request.buffer[i], Sample::getSampleBytes());
                        Serial.println("SD TASK: " + String(sample.toCSVString()));
                    }
                    Serial.println("SD TASK: First timestamp: " + String(UroFlowLogger::getFirstTimestampFromBlock(request.buffer)));
                    Serial.println("SD TASK: Last timestamp:  " + String(UroFlowLogger::getLastTimestampFromBlock(request.buffer, request.bytes)));
                    Serial.println("SD TASK: ----------------------------------------");
                    xSemaphoreGive(serialMutex);
                }
                #endif

                // Transmit interrupt triggered — archive current log and notify BLE task
                if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
                    safeSerialn("SD TASK: Transmit interrupt triggered — archiving and notifying BLE task");
                    xTaskNotifyGive(bleTaskHandle);

                    #ifdef WRITE_TO_SD_CARD
                    safeSerialn("SD TASK: Archiving log of size " + logger.getCurrLogSize());
                    String archivedPath = logger.archiveLog();

                    if (!logger.openNewLog(UroFlowLogger::getFirstTimestampFromBlock(request.buffer)))
                        safeSerialn("SD TASK: **ERROR** Could not create new log file");

                    safeSerialn("SD TASK: Archived to " + archivedPath);
                    safeSerialn("SD TASK: New log opened at " + logger.getCurrLogFullPath());
                    #endif

                } else if (isFirstWriteReq) {
                    // First write on startup — open a new log file
                    #ifdef WRITE_TO_SD_CARD
                    if (!logger.openNewLog(UroFlowLogger::getFirstTimestampFromBlock(request.buffer)))
                        safeSerialn("SD TASK: **ERROR** Could not create log file at " + logger.getCurrLogFullPath());
                    #endif

                    safeSerialn("SD TASK: Opened new log file at " + logger.getCurrLogFullPath());
                    isFirstWriteReq = false;

                } else {
                    // Normal write — reopen current log
                    #ifdef WRITE_TO_SD_CARD
                    if (!logger.openCurrLog())
                        safeSerialn("SD TASK: **ERROR** Could not open log at \"" + logger.getCurrLogFullPath() + "\"");
                    #endif
                }

                #ifdef WRITE_TO_SD_CARD
                #ifdef PRINT_DATA_WRITTEN_TO_SD
                safeSerialn("SD TASK: Writing to " + logger.getCurrLogFullPath());
                #endif

                switch (logger.writeSampleBinaryBlock(request.buffer, request.bytes)) {
                    case NO_LOG_OPEN:      safeSerialn("SD TASK: **ERROR** No log file open");             break;
                    case INVALID_BLOCK:    safeSerialn("SD TASK: **ERROR** Incompatible block size");       break;
                    case INVALID_FILE_TYPE:safeSerialn("SD TASK: **ERROR** File type is not Binary");       break;
                    case FAILED_WRITE:     safeSerialn("SD TASK: **ERROR** Failed to write data");          break;
                }

                #ifdef PRINT_DATA_WRITTEN_TO_SD
                safeSerialn("SD TASK: Log size after write: " + String(logger.getCurrLogSize()));
                #endif
                #endif  // WRITE_TO_SD_CARD

                // Return buffer to the sampling task
                xQueueSend(freeSampleBufferQueue, &request.buffer, portMAX_DELAY);
                break;


            // ----------------------------------------------------------------
            case SD_READ_HEADER:
            // ----------------------------------------------------------------

                if (request.bytes != HEADER_SIZE || HEADER_SIZE != sizeof(fileheader_s)) {
                    safeSerialn("SD TASK: **ERROR** Header size mismatch:");
                    safeSerialn("SD TASK:   HEADER_SIZE:          " + String(HEADER_SIZE));
                    safeSerialn("SD TASK:   request.bytes:        " + String(request.bytes));
                    safeSerialn("SD TASK:   sizeof(fileheader_s): " + String(sizeof(fileheader_s)));
                }

                bleData.buffer     = request.buffer;
                bleData.fileStatus = HEADER;

                #ifdef PRINT_TRANSMISSION_DEBUG
                safeSerialn("SD TASK: -------- READ HEADER REQUEST RECEIVED --------");
                safeSerialn("SD TASK: Header size: " + String(request.bytes));
                #endif

                #ifdef READ_FROM_SD_CARD

                #ifdef TEST_TRANSMIT_FILE
                if (logger.openArchivedFileToTransmit(TEST_TRANSMIT_FILE) == FILE_NOT_OPEN)
                    safeSerialn("SD TASK: **ERROR** Could not open test file for transmission");
                #else
                if (logger.openArchivedFileToTransmit() == FILE_NOT_OPEN)
                    safeSerialn("SD TASK: **ERROR** Could not open archived file for transmission");
                #endif

                #ifdef PRINT_TRANSMISSION_DEBUG
                safeSerialn("SD TASK: Reading from " + logger.getTransmitFileFullPath()
                            + " at position " + logger.getNextTransmitPos());
                #endif

                switch (logger.readArchivedFileData(request.bytes, bleData.buffer, &bleData.bytes)) {
                    case FILE_NOT_OPEN:
                        safeSerialn("SD TASK: **ERROR** No archived file open to read header");
                        break;
                    case END_OF_ARCHIVED_FILE:
                        safeSerialn("SD TASK: **ERROR** Reached end of file while reading header");
                        break;
                    case SUCCESS:
                        break;
                }

                #ifdef PRINT_TRANSMISSION_DEBUG
                {
                    fileheader_s header;
                    memcpy(&header, bleData.buffer, bleData.bytes);
                    if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
                        Serial.println("SD TASK: -------- HEADER --------");
                        Serial.println("SD TASK: Device name:   " + String(header.deviceName));
                        Serial.println("SD TASK: Time started:  " + String(header.timeStarted));
                        Serial.println("SD TASK: Sample rate:   " + String(header.sampleRateHz) + " Hz");
                        Serial.println("SD TASK: Duration:      "
                            + String(header.logDuration.hours)   + "h "
                            + String(header.logDuration.minutes) + "m "
                            + String(header.logDuration.seconds) + "s");
                        Serial.println("SD TASK: ----------------------");
                        xSemaphoreGive(serialMutex);
                    }
                }
                #endif

                #else  // !READ_FROM_SD_CARD — synthetic header for testing

                strcpy(fileHeader.deviceName, DEVICE_NAME);
                fileHeader.timeStarted  = 12334;
                fileHeader.sampleRateHz = 4;
                fileHeader.logDuration  = {10, 2, 15};
                bleData.bytes = request.bytes;
                memcpy(bleData.buffer, &fileHeader, request.bytes);

                #endif  // READ_FROM_SD_CARD

                xQueueSend(bleDataQueue, &bleData, portMAX_DELAY);
                transmissionInProgress = true;
                break;


            // ----------------------------------------------------------------
            case SD_READ_BLOCK:
            // ----------------------------------------------------------------

                if (!transmissionInProgress) {
                    // Already reached end of file — discard the stale request
                    #ifdef PRINT_TRANSMISSION_DEBUG
                    safeSerialn("SD TASK: Read request ignored — transmission already complete");
                    #endif

                    bleData.fileStatus = INVALID_DATA;
                    xQueueSend(bleDataQueue, &bleData, portMAX_DELAY);
                    break;
                }

                #ifdef PRINT_TRANSMISSION_DEBUG
                safeSerialn("SD TASK: -------- READ DATA REQUEST RECEIVED --------");
                safeSerialn("SD TASK: Requested bytes: " + String(request.bytes));
                #endif

                bleData.buffer = request.buffer;

                #ifdef READ_FROM_SD_CARD

                #ifdef TEST_TRANSMIT_FILE
                if (logger.openArchivedFileToTransmit(TEST_TRANSMIT_FILE) == FILE_NOT_OPEN)
                    safeSerialn("SD TASK: **ERROR** Could not open test file for transmission");
                #endif

                #ifdef PRINT_TRANSMISSION_DEBUG
                safeSerialn("SD TASK: Reading from " + logger.getTransmitFileFullPath()
                            + " at position " + logger.getNextTransmitPos());
                #endif

                switch (logger.readArchivedFileData(request.bytes, bleData.buffer, &bleData.bytes)) {
                    case FILE_NOT_OPEN:
                        safeSerialn("SD TASK: **ERROR** No archived file open to read");
                        break;

                    case END_OF_ARCHIVED_FILE:
                        #ifdef PRINT_TRANSMISSION_DEBUG
                        safeSerialn("SD TASK: End of file reached");
                        #endif

                        bleData.fileStatus     = LAST_PACKET;
                        transmissionInProgress = false;

                        #ifdef TEST_TRANSMIT_FILE
                        safeSerialn("SD TASK: Closing test file as " + logger.closeTransmissionFile());
                        #else
                        safeSerialn("SD TASK: Closing transmission file as " + logger.closeAndRenameTransmissionFile());
                        #endif
                        break;

                    case SUCCESS:
                        #ifdef PRINT_TRANSMISSION_DEBUG
                        safeSerialn("SD TASK: Read successful");
                        #endif
                        bleData.fileStatus = DATA;
                        break;
                }

                #ifdef PRINT_TRANSMISSION_DEBUG
                {
                    if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
                        Serial.println("SD TASK: -------- DATA READ --------");
                        for (int i = 0; i < bleData.bytes; i += Sample::getSampleBytes()) {
                            memcpy(&sample.data, &bleData.buffer[i], Sample::getSampleBytes());
                            Serial.println("SD TASK: " + String(sample.toCSVString()));
                        }
                        Serial.println("SD TASK: --------------------------");
                        xSemaphoreGive(serialMutex);
                    }
                }
                #endif

                #else  // !READ_FROM_SD_CARD — synthetic data for testing
                {
                    static Sample     bleSample;
                    static int        samplesRead  = 0;
                    const  int        totalSamples = 345600;

                    bleData.fileStatus = DATA;
                    bleData.bytes      = 0;

                    for (int i = 0; i < request.bytes; i += Sample::getSampleBytes()) {
                        bleSample.data.timeStamp += 1;
                        bleSample.data.volume    += 0.5;
                        bleSample.data.flow      += 0.001;
                        bleData.bytes            += Sample::getSampleBytes();
                        samplesRead++;

                        memcpy(&bleData.buffer[i], &bleSample.data, Sample::getSampleBytes());

                        if (samplesRead >= totalSamples) {
                            bleData.fileStatus     = LAST_PACKET;
                            transmissionInProgress = false;
                            samplesRead            = 0;
                            break;
                        }
                    }
                }
                #endif  // READ_FROM_SD_CARD

                #ifdef PRINT_TRANSMISSION_DEBUG
                safeSerialn("SD TASK: Bytes read: " + String(bleData.bytes));
                #endif

                xQueueSend(bleDataQueue, &bleData, portMAX_DELAY);
                break;

        }  // switch
    }  // while
}
#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <Arduino.h>


// ================================================================
//  Hardware Pins
// ================================================================

#define INTERRUPT_PIN       0   // Button/interrupt to trigger BLE transmission
#define LOADCELL_DOUT_PIN   35
#define LOADCELL_SCK_PIN    32


// ================================================================
//  FreeRTOS Task Core Assignments
// ================================================================

#define SAMPLING_TASK_CORE      0
#define SD_MANAGER_TASK_CORE    1   // Core 1 prevents SD writes from tripping the watchdog
#define BLE_TASK_CORE           1

extern TaskHandle_t samplingTaskHandle;
extern TaskHandle_t sdManagerTaskHandle;
extern TaskHandle_t bleTaskHandle;


// ================================================================
//  Device Identity & Sampling
// ================================================================

#define DEVICE_NAME         "uroflowlogger"
#define SAMPLE_RATE_HZ      4
#define REG_WINDOW_SIZE     30      // Samples used in the flow regression window


// ================================================================
//  Load Cell
// ================================================================

// Raw ADC bits discarded before scaling (reduces noise)
#define BITS_TO_THROW_OUT           9

// Scale factor: maps shifted ADC counts to mL
#define LOADCELL_SCALING_FACTOR     1.2165

// Alternative calibration profiles (uncomment one to use):
// #define LOADCELL_SCALING_FACTOR  0.4113  // Mounted version, 7 bits discarded
// #define BITS_TO_THROW_OUT        7

// Zero the load cell on startup using the first raw reading as the tare value
#define TARE_ON_STARTUP

// Clamp negative volume readings to zero (disable to allow signed values)
// #define CUTOFF_NEGATIVE_VOLUME


// ================================================================
//  Sample Buffers  (Sampling Task → SD Manager)
// ================================================================

#define SAMPLE_SIZE             Sample::getSampleBytes()    // Bytes per sample struct
#define SAMPLES_PER_BUFFER      10
#define SAMPLE_BUFFER_BYTES     (SAMPLES_PER_BUFFER * 16)  // Must match sample struct size
#define NUM_SAMPLE_BUFFERS      5                           // Pool size; increase if writes stall sampling


// ================================================================
//  BLE Buffers  (SD Manager → BLE Task)
// ================================================================

#define NUM_BLE_BUFFERS     5
#define BLE_BUFFER_SIZE     240     // 15 samples × 16 bytes — must fit within negotiated MTU
#define HEADER_SIZE         64      // File header size in bytes; must be ≤ BLE_BUFFER_SIZE


// ================================================================
//  SD Request Queue
// ================================================================

#define NUM_REQUEST_SLOTS   64      // Depth of the shared SD request queue


// ================================================================
//  FreeRTOS Trace (set to 0 to disable runtime stats)
// ================================================================

#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1


// ================================================================
//  Feature Flags
//  Comment out any flag to disable that subsystem.
// ================================================================

#define BLE_WAIT_FOR_CLIENT_ACK     // Require ACK from client before sending next packet
#define USE_SD_CARD
#define READ_FROM_SD_CARD           // Read archived logs for BLE transmission
#define WRITE_TO_SD_CARD            // Write incoming samples to SD
#define USE_SENSOR_DATA             // Use real load cell data (disable for synthetic test data)
#define USE_RTC                     // Use DS3231 RTC for timestamps (disable for counter-based)


// ================================================================
//  Debug / Logging Flags
//  Comment out any flag to silence that output.
// ================================================================

#define PRINT_TRANSMISSION_DEBUG    // BLE + SD read/write trace during transmission
#define PRINT_DATA_WRITTEN_TO_SD    // Log every sample block written to SD
// #define PRINT_SAMPLING_TASK_TIMING  // Per-stage timing inside the sampling loop (typo fixed from TIMIMG)
// #define PRINT_LOADCELL_DATA         // Print every sample to serial


// ================================================================
//  Test Transmission File (optional)
//  Uncomment exactly one to transmit a fixed file instead of the
//  dynamically selected archived log. File must exist on the SD card.
// ================================================================

// #define TEST_TRANSMIT_FILE  "_data_log_37_samples"
// #define TEST_TRANSMIT_FILE  "_test_log_345600_samples"
// #define TEST_TRANSMIT_FILE  "_test_log_20000_samples"


#endif // SYSTEM_CONFIG_H
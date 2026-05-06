#ifndef UROFLOWLOGGER_H
#define UROFLOWLOGGER_H

// For SD Card Storage
#include <Arduino.h>
// #include <RTCLib.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include "DataSample.h"

#define LOGGER_FILE_READ       "r"
#define LOGGER_FILE_WRITE      "w"
#define LOGGER_FILE_APPEND     "a"

#define INDEX_FILE_NAME   "index.txt"
#define STATUS_FILE_NAME  "status.bin"
#define METADATA_RELATIVE_PATH "metadata/"

#define LOG_PREFIX "data_log_"
#define LOG_SUFFIX ".csv"


// Error codes
#define SUCCESS 0
#define NO_LOG_OPEN -1
#define INVALID_BLOCK -2
#define INVALID_FILE_TYPE -3
#define FILE_NOT_OPEN -4
#define END_OF_ARCHIVED_FILE -5
#define FAILED_WRITE -6

#define FUNCTION_NOT_RUN 0

#define ARCHIVED_PREFIX "_"
#define TRANSMITTED_PREFIX "~"

// #define ENABLE_DEBUG_PRINT_STATEMENTS

// Status: transmittionFileName, nextTransmitFilePos 

typedef struct __attribute__((packed)) {
  char transmittionFileName[50];
  uint32_t nextTransmitFilePos;
} trans_status_s;


typedef struct __attribute__((packed)) {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} duration_s;

typedef uint16_t padding_t;

// Size of 64 bytes
typedef struct __attribute__((packed)) {
  char deviceName[50];
  uint64_t timeStarted;
  uint8_t sampleRateHz;
  duration_s logDuration;
  padding_t padding; // So header size is a multiple of 16
} fileheader_s;

enum FileType {CSV, Binary, NoType};

// typedef struct __attribute__((packed)) {
//   int SCK;
//   int MISO;
//   int MOSI;
//   int CS;
//   uint32_t frequency;
//   } spisetup_s;


struct __attribute__((packed)) spisetup_s{
  int SCK;
  int MISO;
  int MOSI;
  int CS;
  uint32_t frequency;

  spisetup_s() : SCK(18), MISO(19), MOSI(23), CS(5), frequency(4000000) {} // default
  spisetup_s(int sck, int miso, int mosi, int cs, uint32_t freq)
    : SCK(sck), MISO(miso), MOSI(mosi), CS(cs), frequency(freq) {}
};

class UroFlowLogger {
  private:
    File logFile;
    File transmissionFile;

    int connectionRetries;

    spisetup_s spiSetup;

    String deviceName;
    int sampleRate;
    timestamp_t logBeginEpoch;
    timestamp_t lastSampleEpoch;

    int logFileEndPos;

    String loggingRootDir;

    String currLogPrefix;
    String currLogExt;
    String currLogFullPath;
    String currLogFileName;

    String transmitLogPath;
    String transmitFileName;
    String transmitLogPrefix;
    String transmitLogExt;
    String transmitLogFullPath;

    
    int nextTransmitFilePos;
    
    // trans_status_s transmissionStatus;

    String metaDataRelPath;
    String indexfFileName;
    String statusFileName;


    // RTC_DS3231 rtc;

    int headerEndPos;
    int hoursPos;
    int minutesPos;
    int secondsPos;

    FileType fileType;

    void writeHeader(timestamp_t firstSampleEpoch);

  public: 

    UroFlowLogger(String deviceName, int sampleRate);
    UroFlowLogger(String deviceName, int sampleRate, FileType fileTypeInp);

    static timestamp_t getFirstTimestampFromBlock(uint8_t* sampleBlockBinary);
    static timestamp_t getLastTimestampFromBlock(uint8_t* sampleBlockBinary, int blockSize);

    // NEW
    int     updateStatusFile(const char* archivedFileName, int filePosition);
    bool    initLogger(const char* path, spisetup_s spiSetupInp, int retries);
    void    initInternalValues();
    String  archiveLog();
    int     openArchivedFileToTransmit();
    int     openArchivedFileToTransmit(String archivedFileName);
    int     readArchivedFileData(int bufferBytes, uint8_t* buffer, uint16_t* bytesRead);
    bool    closeTransmissionFile();
    String  closeAndRenameTransmissionFile();
    String  findFirstWithPrefix(const char* prefix);

    // Initializers
    // bool beginRTC();
    bool beginSD(const char* path = "/", spisetup_s spiSetup = spisetup_s(), int retries = 5);

    bool setLogType(FileType fileTypeInp);

    bool openNewLog(timestamp_t firstSampleEpoch);
    bool openNewLog(String fileName, timestamp_t firstSampleEpoch);
    bool createNewLog(timestamp_t firstSampleEpoch);
    bool openCurrLog();
    bool closeLog();
    bool deleteStatusFile();

    bool writeSample(Sample sample);
    int  writeSampleBinaryBlock(uint8_t* sampleBlockBinary, int blockSize);
    // bool writeLine(String line);
    
    // Getters
    // RTC_DS3231  getRTC();
    String      getCurrLogFullPath();
    String      getTransmitFileFullPath();
    // DateTime    getRTCTime();
    timestamp_t getLastSampleEpoch();
    int         getNextTransmitPos();
    int         getCurrLogSize();
    int         getTransmitFileSize();

    bool sdConnected();

    static String getFilesInDir(const char* rootPath = "/");

    bool initSD(int SCK = 18, int MISO = 19, int MOSI = 23, int CS = 5, uint32_t frequency = 4000000);

};



#endif // UROFLOWLOGGER_H

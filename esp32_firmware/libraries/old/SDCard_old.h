#ifndef SDCARD_H
#define SDCARD_H

// For SD Card Storage
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "DataSample.h"

#define PRINT_WRITE_TIMES  // enable debug serial output

// ===== SD Card Functions =====
bool initSD(int SCK = 18, int MISO = 19, int MOSI = 23, int CS = 5, uint32_t frequency = 4000000, int retry = 5) {
 for (int i = 0; i < retry; i++) {
    Serial.println("Mounting SD card...");
    SPI.begin(SCK, MISO, MOSI, CS);  // SCK, MISO, MOSI, CS
    if (!SD.begin(CS, SPI, frequency)) {
      Serial.println("SD mount failed!");
    } else {
      Serial.println("SD mounted.");
      return true;
    }
    delay(1000);
  }
  Serial.print("Unable to mount SD card after ");
  Serial.print(retry);
  Serial.println(" retries.");
  return false;
}


bool openFile(File &file, const char* path = "/flow_data.csv", const char* mode = FILE_WRITE) {
  file = SD.open(path, mode);
  if (!file) {
    Serial.println("Failed to open file");
    return false;
  }
  return true;
}

int numberOfLogs(const char* rootPath = "/", const String filePrefix = "flow_data", const String fileSuffix = ".csv") {
  int count = 0;
  File root = SD.open(rootPath);

  File file = root.openNextFile();
  while (file) {
    Serial.println(String("Found file: ") + String(file.name()));
    if (String(file.name()).startsWith(filePrefix) && String(file.name()).endsWith(fileSuffix)) {
      count++;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  Serial.print("Number of existing logs with prefix and suffix ");
  Serial.print(filePrefix);
  Serial.print(", ");
  Serial.print(fileSuffix);
  Serial.print(": ");
  Serial.println(count);
  return count;
}

bool sdIsConnected(){
  int durationTime;

  int startTime = micros();
  bool isConnected = SD.open("/");
  durationTime = micros() - startTime;

  Serial.printf("isConnected() took %d us\n", durationTime);

  return isConnected;
}

// ===== Write binary sample (optional) =====
void writeSampleBinary(File &file, Sample &s) {

  #ifdef PRINT_WRITE_TIMES
    int startTime = micros();
    file.write((uint8_t*)&s, sizeof(Sample));
    int durationTime = micros() - startTime;

    Serial.printf("Wrote binary sample of size %d in %d us\n", sizeof(Sample), sizeof(s), durationTime);
  #else
    file.write((uint8_t*)&s, sizeof(Sample));
  #endif
}

void writeSampleString(File &file, Sample &s) {
  // Print sample

  #ifdef PRINT_WRITE_TIMES
    int startTime = micros();

    file.println(s.toCSVString());
    int durationTime = micros() - startTime;

    Serial.printf("Wrote string sample in %d us\n", durationTime);
  #else
    file.print(s.timeStamp);
    file.print(",");
    file.print(s.flow);
    file.print(",");
    file.println(s.volume);
  #endif
}

#endif // SDCARD_H

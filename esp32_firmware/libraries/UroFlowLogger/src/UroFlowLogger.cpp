#include "UroFlowLogger.h"


void UroFlowLogger::initInternalValues(){
  this->metaDataRelPath = METADATA_RELATIVE_PATH;
  this->indexfFileName = INDEX_FILE_NAME;
  this->statusFileName = STATUS_FILE_NAME;
  this->currLogPrefix = LOG_PREFIX;
  this->transmitFileName = "";

}

// -------------- TRAMSMISSION FUNCTIONS --------------

bool UroFlowLogger::deleteStatusFile(){
  SD.remove(this->loggingRootDir + String(STATUS_FILE_NAME));
  return true;
}

int UroFlowLogger::updateStatusFile(const char* archivedFileName, int filePosition){
  trans_status_s fileStatus;
  strcpy(fileStatus.transmittionFileName, archivedFileName);
  fileStatus.nextTransmitFilePos = filePosition;

  File statusFile = SD.open(this->loggingRootDir + String(STATUS_FILE_NAME), LOGGER_FILE_WRITE);
  
  if (!statusFile)
    return FILE_NOT_OPEN;

  statusFile.write((uint8_t*)&fileStatus, sizeof(fileStatus));
  statusFile.close();

  return SUCCESS;
}

timestamp_t UroFlowLogger::getFirstTimestampFromBlock(uint8_t* sampleBlockBinary){
  timestamp_t timestamp;
  memcpy(&timestamp, &sampleBlockBinary[0], Sample::getTimeStampBytes());
  return timestamp;
}

timestamp_t UroFlowLogger::getLastTimestampFromBlock(uint8_t* sampleBlockBinary, int blockSize){
  timestamp_t timestamp;
  memcpy(&timestamp, &sampleBlockBinary[blockSize-Sample::getSampleBytes()], Sample::getTimeStampBytes());

  return timestamp;
}


// Does the process of archiving the current log file
String UroFlowLogger::archiveLog(){
  // Closes and renames current files
  
  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UroFlowLogger DEBUG: Size before archiving: " + this->logFile.size());
  #endif
  if (this->logFile)
    this->closeLog();
  String archiveLogFileName = ARCHIVED_PREFIX + this->currLogFileName;
  String archivedLogFullPath = this->loggingRootDir + archiveLogFileName + String(this->currLogExt);

  // Continually updates name until it is unique
  int i = 1;
  while (SD.exists(archivedLogFullPath)){
    i++;
    archiveLogFileName = ARCHIVED_PREFIX + this->currLogFileName + "(" + i + ")";
    archivedLogFullPath = this->loggingRootDir + archiveLogFileName + String(this->currLogExt);
  }

  if (SD.exists(this->currLogFullPath)){
    SD.rename(this->currLogFullPath, archivedLogFullPath);
    // this->updateStatusFile(archivedLogFullPath.c_str(), 0);

    // Set this archived file to be the one that is transmitted
    this->transmitFileName = archiveLogFileName;
    this->transmitLogFullPath = archivedLogFullPath;

    return archivedLogFullPath;
  } else {
    return "COULD NOT FIND FILE TO RENAME";
  }
  

    
}

int UroFlowLogger::openArchivedFileToTransmit(String archivedFileName){

  this->transmitFileName = archivedFileName;

  return this->openArchivedFileToTransmit();
  
}

// CURRENT WORK IN PROGRESS
int UroFlowLogger::openArchivedFileToTransmit(){


  File statusFile;
  trans_status_s transmissionStatus;


  // Check if file already open
  if (this->transmissionFile)
    return FUNCTION_NOT_RUN;
  
  // SINCE THE FILE IS ALWAYS OPEN, NO NEED FOR A STATUS FILE

  // // First check the transmission status file 
  // statusFile = SD.open(this->loggingRootDir + String(STATUS_FILE_NAME), LOGGER_FILE_READ);

  // if (statusFile){ // If a status file exists, pull data from it

  //   statusFile.read((uint8_t*)&transmissionStatus, sizeof(trans_status_s));
  //   this->transmitFileName = String(transmissionStatus.transmittionFileName);
  //   this->nextTransmitFilePos = transmissionStatus.nextTransmitFilePos;

  //   statusFile.close();

  if (this->transmitFileName == "") { // If this is a new transmission, find a new archived file

    // Find the oldest archived file that has not yet been transmitted
    this->transmitFileName = this->findFirstWithPrefix(ARCHIVED_PREFIX);
    this->nextTransmitFilePos = 0;

  }

  this->transmitLogFullPath = this->loggingRootDir + this->transmitFileName + this->currLogExt;

  // Open found archived file
  this->transmissionFile = SD.open(this->transmitLogFullPath, LOGGER_FILE_READ);

  if (!this->transmissionFile){
    return FILE_NOT_OPEN;
  }

  //   // file.read(buffer, sizeof(buffer)) || file.gcount() > 0



  // String transFileName = 

  // if (transFileName == "")
  //   return FUNCTION_NOT_RUN;
  
  // this->transmitFileName = transFileName;
  // this->transmitLogFullPath = this->loggingRootDir + this->transmitFileName;
  
  
  // this->transmissionFile = SD.open(this->transmitLogFullPath, LOGGER_FILE_READ);
  // if (!logFile) {
  //   Serial.println("UoFlowLogger DEBUG: Failed to open file");
  //   return false;
  // }

  return SUCCESS;

  
}

int UroFlowLogger::readArchivedFileData(int bufferBytes, uint8_t* buffer, uint16_t* bytesRead){

  // Check if file already open
  if (!this->transmissionFile)
    return FILE_NOT_OPEN;

  // Go to the next position to read
  this->transmissionFile.seek(this->nextTransmitFilePos);

  *bytesRead = this->transmissionFile.read(buffer, bufferBytes);

  this->nextTransmitFilePos = this->transmissionFile.position();

  // Check if it's partial block
  if (*bytesRead < bufferBytes)
    return END_OF_ARCHIVED_FILE;

  // Peek 1 byte to check if EOF follows a full-sized block
  uint8_t peek;
  std::streamsize peekRead = this->transmissionFile.read(&peek, 1);

  // Check if this block is the last block in the file, if it is, then return how many bytes were read
  if (peekRead == 0)
    return END_OF_ARCHIVED_FILE;
  else 
    return SUCCESS;


  // String transFileName = 

  // if (transFileName == "")
  //   return FUNCTION_NOT_RUN;
  
  // this->transmitFileName = transFileName;
  // this->transmitLogFullPath = this->loggingRootDir + this->transmitFileName;
  
  
  // this->transmissionFile = SD.open(this->transmitLogFullPath, LOGGER_FILE_READ);
  // if (!logFile) {
  //   Serial.println("UoFlowLogger DEBUG: Failed to open file");
  //   return false;
  // }

  
}

// void UroFlowLogger::cacheTransmissionStatus(){
//   if (!this->transmissionFile)
//     return FILE_NOT_OPEN
  
//   // Store data in a status file
//   File statusFile;
//   trans_status_s fileStatus;
//   fileStatus.transmittionFileName = this->transmitFileName.c_str();
//   fileStatus.nextTransmitFilePos = this->transmissionFile.position();
//   statusFile = SD.open(this->loggingRootDir + String(STATUS_FILE_NAME), LOGGER_FILE_WRITE);
//   statusFile.close();
  
// }

bool UroFlowLogger::closeTransmissionFile(){

  if (!this->transmissionFile)
    return false;
  
  // this->updateStatusFile(this->transmitLogFullPath.c_str(), this->nextTransmitFilePos);
  this->transmissionFile.close();

  // Reset the transmit file name
  this->transmitLogFullPath = "";
  this->transmitFileName = "";
  this->nextTransmitFilePos = 0;
  
  // this->updateStatusFile(renamedTransmissionFileFullPath.c_str(), 0);
  return true;

}

String UroFlowLogger::closeAndRenameTransmissionFile(){

  if (!this->transmissionFile)
    return "";
  
  // this->updateStatusFile(this->transmitLogFullPath.c_str(), this->nextTransmitFilePos);
  this->transmissionFile.close();

  // Rename this file to indicate that it is done transmitting  

  String renamedTransmissionFileFullPath = this->loggingRootDir + TRANSMITTED_PREFIX + this->transmitFileName + String(this->currLogExt);

  // Continually updates name until it is unique
  int i = 1;
  while (SD.exists(renamedTransmissionFileFullPath)){
    i++;
    renamedTransmissionFileFullPath = this->loggingRootDir + TRANSMITTED_PREFIX + this->transmitFileName + "(" + i + ")" + String(this->currLogExt);
  }

  if (!SD.exists(this->transmitLogFullPath)){
    return "";
  }

  SD.rename(this->transmitLogFullPath, renamedTransmissionFileFullPath);

  // Reset the transmit file name
  this->transmitLogFullPath = "";
  this->transmitFileName = "";
  this->nextTransmitFilePos = 0;
  
  // this->updateStatusFile(renamedTransmissionFileFullPath.c_str(), 0);
  return renamedTransmissionFileFullPath;

}

String UroFlowLogger::findFirstWithPrefix(const char* prefix){
  File root = SD.open(this->loggingRootDir);

  File file = root.openNextFile();
  while (file) {
    if (String(file.name()).startsWith(prefix)) {
      return file.name();
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  return "";
}

// ----------------------------------------------------




UroFlowLogger::UroFlowLogger(String deviceName, int sampleRate, FileType fileTypeInp){
  this->initInternalValues();

  this->fileType = fileTypeInp;
  this->sampleRate = sampleRate;
  this->deviceName = deviceName;

  switch(this->fileType){
    case Binary:  this->currLogExt = ".bin";  break;
    case CSV:     this->currLogExt = ".csv";  break;
    default:      this->currLogExt = ".txt";   break;
  }
}

UroFlowLogger::UroFlowLogger(String deviceName, int sampleRate){
  this->initInternalValues();

  this->fileType = NoType;
  this->sampleRate = sampleRate;
  this->deviceName = deviceName;

  this->currLogExt = ".txt"; 
}

// bool UroFlowLogger::beginRTC(){
//   return rtc.begin();
// }

bool UroFlowLogger::beginSD(const char* path, spisetup_s spiSetupInp, int retries){

  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UoFlowLogger DEBUG: Path of " + String(path) + " MISO: " + String(spiSetup.MISO) + " MOSI: " + spiSetup.MOSI + " retries: " + retries);
  #endif

  this->connectionRetries = retries;

  this->spiSetup = spiSetupInp;

  this->loggingRootDir = path;

  return this->initSD(spiSetup.SCK, spiSetup.MISO, spiSetup.MOSI, spiSetup.CS, spiSetup.frequency);

  // return initSD(spiSetup.SCK, spiSetup.MISO, spiSetup.MOSI, spiSetup.CS, spiSetup.frequency, connectionRetries);

}

// NEW ONE
bool UroFlowLogger::initLogger(const char* path, spisetup_s spiSetupInp, int retries){

  this->connectionRetries = retries;

  this->spiSetup = spiSetupInp;

  this->loggingRootDir = path;

  // Check for metadate folder, if not there, open one


  return this->initSD(spiSetup.SCK, spiSetup.MISO, spiSetup.MOSI, spiSetup.CS, spiSetup.frequency);

  // return initSD(spiSetup.SCK, spiSetup.MISO, spiSetup.MOSI, spiSetup.CS, spiSetup.frequency, connectionRetries);

}

bool UroFlowLogger::setLogType(FileType fileTypeInp){
  if (logFile)    // If log file is open, then do not change the type
    return false;

  this->fileType = fileTypeInp;

  switch(this->fileType){
    case Binary:  this->currLogExt = ".bin";  break;
    case CSV:     this->currLogExt = ".csv";  break;
    default:      this->currLogExt = ".txt";   break;
  }
  return true;
}


// RTC_DS3231 UroFlowLogger::getRTC() { return rtc; }

// ===== SD Card Functions =====
bool UroFlowLogger::initSD(int SCK, int MISO, int MOSI, int CS, uint32_t frequency) {
  SPI.begin(SCK, MISO, MOSI, CS);  // SCK, MISO, MOSI, CS
  if (!SD.begin(CS, SPI, frequency)) {

    #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
    Serial.println("UoFlowLogger DEBUG: SD mount failed!");
    #endif

    return false;
  } else {

    #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
    Serial.println("UoFlowLogger DEBUG: SD mounted.");
    #endif
    return true;
  }
}

int UroFlowLogger::getNextTransmitPos(){
  return this->nextTransmitFilePos;
}

void UroFlowLogger::writeHeader(timestamp_t firstSampleEpoch){
  // Create and write the header
  fileheader_s header;

  // this->logBeginEpoch = (uint64_t)RTC::rtc_to_epoch(rtc.now());
  this->logBeginEpoch = firstSampleEpoch;
  this->lastSampleEpoch = this->logBeginEpoch;


  strcpy(header.deviceName, deviceName.c_str());
  header.timeStarted = logBeginEpoch;
  header.sampleRateHz = sampleRate;

  switch(fileType){
    case Binary:
      logFile.write((uint8_t*)&header, sizeof(fileheader_s));
      break;
    case CSV:
      logFile.println("Device Name," + String(header.deviceName));
      logFile.println("TimeStamp," + String(header.timeStarted));
      logFile.println("Sample Rate," + String(header.sampleRateHz));
       
      logFile.print("Log Duration (hh:mm:ss),");
      
      hoursPos = logFile.position();
      logFile.print("00:");

      minutesPos = logFile.position();
      logFile.print("00:");

      secondsPos = logFile.position();
      logFile.println("00:");

      logFile.println("------------------------");
      logFile.println(SAMPLE_CSV_HEADER_TEXT);
      break;
  }
  logFile.flush();
  headerEndPos = logFile.position(); 
  logFileEndPos = logFile.position();
}


bool UroFlowLogger::openNewLog(String fileName, timestamp_t firstSampleEpoch){
  if (fileType == NoType) 
    return false;
  
  this->logFileEndPos = 0;

  if(logFile) this->closeLog();
  this->currLogFileName = fileName;
  this->currLogFullPath = (String(loggingRootDir) + this->currLogFileName + this->currLogExt);
  
  if (SD.exists(this->currLogFullPath)){
    SD.remove(this->currLogFullPath);
  }

  logFile = SD.open(currLogFullPath, LOGGER_FILE_WRITE);
  if (!logFile) {

    #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
    Serial.println("UoFlowLogger DEBUG: Failed to open file");
    #endif

    return false;
  }

  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UoFlowLogger DEBUG: File opened. Size after open: " + String(logFile.size()));
  #endif

  this->writeHeader(firstSampleEpoch);

  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UoFlowLogger DEBUG: Size after writeHeader: " + String(logFile.size()));
  Serial.println("UoFlowLogger DEBUG: headerEndPos: " + String(this->headerEndPos));
  #endif

  return true;
}

bool UroFlowLogger::openCurrLog(){
  if (!logFile)
    logFile = SD.open(this->currLogFullPath, LOGGER_FILE_WRITE);
  return logFile;
}

bool UroFlowLogger::openNewLog(timestamp_t firstSampleEpoch){
  // if (fileType == NoType) 
  //   return false;

  if(logFile) this->closeLog();
  this->currLogFileName = LOG_PREFIX + String(firstSampleEpoch);
  this->currLogFullPath = (String(this->loggingRootDir) + this->currLogFileName + String(this->currLogExt));
  
  this->logBeginEpoch = firstSampleEpoch;

  logFile = SD.open(currLogFullPath, "w");

  if (!logFile) {

    #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
    Serial.println("UoFlowLogger DEBUG: Failed to open file");
    #endif

    return false;
  }

  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UoFlowLogger DEBUG: File opened. Size after open: " + String(logFile.size()));
  #endif 

  this->writeHeader(firstSampleEpoch);

  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UoFlowLogger DEBUG: Size after writeHeader: " + String(logFile.size()));
  Serial.println("UoFlowLogger DEBUG: headerEndPos: " + String(this->headerEndPos));
  #endif

  return true;
}



// bool UroFlowLogger::openNewLog() {
//   if (fileType == NoType) 
//     return false;

//   if(logFile) this->closeLog();

//   this->currLogFullPath = (String(loggingRootDir) + String(currLogPrefix) + String("_") + String(RTC::rtc_to_epoch(rtc.now())) + String(currLogExt));
  
//   logFile = SD.open(this->currLogFullPath, LOGGER_FILE_WRITE);
//   if (!logFile) {
//     Serial.println("UoFlowLogger DEBUG: Failed to open file");
//     return false;
//   }

//   this->writeHeader();

//   return true;

// }

bool UroFlowLogger::createNewLog(timestamp_t firstSampleEpoch){
  this->openNewLog(firstSampleEpoch);
  this->closeLog();
  return true;
}

bool UroFlowLogger::closeLog() {

  // Check if there is a file currently open
  if (!logFile) 
    return false;

  // Retrieve timestamp of the last sample stored (only works with binary files)
  // if (this->fileType == Binary){
  //   uint64_t lastSampleEpoch = 
  // }


  uint64_t logDuration = this->lastSampleEpoch - this->logBeginEpoch;
  // RTC::rtc_to_epoch(rtc.now()) - logBeginEpoch;
  duration_s duration;

  duration.hours   = (logDuration % 86400) / 3600;
  duration.minutes = (logDuration % 3600) / 60;
  duration.seconds = logDuration % 60;

  // Overwrite header with appropriate data
  switch(fileType){
    case Binary:
      logFile.seek(headerEndPos-(sizeof(duration_s)+sizeof(padding_t)));
      logFile.write((uint8_t*)&duration, sizeof(duration_s)); 
      break;

    case CSV:
      logFile.seek((duration.hours < 10) ? hoursPos+1 : hoursPos);
      logFile.print(duration.hours);

      logFile.seek((duration.minutes < 10) ? minutesPos+1 : minutesPos);
      logFile.print(duration.minutes);

      logFile.seek((duration.seconds < 10) ? secondsPos+1 : secondsPos);
      logFile.print(duration.seconds);
      break;
  }

  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UoFlowLogger DEBUG: File size before seek: " + String(logFile.size()));
  Serial.println("UoFlowLogger DEBUG: headerEndPos: " + String(headerEndPos));
  #endif

  // logFile.seek(this->logFileEndPos); // Jump file pointer to the end of the file before closing
  logFile.close();

  this->logFileEndPos = 0;
  this->headerEndPos = 0;
  this->hoursPos = 0;
  this->minutesPos = 0;
  this->secondsPos = 0;
  return true;
}

String UroFlowLogger::getCurrLogFullPath() { 
  return this->currLogFullPath; 
}

int UroFlowLogger::getCurrLogSize(){
  if (!this->logFile)
    return -1;
  return this->logFile.size();
}

int UroFlowLogger::getTransmitFileSize(){
  if (!this->transmissionFile)
    return -1;
  return this->transmissionFile.size();
}





String UroFlowLogger::getTransmitFileFullPath() { 
  if (!this->transmissionFile){
    return "";
  }
  return this->transmitLogFullPath; 
}

// DateTime UroFlowLogger::getRTCTime() { 
//   return rtc.now(); 
// }

timestamp_t UroFlowLogger::getLastSampleEpoch(){
  return this->lastSampleEpoch;
}

bool UroFlowLogger::writeSample(Sample sample){
  if (!logFile)
    return false;

  this->lastSampleEpoch = sample.data.timeStamp;

  switch(fileType){
    case Binary:  logFile.write((uint8_t*)&sample.data, sample.getSampleBytes());      break;
    case CSV:     logFile.println(sample.toCSVString());                                  break;
  }
  this->logFileEndPos = logFile.position();
  return true;
}

int UroFlowLogger::writeSampleBinaryBlock(uint8_t* sampleBlockBinary, int blockSize){

  if (!logFile)
    return NO_LOG_OPEN;

  if (blockSize % Sample::getSampleBytes() != 0) // if block size given is not divisible by the number of samples something went wrong
    return INVALID_BLOCK;
  
  if (this->fileType != Binary) // This function only compatible with Binary filetype
    return INVALID_FILE_TYPE;
  
  // Get the timeStamp of the last sample in binary block
  // memcpy(&(this->lastSampleEpoch), &sampleBlockBinary[blockSize-Sample::getSampleBytes()], Sample::getTimeStampBytes());
  this->lastSampleEpoch = this->getLastTimestampFromBlock(sampleBlockBinary, blockSize);

  // logFile.write(sampleBlockBinary, blockSize);
  // logFile.flush(); 

  // Write block to file
  size_t bytesWritten = logFile.write(sampleBlockBinary, blockSize);
  logFile.flush(); 

  this->logFileEndPos = logFile.position();

  #ifdef ENABLE_DEBUG_PRINT_STATEMENTS
  Serial.println("UoFlowLogger DEBUG: File size after write: " + String(logFile.size()));
  #endif

  if (bytesWritten == 0){
    return FAILED_WRITE;
  }
  
  return bytesWritten;
}


// int UroFlowLogger::readBlock(uint8_t bytesToRead, uint8_t* outputDataBlock, uint8_t* bytesRead){
//   // file.read(buffer, sizeof(buffer)) || file.gcount() > 0
// }


bool UroFlowLogger::sdConnected(){
  File root = SD.open("/");
  if (!root) return false;
  root.close(); // close it!
  return true;
}

String UroFlowLogger::getFilesInDir(const char* rootPath) {
  String fileString = "";
  int count = 0;
  File root = SD.open(rootPath);

  File file = root.openNextFile();
  while (file) {
    fileString += (String("Found file: ") + String(file.name())) + "\n";
    file.close();
    file = root.openNextFile();
  }

  return fileString;
}

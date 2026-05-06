#include "DataSample.h"


Sample::Sample(){
    this->data.timeStamp = {0};
    this->data.flow = 0;
    this->data.volume = 0;
}

String Sample::headerCSVString(){
    return SAMPLE_CSV_HEADER_TEXT;
    // String buff = "";

    // for (int i = 0; i < (NUM_ELTS-1); i++){
    //     buff += String(sampleLables[i]) + ",";
    // }
    // buff += String(sampleLabels[NUM_ELTS-1]);
    // return buff
}

// // Timestamp conversion
// time_t Sample::rtc_to_epoch(DateTime now) {

//     struct tm t;

//     t.tm_year = now.year() - 1900;
//     t.tm_mon  = now.month() - 1;
//     t.tm_mday = now.day();
//     t.tm_hour = now.hour();
//     t.tm_min  = now.minute();
//     t.tm_sec  = now.second();

//     return mktime(&t);
// }



// void setTimeStamp(DateTime time){
//     this->data.timeStamp.year = time.year();
//     this->data.timeStamp.month = time.month();
//     this->data.timeStamp.day = time.day();
//     this->data.timeStamp.hour = time.hour();
//     this->data.timeStamp.minute = time.minute();
//     this->data.timeStamp.second = time.second();
//     this->data.timeStamp.dayOfWeek = time.dayOfTheWeek();
// }

void Sample::setTimeStamp(DateTime time){
    this->data.timeStamp = (uint64_t)RTC::rtc_to_epoch(time);
}

void Sample::incrimentTimeStamp(int seoncds){
    this->data.timeStamp += seoncds;
}

const int Sample::getSampleBytes() { return sizeof(sample_s); }
const int Sample::getTimeStampBytes() { return sizeof(timestamp_t); }


String Sample::timestamp_to_string(timestamp_formatted_s t){
    char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

    String yearStr = String(t.year, DEC);
    String monthStr = (t.month < 10 ? "0" : "") + String(t.month, DEC);
    String dayStr = (t.day < 10 ? "0" : "") + String(t.day, DEC);
    String hourStr = (t.hour < 10 ? "0" : "") + String(t.hour, DEC); 
    String minuteStr = (t.minute < 10 ? "0" : "") + String(t.minute, DEC);
    String secondStr = (t.second < 10 ? "0" : "") + String(t.second, DEC);
    String dayOfWeek = daysOfTheWeek[t.dayOfWeek];

    // Complete time string
    String formattedTime = dayOfWeek + ", " + yearStr + "-" + monthStr + "-" + dayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;
    return formattedTime;
}

// const char* toCSVString() {
//     String outString = String(data.timeStamp.year) + "," + String(data.timeStamp.month) + "," + String(data.timeStamp.day) + "," + String(data.timeStamp.hour) + "," + String(data.timeStamp.minute) + "," + String(data.timeStamp.second) + "," + String(data.timeStamp.dayOfWeek) + "," + String(data.flow) + "," + String(data.volume);
//     return outString.c_str();
// }

String Sample::toCSVString() {
    return String(data.timeStamp) + "," + String(data.volume) + "," + String(data.flow);
}


// Update sample from a CSV string in the form timestamp,flow,estimatedVolume
int Sample::setFromCSVString(const String& csvString) {
    
    String elements[NUM_ELTS];
    int subStrStart = 0;
    int commaIndex = csvString.indexOf(',');
    for (int i = 0; i<NUM_ELTS; i++){
        if (commaIndex == -1) return -1;

        // Add new element from csv string to element list
        elements[i] = csvString.substring(subStrStart, commaIndex);

        // find the next comma
        subStrStart = commaIndex + 1;
        commaIndex = csvString.indexOf(',', commaIndex + 1);
    }

    // Assign elements to the struct
    // data.timeStamp.year      = (uint16_t)elements[0].toInt();
    // data.timeStamp.month     = (uint8_t) elements[1].toInt();
    // data.timeStamp.day       = (uint8_t) elements[2].toInt();
    // data.timeStamp.hour      = (uint8_t) elements[3].toInt();
    // data.timeStamp.minute    = (uint8_t) elements[4].toInt();
    // data.timeStamp.second    = (uint8_t) elements[5].toInt();
    // data.timeStamp.dayOfWeek = (uint8_t) elements[6].toInt();
    
    data.timeStamp    = elements[0].toInt();    

    data.volume             = elements[1].toFloat();           
    data.flow               = elements[2].toFloat();

    return 1;
}


// ---------------- Sample Packet ---------------------


// Construct the packet based off given MTU size
SamplePacket::SamplePacket(){
    amtFilled = 0;
}

int SamplePacket::init(int mtuSize){
    int payloadSize = mtuSize - BLE_HEADER_SIZE;

    if (payloadSize > MAX_PACKET_SIZE)  { return -1; } // Check if sampleArray bigger than initialized
    if (payloadSize < sizeof(sample_s)) { return -1; } // Check if MTU size smaller than size of one sample

    packetCapacity = (int)payloadSize / (int)sizeof(sample_s); 
    packetSize = sizeof(sample_s) * packetCapacity;

    return 1;

}


int SamplePacket::addSample(sample_s sample){
    if (amtFilled == packetCapacity) 
        return 0;

    samplePacket[amtFilled] = sample;
    amtFilled = (amtFilled + 1) % packetCapacity;
    return 1;
}

int SamplePacket::getPacketBytes() { return packetSize; }
int SamplePacket::getPacketCapacity() { return packetCapacity; }




#ifndef DATASAMPLE_H
#define DATASAMPLE_H

#include <Arduino.h>
#include <time.h>
#include "RTC.h"

#define MAX_PACKET_SIZE 247
#define MIN_MTU_SIZE 23
#define BLE_HEADER_SIZE 3


typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t dayOfWeek;
}timestamp_formatted_s;

// typedef struct __attribute__((packed)) {
//     uint64_t epoch;
// }timestamp_t;

typedef uint64_t timestamp_t;


#define NUM_TIMESTAMP_ELTS 1
#define NUM_ELTS 3

#define SAMPLE_CSV_HEADER_TEXT "Timestamp,Volume,Flow"

typedef struct __attribute__((packed)) {
        timestamp_t timeStamp;
        float    volume;
        float    flow;
} sample_s;



class Sample {
    public:
        sample_s data;

        // Initializes the data values to zero
        Sample();

        static String headerCSVString();

        static const int getSampleBytes(); 
        static const int getTimeStampBytes(); 
        
        
        // static time_t rtc_to_epoch(DateTime now); // Timestamp Conversion
        void setTimeStamp(DateTime time);
        void incrimentTimeStamp(int seoncds);
        String timestamp_to_string(timestamp_formatted_s t);

        String toCSVString();

        // Update sample from a CSV string in the form timestamp,flow,volume
        int setFromCSVString(const String& csvString);


};

class SamplePacket {
    public: 
        sample_s samplePacket[MAX_PACKET_SIZE];
        int packetSize;
        int packetCapacity;
        int amtFilled;

        // Construct the packet based off given MTU size
        SamplePacket();

        int init(int mtuSize);

        int addSample(sample_s sample);

        int getPacketBytes();
        int getPacketCapacity();

}; 

#endif // DATASAMPLE_H

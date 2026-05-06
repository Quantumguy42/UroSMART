
#ifndef DATASAMPLE_H
#define DATASAMPLE_H

#include <Arduino.h>

typedef struct sample_s {
        uint32_t timeStamp;
        float    flow;
        float    estimatedVolume;
} sample_s;

class Sample {
    public:
        sample_s data;

        const char* toCSVString() {
            String outString = String(data.timeStamp) + "," + String(data.flow) + "," + String(data.volume);
            return outString.c_str();
        }

};

// class SamplePacket {
//     public:
//         sample_s dataPacket[10];

//         SamplePacket(SamplePacket packet){
//             for (int i = 0; i < 10; i++) {
//                 dataPacket[i] = packet.dataPacket[i];
//             }
//         }

// };


#endif // DATASAMPLE_H

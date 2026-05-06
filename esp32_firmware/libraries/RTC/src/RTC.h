#ifndef RTC_H
#define RTC_H

#include <Arduino.h>
#include <time.h>
#include "RTClib.h"

// RTC_DS3231 rtc;

// char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// time_t rtc_to_epoch(DateTime now) {

//     struct tm t;

//     t.tm_year = now.year() - 1900;
//     t.tm_mon  = now.month() - 1;
//     t.tm_mday = now.day();
//     t.tm_hour = now.hour();
//     t.tm_min  = now.minute();
//     t.tm_sec  = now.second();

//     return mktime(&t);
// }

namespace RTC {
  time_t rtc_to_epoch(DateTime now);
  String printTimeStamp(DateTime date);
}


#endif // RTC_H

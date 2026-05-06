
#include "RTC.h"

namespace RTC {

  // RTC_DS3231 rtc;
  char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

  time_t rtc_to_epoch(DateTime now) {

      struct tm t;

      t.tm_year = now.year() - 1900;
      t.tm_mon  = now.month() - 1;
      t.tm_mday = now.day();
      t.tm_hour = now.hour();
      t.tm_min  = now.minute();
      t.tm_sec  = now.second();

      return mktime(&t);
  }

  String printTimeStamp(DateTime date){
    // Get the current time from the RTC
    // DateTime now = rtc.now();
    
    // Getting each time field in individual variables
    // And adding a leading zero when needed;
    String yearStr = String(date.year(), DEC);
    String monthStr = (date.month() < 10 ? "0" : "") + String(date.month(), DEC);
    String dayStr = (date.day() < 10 ? "0" : "") + String(date.day(), DEC);
    String hourStr = (date.hour() < 10 ? "0" : "") + String(date.hour(), DEC); 
    String minuteStr = (date.minute() < 10 ? "0" : "") + String(date.minute(), DEC);
    String secondStr = (date.second() < 10 ? "0" : "") + String(date.second(), DEC);
    String dayOfWeek = daysOfTheWeek[date.dayOfTheWeek()];

    // Complete time string
    String formattedTime = dayOfWeek + ", " + yearStr + "-" + monthStr + "-" + dayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;

    // Print the complete formatted time
    return formattedTime;
  }
}

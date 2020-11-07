/*
DS3231_set.pde
Eric Ayars
4/11

Test of set-time routines for a DS3231 RTC

*/

#include <DS3231.h>
#include <Wire.h>

DS3231 Clock;

byte sYear;
byte sMonth;
byte sDate;
byte sDoW;
byte sHour;
byte sMinute;
byte sSecond;

bool h12=false;
bool PM;
bool Century=false;

void setup() {
	// Start the serial port
	Serial.begin(115200);

	// Start the I2C interface
	Wire.begin(4,0);

  sYear = 17;
  sMonth = 12;
  sDate = 2;
  sDoW = 6;
  sHour = 19;
  sMinute = 0;
  sSecond = 0;
  
}

void loop() {
  Serial.println("Start"); 

  Clock.setClockMode(false);	// set to 24h
  //setClockMode(true);	// set to 12h
  
  Clock.setYear(sYear);
  Clock.setMonth(sMonth);
  Clock.setDate(sDate);
  Clock.setDoW(sDoW);
  Clock.setHour(sHour);
  Clock.setMinute(sMinute);
  Clock.setSecond(sSecond);
  
  // Test of alarm functions
  // set A1 to one minute past the time we just set the clock
  // on current day of week.
  //Clock.setA1Time(DoW, Hour, Minute+1, Second, 0x0, true, 
  //		false, false);
  // set A2 to two minutes past, on current day of month.
  //Clock.setA2Time(Date, Hour, Minute+2, 0x0, false, false, 
  //		false);
  // Turn on both alarms, with external interrupt
  //Clock.turnOnAlarm(1);
  //Clock.turnOnAlarm(2);
  
  //}

  Serial.println("Time set"); 

  while (1){
    Serial.print("\nYesr:"); 
    Serial.print(Clock.getYear(), DEC);
    Serial.print(' ');
    // then the month
    Serial.print("\nMonth:"); 
    Serial.print(Clock.getMonth(Century), DEC);
    Serial.print(' ');
    // then the date
    Serial.print("\nDay:"); 
    Serial.print(Clock.getDate(), DEC);
    Serial.print(' ');
    // and the day of the week
    Serial.print("\nDay of Week:"); 
    Serial.print(Clock.getDoW(), DEC);
    Serial.print(' ');
    // Finally the hour, minute, and second
    Serial.print("\nHour:"); 
    Serial.print(Clock.getHour(h12, PM), DEC);
    Serial.print(' ');
    Serial.print("\nMinute:"); 
    Serial.print(Clock.getMinute(), DEC);
    Serial.print(' ');
    Serial.print("\nSecond:"); 
    Serial.print(Clock.getSecond(), DEC);

    delay(1000);
    
  }




  
}

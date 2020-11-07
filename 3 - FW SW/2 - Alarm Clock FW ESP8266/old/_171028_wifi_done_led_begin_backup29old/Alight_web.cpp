#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <time.h>
#include "FastLED.h"
#include <Arduino.h>
#include "Alight_web.h"

#define DEBUG 1

#define PRINTDEBUG(STR) \
  {  \
    if (DEBUG) Serial.print(STR); \
  }

/***Commands**/
#define strResponseToSet "CT"
#define strResponseToAdd "AT"
#define strResponseToDel "DT"
#define strResponseToSyn "ST"
#define strResponseToRes "RT"
/*************/

int testWifi(void) { //20 - connected to local, 10 - not connected to local
  int c = 0;
  PRINTDEBUG("\nWaiting for Wifi to connect\nStatus:");  
  while ( c < 50 ) { //change to 50
    if (WiFi.status() == WL_CONNECTED) { return(20); } 
    delay(500);
    PRINTDEBUG(WiFi.status());    
    c++;
  }
  PRINTDEBUG("\nConnect timed out, opening AP\n");
  return(10);
} 

String special_char_fix(String input){
  //search for first % in input
  char* b;
  char* to_replace = "%00";
  char* replace_with = "X";
  unsigned long number = 0;
  
  while (1) {
    b = strchr(input.c_str(),'%');
    //found %
    if (b==NULL){ //% not found
      input.replace('þ','%'); //replacing back
      return input; //NOT FOUND
    }
    //string a begins with % and follows by two char long code
    char code[3] =  {'0','0','\n'};
    code[0] = *(b+1);
    code[1] = *(b+2);
    number = strtoul( code, nullptr, 16); //get number from hex code

    if (number == 37)
    {  number = 254; //replace character % with þ. no onw uses þ and % interferes with algorithm, 
      //so þ will be replaced back to % in the end.
    }
    *(to_replace+1)=code[0];
    *(to_replace+2)=code[1];
    *(replace_with) = html_replace[number]; //lookup character representation on the code %code
    input.replace(to_replace,replace_with);
  }
}


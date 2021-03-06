#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <ESP8266httpUpdate.h>
#include <Time.h>
#include <TimeLib.h>
#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ALLOW_INTERRUPTS 0 //to avoid flickering
#include "FastLED.h"
#include "Alight_web.h"
#include <DS3231.h>
#include <Wire.h>

#define DEBUG 1

#if DEBUG == 1
  #define PRINTDEBUG Serial.print
#else
  #define PRINTDEBUG
#endif 

/***Commands**/
#define strResponseToSet "CT"
#define strResponseToAdd "AT"
#define strResponseToDel "DT"
#define strResponseToSyn "ST"
#define strResponseToRes "RT"
/*************/

int led = HIGH;

//FastLED
#define NUM_LEDS_H 12
#define TAIL_LEDS_H 4
#define NUM_LEDS_M 24
#define SACRIFICIAL_LED 1 //0 or 1 if additinal LED for level shift is used.
#define TAIL_LEDS_M 8
#define DATA_PIN_LED_M_H 12
// Define the array of leds
CRGB leds[NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED];
#define SHOW_SECONDS 1

const byte max_bright[3] = {128,63,3};// {3,128,63};
unsigned long last_update_LED_H;
unsigned long last_update_LED_M;
byte dot_H = 0;
byte dot_M = 0;
byte br = 0; //alarm brightness begin

//ADC
int LightValue = 0;        // value read from the light sensor

#define UPDATE_LED_MS 40 //for loading animation

//WEB stuff
MDNSResponder mdns;
IPAddress ip;
String st;
String ipStr;
const int req_buffer_size = 20;
String req_value_buf[req_buffer_size];
String req_name_buf[req_buffer_size];
String ssid = "A_light";
String password = "setupA_light";
WiFiServer server(80);
bool summer_mode = false;

TimeElements tm;
unsigned long last_update_time;
DS3231 Clock;
#define RTCon
bool h12=false;
bool PM;
bool Century=false;

//time stuff
#define ALARM_STEP_BRIGHTNESS_MS 500
byte Alarm_buffer[40]; //4 items per alarm, 10 alarms available
#define ALARM_MEMORY_OFFSET 256
bool Alarm_shining = false;
#define ALARM_TIMEOUT_MS 3600000 //one hour, have to be more than 60s
unsigned long alarm_timeout_from_ms = 0;

void setup() {
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  delay(100);
  //FastLED
  FastLED.addLeds<WS2812, DATA_PIN_LED_M_H, GRB>(leds, NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED);//GBR?
  dot_H = 0;
  last_update_LED_H = millis();
  dot_M = 0;
  last_update_LED_M = millis();
  last_update_time = millis();
  
  pinMode(LED_BUILTIN, OUTPUT); //D0 or GPIO16
  pinMode(D3, INPUT); //D3 or GPIO0 is button
  digitalWrite(LED_BUILTIN, LOW); //do not interfere with light sensor

  //Enable Serial
  Serial.begin(115200);

  #ifdef RTCon
    // Start the I2C interface
    Wire.begin(4,0);
    setSyncProvider(RTC_time_read);
    setSyncInterval(30);//seconds
  #endif

  //Enable EEPROM
  EEPROM.begin(512);
  delay(10);

  PRINTDEBUG("...\n");
  PRINTDEBUG("Startup");

  // read eeprom for ssid and pass
  PRINTDEBUG("\nReading EEPROM");
  // Read SSID EEPROM
  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  PRINTDEBUG("\nSSID: "); PRINTDEBUG(esid);

  // Read password from EEPROM
  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  PRINTDEBUG("\nPASS: "); PRINTDEBUG(epass);

  PRINTDEBUG("\nAlarm Memory:");
  // Read Alarm memory to Alarm buffer from EEPROM
  for (int i = 0; i < 40; ++i)
  {
    Alarm_buffer[i] = EEPROM.read(i+ALARM_MEMORY_OFFSET);
    #if DEBUG == 1
      if ((i%4)==0) {PRINTDEBUG("\nAlarm #");PRINTDEBUG(1+((i)/4));PRINTDEBUG(":\t");}
      PRINTDEBUG(Alarm_buffer[i]);
      PRINTDEBUG(",\t");
    #endif
  }

  String chip_id = String(ESP.getFlashChipId());
  ssid = ssid + chip_id;

  byte webtype = 0; //0 local, 1 AP
  if ((esid.length() > 2)) {
    // test esid from eeprom
    WiFi.begin(esid.c_str(), epass.c_str());
    //use DHCP, no IP address is set.

    if (testWifi() == 20) { //20 connected, 10 not connected     this takes about ten seconds
      WiFi.mode(WIFI_STA);
      webtype = 0; //0 local, 1 AP
    } else {
      setupAP();
      webtype = 1; //web server on on AP
    }
  } else {
    setupAP();
    webtype = 1; //web server on on AP
  }

  launchWeb(webtype); //0 local, 1 AP
  int b = 20;
  while (b == 20) {
    b = mdns1(webtype); //will be in this function forever
    //if not return 20 program stops
    check_reset_button();
    
    if (Alarm_shining){
      if (millis()-last_update_time>ALARM_STEP_BRIGHTNESS_MS){
        check_alarm();
        alarm_shine();
        last_update_time = millis();
      }
    }else{
      br = 0;
      if (millis()-last_update_time>1000){//every second
         check_alarm(); //check alarm every second only
         last_update_time = millis();
         LightValue = analogRead(A0);
         time_show(); 
        
         //checking if WiFi connection is still active
         if ((summer_mode == 0) && (webtype == 0) && (WiFi.status() != WL_CONNECTED))  { //only for local Wifi Connection when in client mode
            LED_blink_all(1,CRGB::Orange);
            PRINTDEBUG("\nConnection lost, trying to reconnect");
            WiFi.disconnect(); //check if still connected, if lost or anything, disconnect
            delay(100);
            WiFi.begin(esid.c_str(), epass.c_str()); //use DHCP, no IP address is set.
            if (testWifi() == 20) { //20 connected, 10 not connected     this takes about ten seconds
              WiFi.mode(WIFI_STA);
            } else {
              setupAP();
              webtype = 1; //web server on on AP
              LED_blink_all(10,CRGB::Red); //error red blink 10 times
            }
         }
      }//end every second
    }
  }
  PRINTDEBUG("Error in function mdns1, program stopped");
  LED_blink_all(0,CRGB::Red);
}
//---------------------------------------------END SETUP---------------------------------------------

//---------------------------------
//--------WEB FUNCTIONS------------
//---------------------------------

void check_reset_button(){
    if (!digitalRead(D3)) //button pressed
    {
      LED_blink_all(1,CRGB::Yellow);
      program_restart();
    }
}

void program_restart(){
  led = HIGH;
  Alarm_shining = false;
  dot_H = 0;
  dot_M = 0;
  br = 0; //alarm brightness begin
  for (int i=0;i<40;i++)
    Alarm_buffer[i] = 0;
  ssid = "A_light";
  password = "setupA_light";
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  delay(100);
  setup();
}

int testWifi(void) { //20 - connected to local, 10 - not connected to localint testWifi(void) { //20 - connected to local, 10 - not connected to local
  unsigned long last_check_wifi = millis();
  last_update_LED_H = millis();
  PRINTDEBUG("\nWaiting for Wifi to connect\nStatus:");
  wl_status_t current_status;
  while (millis()-last_check_wifi<25000) { //change to 50*500ms=25s
    current_status = WiFi.status();
    //PRINTDEBUG(current_status); 
   /* WL_IDLE_STATUS      = 0,
      WL_NO_SSID_AVAIL    = 1,
      WL_SCAN_COMPLETED   = 2,
      WL_CONNECTED        = 3,
      WL_CONNECT_FAILED   = 4,
      WL_CONNECTION_LOST  = 5,
      WL_DISCONNECTED     = 6*/
    if (current_status == WL_CONNECTED) {
      PRINTDEBUG(current_status); 
      LED_blink_all(3,CRGB::Green);
      return (20);
    } else if ( (current_status==WL_NO_SSID_AVAIL)| //after while, when SSID that was saved in memory not found
                (current_status==WL_IDLE_STATUS)  | //when no SSID is in memory
                (current_status==WL_CONNECT_FAILED)){ //fail is fail
                  PRINTDEBUG(current_status); 
      break; //breaks before time is left if it is sure we will not connect
    }
    //Loading animation!!!
    if (millis()-last_update_LED_M>UPDATE_LED_MS){
      if (++dot_M==NUM_LEDS_M){ dot_M = 0;  }
      if (++dot_H==NUM_LEDS_H){ dot_H = 0;  }
      LED_loading_anim(dot_M, 0);
      LED_loading_anim(dot_H, 1);
      last_update_LED_M = millis();
    }
    delay(2);
  }
  LED_blink_all(1,CRGB::Red);
  PRINTDEBUG("\nConnect timed out, opening AP\n");
  PRINTDEBUG("password:");
  PRINTDEBUG(password);PRINTDEBUG("\n");
  return (10);
}

void launchWeb(int webtype) {
  PRINTDEBUG("\nWiFi connected : ");
  if (webtype) {
    PRINTDEBUG("\nAP SSID:");
    PRINTDEBUG(ssid);
    PRINTDEBUG("\nPASS: ");
    PRINTDEBUG(password);
    PRINTDEBUG("\nIP: ");
    ip = WiFi.softAPIP();
  } else {
    PRINTDEBUG("\nLocal WiFi, IP: ");
    ip = WiFi.localIP();
  }
  PRINTDEBUG(ip);
  ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

  if (!mdns.begin("Alight",ip)) {
    PRINTDEBUG("\nError setting up MDNS responder! deadlock");
    LED_blink_all(0,CRGB::Red);
  }
  PRINTDEBUG("\nmDNS responder started");
  // Start the server
  server.begin();
  mdns.addService("http","tcp",80);
  PRINTDEBUG("\nServer started");
  delay(100);
}

String special_char_fix(String input) {
  //search for first % in input
  char* b;
  char to_replace[4] = {'%', '0', '0', '\0'};
  String replace_with; //= "X";
  unsigned long number = 0;

  while (1) {
    b = strchr(input.c_str(), '%');
    //found %
    if (b == NULL) { //% not found
      input.replace(html_replace[254], '%'); //replacing back  //'�'
      return input; //NOT FOUND
    }
    //string a begins with % and follows by two char long code
    char code[3] =  {'0', '0', '\0'};
    code[0] = *(b + 1);
    code[1] = *(b + 2);
    number = strtoul( code, 0, 16); //get number from hex code
    if (number == 37)
    { number = 254; //replace character % with �. no onw uses � and % interferes with algorithm,
      //so � will be replaced back to % in the end.
    }
    to_replace[1] = code[0];
    to_replace[2] = code[1];
    replace_with = String(html_replace[number]); //lookup character representation on the code %code
    input.replace(String(to_replace), replace_with);
  }
}

void setupAP(void) {

  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  PRINTDEBUG("\nScan done\n");
  if (n == 0)
  {
    PRINTDEBUG("\nno networks found");
  }
  else
  {
    PRINTDEBUG(n);
    PRINTDEBUG(" networks found:\n");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      PRINTDEBUG(i + 1);
      PRINTDEBUG(": ");
      PRINTDEBUG(WiFi.SSID(i));
      PRINTDEBUG(" (");
      PRINTDEBUG(WiFi.RSSI(i));
      PRINTDEBUG(")");
      PRINTDEBUG((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " \n" : "*\n");
      delay(10);
    }
  }
  PRINTDEBUG("");
  st = "<ul>"; //string with networks for http page
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += i + 1;
    st += ": ";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ul>";
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAP(ssid.c_str(), password.c_str());
  PRINTDEBUG("SoftAp\n");
}

String buildHTTPResponse(String content) //build response so the app ESPAlarm will get it in a good form
{
  // build HTTP response
  String httpResponse;
  httpResponse  = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n";
  httpResponse += "Content-Length: ";
  httpResponse += content.length();
  httpResponse += "\r\n";
  httpResponse += "Connection: close\r\n\r\n";
  httpResponse += content;
  httpResponse += " ";
  return httpResponse;
}

void req_inter(String in) //split recevied string to command and value and put to global buffers
{
  int i = 0;
  // Read each command pair
  // Length (with one extra character for the null terminator)
  int str_len = in.length() + 1;

  // Prepare the character array (the buffer)
  char input[str_len];

  // Copy it over
  in.toCharArray(input, str_len);
  char* command = strtok(input, "&");
  while (command != 0)
  {
    // Split the command in two values
    char* separator = strchr(command, '=');
    if (separator != 0)
    {
      // Actually split the string in 2: replace ':' with 0
      *separator = 0;
      req_name_buf[i] = command; //write command to this GLOBAL buffer
      ++separator;
      req_value_buf[i++] = separator; //value to this GLOBAL buffer
    }
    // Find the next command in input string
    command = strtok(0, "&");
  }
}

void http_update_handle(String req) {
  String t = "/a?";
  req = req.substring(t.length());
  req_inter(req);
  PRINTDEBUG(req_name_buf[0]);
  PRINTDEBUG(req_value_buf[0]);
  t_httpUpdate_return ret = ESPhttpUpdate.update(req_value_buf[0]);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      PRINTDEBUG("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      PRINTDEBUG("HTTP_UPDATE_OK");
      break;
  }
}

int mdns1(int webtype) //main web function that do everything. responds to requests etc..
//if returned 20, everything is ok, other value is bad
{
  // Check for any mDNS queries and send responses
  mdns.update();
  WiFiClient client = server.available();
  if (!client) {
    return (20);
  }
  PRINTDEBUG("");
  PRINTDEBUG("\n\nNew client request: ");
  String req = client.readStringUntil('\r');
  PRINTDEBUG(req);
  // Wait for data from client to become available
  if (client.connected() && !client.available()) {
    PRINTDEBUG("\nGone");
    return (20);
  }

  // Read the first line of HTTP request

  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    Serial.print("\nInvalid request: ");
    PRINTDEBUG(req);
    return (20);
  }
  req = req.substring(addr_start + 1, addr_end);
  Serial.print("\nRequest: ");
  PRINTDEBUG(req);
  client.flush();

  String s;
  int commaIndex[6];
  String temp;
  String ipStr;

  if ( webtype == 1 ) {
    PRINTDEBUG(req);
    if (req == "/")
    {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>A_light";
      s += "<p>";
      s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><label><br><br>PASS: </label><input name='pass' length=64><label><br><br><input type='submit'><br><br>";
      s += st;
      s += "</form>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      LED_blink_all(1,CRGB::Blue);
    }
    else if ( req.startsWith("/a?ssid=") ) {
      String t = "/a?";
      req = req.substring(t.length());
      req_inter(req);
      for (int i = 0; i < 2; i++)
      {
        PRINTDEBUG("\n");
        PRINTDEBUG(req_name_buf[i]);
        PRINTDEBUG(": ");
        PRINTDEBUG(req_value_buf[i]);
      }

      String qsid = req_value_buf[0];
      String qpass = req_value_buf[1];

      if (qsid.length()<2)
      {
        PRINTDEBUG("\nSSID entered is too short! values are not changed");
        //message stating credentials saved with restart button
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Local ";
        s += "WiFi credentials NOT saved! (SSID too short)<br><br><a href=\"";
        s += "restart";
        s += "\"><button>Restart!</button></a>";
        s += "<p></p></html>\r\n\r\n";
        LED_blink_all(1,CRGB::Red);
      }else{
        qsid = special_char_fix(qsid);
        qpass = special_char_fix(qpass);
        
        PRINTDEBUG("\nclearing eeprom 0 - 127 index");
        for (unsigned int i = 0; i < 127; ++i) {
          EEPROM.write(i, 0);
        }
        PRINTDEBUG("\neeprom cleared");
        delay(5);
        PRINTDEBUG("\nwriting eeprom ssid:");
        for (byte i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          PRINTDEBUG(qsid[i]);
        }
        PRINTDEBUG("\nwriting eeprom pass:");
        for (byte i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          PRINTDEBUG(qpass[i]);
        }
        EEPROM.commit();
        //message stating credentials saved with restart button
        s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>Local WiFi credentials saved!<br><br><a href=\"";
        s += "restart";
        s += "\"><button>Restart!</button></a>";
        s += "<p></p></html>\r\n\r\n";
        LED_blink_all(2,CRGB::Green); 
      }
    }
    else if ( req.startsWith("/cleareeprom") ) {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>EEPROM cleared!<br><br><a href=\"";
      s += "restart";
      s += "\"><button>Restart!</button></a>";
      s += "<p></p></html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      PRINTDEBUG("\nclearing eeprom");
      for (int i = 0; i < 512; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      LED_blink_all(1,CRGB::Green);
    }
    else if (req.startsWith("/a?update="))
    {
      http_update_handle(req);
    }
    else if ( req.startsWith("/restart") ) {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>A_light";
      s += "<p>Restarting...<p>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      PRINTDEBUG("\nRestarting");
      client.print(s); //because it won't run in the end of this IF statements
      if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
      delay(100);
      LED_blink_all(1,CRGB::Yellow);
      program_restart();
    }
    else if ( req.startsWith("/wifioff")) {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>A_light";
      s += "<p>Wifi Turning off, no wireless in summer mode, restart for connection...<p>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      PRINTDEBUG("\nWifi off");
      client.print(s); //because it won't run in the end of this IF statements
      if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
      WiFi.mode( WIFI_OFF );
      WiFi.forceSleepBegin();
      delay(100);
      LED_blink_all(1,CRGB::Green);
      summer_mode=true;
    }
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n";
      PRINTDEBUG("\nSending 404");
      LED_blink_all(1,CRGB::Red);  //get rid of this maybe, because browser asks for an icon every load and it is error
    }
  }
  else //web type 0 is when connected to local wifi
  {
    if (req == "/")
    {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>ALight active<br>IP:";
      s += ipStr;
      s += "<p>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      LED_blink_all(1,CRGB::Blue);
    }
    else if ( req.startsWith("/cleareeprom") ) {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>EEPROM cleared!<br><br><a href=\"";
      s += "restart";
      s += "\"><button>Restart!</button></a>";
      s += "<p></p></html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      PRINTDEBUG("\nClearing eeprom");
      for (int i = 0; i < 512; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      LED_blink_all(1,CRGB::Green);
    }
    else if ( req.startsWith("/restart") ) {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>A_light";
      s += "<p>Restarting...<p>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      PRINTDEBUG("\nRestarting");
      client.print(s); //because it won't run in the end of this IF statements
      if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
      delay(100);
      LED_blink_all(1,CRGB::Yellow);
      program_restart();
    }
    else if ( req.startsWith("/wifioff")) {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>A_light";
      s += "<p>Wifi Turning off, no wireless in summer mode, restart for connection...<p>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      PRINTDEBUG("\nWifi off");
      client.print(s); //because it won't run in the end of this IF statements
      if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
      WiFi.mode( WIFI_OFF );
      WiFi.forceSleepBegin();
      delay(100);
      LED_blink_all(1,CRGB::Green);
      summer_mode=true;
    }
    else if ( req.startsWith("/time") ) {
      // getting time from time server, value returned to debug serial link only yet
      //gettime();
      LED_blink_all(1,CRGB::Purple);
    }
    else if ( req.startsWith("/a?update=") ) {
      http_update_handle(req); //idk what is going on here
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>HTTP updated<p>";
      s += String(1);
      s += "</p></html>\r\n\r\n";
    }
    //Responses to time commands
    else if ( req.startsWith("/?SYN") ) { //Sync   //Example: SYN,08+20+2016,09:28:56,1

      commaIndex[0] = req.indexOf(','); //first Index
      commaIndex[1] = req.indexOf(',',commaIndex[0]+1); //next 5 commas
      tm.Month = req.substring(commaIndex[0]+1, commaIndex[0]+3).toInt();
      temp = req.substring(commaIndex[0]+4, commaIndex[0]+6);
      tm.Day = temp.toInt();
      temp = req.substring(commaIndex[0]+9, commaIndex[0]+11);
      tm.Year = temp.toInt();
      temp = req.substring(commaIndex[1]+1, commaIndex[1]+3);
      tm.Hour = temp.toInt();
      temp = req.substring(commaIndex[1]+4, commaIndex[1]+6);
      tm.Minute = temp.toInt();
      temp = req.substring(commaIndex[1]+7, commaIndex[1]+9);
      tm.Second = temp.toInt();
      temp = req.substring(commaIndex[1]+10);
      tm.Wday = temp.toInt();

      #ifndef RTCon
        setTime(tm.Hour,tm.Minute,tm.Second,tm.Day,tm.Month,tm.Year);
      #endif
      
      #ifdef RTCon
        Clock.setClockMode(false);  // set to 24h
        Clock.setYear(tm.Year);
        Clock.setMonth(tm.Month);
        Clock.setDate(tm.Day);
        Clock.setDoW(tm.Wday);
        Clock.setHour(tm.Hour);
        Clock.setMinute(tm.Minute);
        Clock.setSecond(tm.Second);
      #endif
      
      time_show();
      
      Alarm_shining = false; //turn off alarm after syncing clock

      #if DEBUG == 1
        PRINTDEBUG("\nAlarms from buffer:");
        for (int i=0;i<40;i++){
          if ((i%4)==0) {PRINTDEBUG("\nAlarm #");PRINTDEBUG(1+((i)/4));PRINTDEBUG(":\t");}
          PRINTDEBUG(Alarm_buffer[i]);
          PRINTDEBUG(",\t");
        }
        PRINTDEBUG("\nAlarms from EEPROM:");
        for (int i=0;i<40;i++){
          if ((i%4)==0) {PRINTDEBUG("\nAlarm #");PRINTDEBUG(1+((i)/4));PRINTDEBUG(":\t");}
          PRINTDEBUG(EEPROM.read(i+ALARM_MEMORY_OFFSET));
          PRINTDEBUG(",\t");
        }
      #endif
      
      //if success
      s = buildHTTPResponse(strResponseToSyn);
      PRINTDEBUG("\nSending:");
      PRINTDEBUG(strResponseToSyn);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////ALARMS/////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    else if ( req.startsWith("/?ADD") ) { //Example:  /?ADD,2,A,0951,O,0,a      ?ADD,1,A,0825,234,0,name; HTTP/1.1  //234 means monday tuesday and wednesday repetition
      
      Alarm_shining = false; //turn off alarm after adding new or deactivating ANY alarm
      
      //find commas
      commaIndex[0] = req.indexOf(','); //first Index
      for (byte i=1;i<6;i++){
        commaIndex[i] = req.indexOf(',',commaIndex[i-1]+1); //next 5 commas
      }
      byte Alarm_ID = req.substring(commaIndex[0]+1, commaIndex[1]).toInt();  //1 to X
      String Alarm_Active = req.substring(commaIndex[1]+1, commaIndex[2]);    //A or D
      if (Alarm_Active == "A"){   
        delete_alarm(Alarm_ID); //delete previous     
        String Alarm_Repeat = req.substring(commaIndex[3]+1, commaIndex[4]); //o of nothing, numbers for others Sunday=1, to Saturday=7;
        String Alarm_Name = req.substring(commaIndex[5]+1);
        byte Alarm_Sound = req.substring(commaIndex[4]+1, commaIndex[5]).toInt(); //SETTING, alarm sound field only 0 to 2, in app as 1 to 3
        
        Alarm_buffer[(Alarm_ID-1)*4 + 0] = req.substring(commaIndex[2]+1, commaIndex[2]+3).toInt(); //HOUR
        Alarm_buffer[(Alarm_ID-1)*4 + 1] = req.substring(commaIndex[2]+3, commaIndex[3]).toInt(); //MINUTE
        if (Alarm_Repeat == "o"){
          Alarm_buffer[(Alarm_ID-1)*4 + 2] = 0; //no day checkbox was set in app
        } else { 
          if (Alarm_Repeat == "F"){ //all days selected
            Alarm_buffer[(Alarm_ID-1)*4 + 2] = 0b01111111;
          } else {
            byte rep_len = Alarm_Repeat.length();
            Alarm_buffer[(Alarm_ID-1)*4 + 2] = 0;
            for (byte rep_idx=0; rep_idx<rep_len;rep_idx++){
              byte Wday_number = Alarm_Repeat.substring(rep_idx,rep_idx+1).toInt();
              Alarm_buffer[(Alarm_ID-1)*4 + 2] |= 1<<(Wday_number-1); //shift 1 on correct date and then bitwise OR
            }
          }
        }
        Alarm_buffer[(Alarm_ID-1)*4 + 3] = Alarm_Sound+1;
        //Setting, non-zero value means alarm active

        for (int i = 0; i < 4; i++)
        {
          EEPROM.write(ALARM_MEMORY_OFFSET+((Alarm_ID-1)*4)+i, Alarm_buffer[(Alarm_ID-1)*4 + i]);
        }
        EEPROM.commit();
        delay(100);
        PRINTDEBUG("\nAlarm written in EEPROM");
        
        PRINTDEBUG("\nAlarm #");
        PRINTDEBUG(Alarm_ID);
        PRINTDEBUG(" written (Alarm Buffer)");
        PRINTDEBUG("\n\tHour: ");
        PRINTDEBUG(Alarm_buffer[(Alarm_ID-1)*4 + 0]);
        PRINTDEBUG("\n\tMinute: ");
        PRINTDEBUG(Alarm_buffer[(Alarm_ID-1)*4 + 1]);
        PRINTDEBUG("\n\tRepetition: ");
        PRINTDEBUG(Alarm_buffer[(Alarm_ID-1)*4 + 2]);
        PRINTDEBUG("\n\tRepetition BIN: ");
        PRINTDEBUG(Alarm_buffer[(Alarm_ID-1)*4 + 2],BIN);
        PRINTDEBUG("\n\tSetting: ");
        PRINTDEBUG(Alarm_buffer[(Alarm_ID-1)*4 + 3]);
        
      }else{
        PRINTDEBUG("\nNot active - deleting alarm ");
        delete_alarm(Alarm_ID);
      }
      

      
      String responseAdd_w_ID = strResponseToAdd;
      responseAdd_w_ID += Alarm_ID;//Alarm_ID;
      s = buildHTTPResponse(responseAdd_w_ID);
      PRINTDEBUG("\nSending:");
      PRINTDEBUG(responseAdd_w_ID);
      if (Alarm_Active == "A"){
        LED_blink_all(Alarm_ID,CRGB::Green);
      }else{
        LED_blink_all(Alarm_ID,CRGB::Orange);
      }
    }
    else if (req.startsWith("/?DEL") ) { //Example: /?DEL,2 HTTP/1.1
      commaIndex[0] = req.indexOf(','); //first Index
      byte del_ID   = req.substring(commaIndex[0]+1, commaIndex[0]+3).toInt();

      delete_alarm(del_ID);

      Alarm_shining = false; //turn off alarm after deleting ANY alarm
      
      //if success deleting
      s = buildHTTPResponse(strResponseToDel);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToDel);
      LED_blink_all(1,CRGB::Orange);
    }
    else if (req.startsWith("/?SET") ) { //Sync   //Example: /?SET,5,Name+SecondWord; HTTP/1.1
      //if success
      s = buildHTTPResponse(strResponseToSet);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToSet);
      LED_blink_all(1,CRGB::Green);
    }
    else if (req.startsWith("/?RES") ) { //Reset memory
      //if success
      s = buildHTTPResponse(strResponseToRes);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToRes);
      LED_blink_all(1,CRGB::Green);
    }
    else if (req.startsWith("/?DEB") ) { //Debugging screen
      //if success
      s = buildHTTPResponse("Debugging will be here");
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToRes);
      LED_blink_all(1,CRGB::Green);
    }
    else
    {
      s = buildHTTPResponse("ERR");
      PRINTDEBUG("\nSending ERR");
      LED_blink_all(1,CRGB::Red);
    }
  }
  client.print(s);
  PRINTDEBUG("\nDone with client");
  return (20);
}

////////////////////////////////
//////LED strip functions///////
////////////////////////////////

void LED_loading_anim(byte dot, byte strip_num) {
  byte dot_loopback = dot;
  byte tail_leds = (strip_num ? TAIL_LEDS_H : TAIL_LEDS_M);
  byte num_leds = (strip_num ? NUM_LEDS_H : NUM_LEDS_M);
  byte led_begin = (strip_num ? NUM_LEDS_M + SACRIFICIAL_LED : SACRIFICIAL_LED);
  //start at LED1 for minutes and at LED 25 for hours

  for (byte t = 0; t <= (tail_leds); t++) { //set max brightness gradually decreasing, not last one, that have to be 0, computing takes 180 us
    if (t > dot) {
      dot_loopback = num_leds + dot; //t-dot gives how much off scale (negative it would be), go from end instead
      //NUM_LEDS_H-(t-dot) addresses correct led. -t will be subtracted, so NUM+dot
    }
    if (t == tail_leds) {
      leds[led_begin + dot_loopback - t ].setRGB(0, 0, 0); //turn off the last one so it keeps off after the TAIL goes away
    } else {
      leds[led_begin + dot_loopback - t ].setRGB(max_bright[0] / (3 * t + 1), max_bright[1] / (3 * t + 1), max_bright[2] / (3 * t + 1));
    }
  }
  leds[0].setRGB(0,0,0);
  FastLED.show();
}

void LED_blink_all(byte count_blinks, struct CRGB TheColor) { //0 means forever
  byte blinks = 0;
  while (1) { //forever when zero
    fill_solid( leds, NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED, TheColor);
    leds[0].setRGB(0,0,0);
    FastLED.show();
    delay(100); //adjustbred
    FastLED.clear ();
    FastLED.show();
    delay(200);
    if (count_blinks > 0) { //should be exiting some time
      blinks++;
      if (blinks == count_blinks) return;
    } else //else forever, else is only when count_blinks is 0
    {
      check_reset_button();
    }//else forever, else is only when count_blinks is 0
  }
}

void time_show(){
  byte bright_on = (LightValue > 220) ? 1 : 0;  //th was 190
  byte hour12h;
  hour12h = hourFormat12();
  hour12h = (hour12h==12) ? 0 : hour12h; //noon or midnight is zero
  bool hour_array[12];
  bool minute_array[24];
  bool second_array[24];
  byte minute_show = minute();
  byte second_show = second();
  
  for(byte l=0;l<12;l++){
    if (hour()<=12) //in the morning example: 4:30AM, leds 1,2,3,4 are ON
      hour_array[l] = !((hour12h+1)<=l); //true is shine
    else            //in the afternoon, example: 11:30PM, only led at 11 position is ON
                    //to limit light intensity in the night
      hour_array[l] = ((hour12h)==l); //true is shine
      
    if (hour()==12) //comparing 24h time
      hour_array[0]=true; //all at noon
    else
      hour_array[0]=false;
  }
  for(byte l=0;l<24;l++){
    minute_array[l] = !((minute_show/2.5)<=l);
    if (SHOW_SECONDS)
      second_array[l] = !((second_show/2.5)<=l);
    else
      second_array[l] = false;    
  }
  for(byte l=0;l<12;l++){
    leds[l+NUM_LEDS_M + SACRIFICIAL_LED].setRGB(0,(1+(bright_on*128))*(int)hour_array[l],0);
  }
  for(byte l=0;l<24;l++){
    leds[l+SACRIFICIAL_LED].setRGB((4+(bright_on*96))*(int)minute_array[l],(3+(bright_on*96))*(int)minute_array[l],(3+(bright_on*8))*(int)second_array[l]);
  }
  leds[0].setRGB(0,0,0);
  FastLED.show();
}

//////////////////////////////////////////////////////////////////
///////////TIME functions ////////////////////////////////////////
//////////////////////////////////////////////////////////////////

void alarm_shine(){
  if (br<255) {
      br++; //rise to full intensity
  }
  fill_solid( leds, NUM_LEDS_H + NUM_LEDS_M + SACRIFICIAL_LED, CRGB(br,br,br));
  leds[0].setRGB(0,0,0);
  FastLED.show();
  //PRINTDEBUG("\nBright:");
  //PRINTDEBUG(br);
}

#ifdef RTCon
time_t RTC_time_read(){
  tm.Year = Clock.getYear()+30; //offset from 1970
  tm.Month = Clock.getMonth(Century);
  tm.Day = Clock.getDate();
  tm.Wday = Clock.getDoW(); //1-7 Sunday to Saturday
  tm.Hour = Clock.getHour(h12,PM); //24h = false, AM/PM thing for 12h does not matter
  tm.Minute = Clock.getMinute();
  tm.Second = Clock.getSecond();
  time_t tRTC = makeTime(tm);
  if (tRTC < 1512330480) //rudimentary check if time is correct, not older than this code
    return 0;
  else
    return tRTC;
}
#endif

bool check_alarm(){ //need to be run at least every 60 seconds (time of alarm have to match exactly on hour,minute and Wday)
  if (Alarm_shining == true){
    //PRINTDEBUG("\nAlarm timeout in: \t");PRINTDEBUG((ALARM_TIMEOUT_MS - (millis() - alarm_timeout_from_ms))/1000);PRINTDEBUG(" s");
    if (millis() - alarm_timeout_from_ms > ALARM_TIMEOUT_MS){
      Alarm_shining = false; //timeout happened
      LED_blink_all(20,CRGB::Blue);
      return false;
    }
    return true; //alarm is already ON
  }
  breakTime(now(),tm);
  for (byte al_idx=0;al_idx<10;al_idx++){
    if (Alarm_buffer[(al_idx*4)+3] != 0){
      if (  (  (Alarm_buffer[(al_idx*4)+2])  &  (1<<(tm.Wday-1))  )||(Alarm_buffer[(al_idx*4)+2]==0) ){ 
                //shift 1 to correct position, then bitwise AND.. normal OR zero in alarm wday (no day ticked)
        //today IS an alarm weekday OR weekdays are not set
        //is the alarm time?
        if ((tm.Hour == Alarm_buffer[(al_idx*4)+0]) && (tm.Minute == Alarm_buffer[(al_idx*4)+1])){
          Alarm_shining = true; //so it will not go off after THE minute passes
          alarm_timeout_from_ms = millis()-(1000*tm.Second);//time when alarm was activated (for timeout)
          return true;
        }
      }
    }
  }
  return false;
}

void delete_alarm(byte Alarm_ID){
  for (byte del_idx=0;del_idx<4;del_idx++){
    Alarm_buffer[(Alarm_ID-1)*4 + del_idx] = 0;
    EEPROM.write(ALARM_MEMORY_OFFSET+((Alarm_ID-1)*4)+del_idx, 0);
  }
  EEPROM.commit();
}

////////////////////////////////////////////////////
///////////////////DEBUG////////////////////////////
////////////////////////////////////////////////////
#if DEBUG == 1

#endif

void loop() {
  //nothing
}

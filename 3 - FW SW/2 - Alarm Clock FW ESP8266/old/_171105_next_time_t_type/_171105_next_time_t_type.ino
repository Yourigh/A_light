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

int led = HIGH;

//FastLED
#define NUM_LEDS_H 12
#define TAIL_LEDS_H 4
#define NUM_LEDS_M 24
#define SACRIFICIAL_LED 1 //0 or 1 if additinal LED for level shift is used.
#define TAIL_LEDS_M 8
#define DATA_PIN_LED_H 2
#define DATA_PIN_LED_M 12
// Define the array of leds
CRGB leds_h[NUM_LEDS_H];
CRGB leds_m[NUM_LEDS_M];
#define SHOW_SECONDS;

const byte max_bright[3] = {128,63,3};// {3,128,63};
unsigned long last_update_LED_H;
unsigned long last_update_LED_M;
byte dot_H = 0;
byte dot_M = 0;
byte br = 0; //alarm brightness begin

#define UPDATE_LED_MS 100 //for loading animation

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

//time stuff
time_t Alarm[10] = {0,0,0,0,0,0,0,0,0,0};
TimeElements tm;
unsigned long last_update_time;

void setup() {
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  delay(100);
  //FastLED
  FastLED.addLeds<WS2812, DATA_PIN_LED_H, GRB>(leds_h, NUM_LEDS_H);
  FastLED.addLeds<WS2812, DATA_PIN_LED_M, GRB>(leds_m, NUM_LEDS_M);//GBR?
  dot_H = 0;
  last_update_LED_H = millis();
  dot_M = 0;
  last_update_LED_M = millis();
  last_update_time = millis();
  
  pinMode(LED_BUILTIN, OUTPUT); //D0 or GPIO16
  pinMode(D3, INPUT); //D3 or GPIO0 is button

  //Enable Serial
  Serial.begin(9600);

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
    if ((Alarm[0]!=0)&&(now()>Alarm[0])){
      /*
      PRINTDEBUG("\nAlarm  \t");
      PRINTDEBUG(Alarm[0]);
      PRINTDEBUG("\nGreater\t");
      PRINTDEBUG(now());*/
      alarm_shine();
    }else{
      br = 0;
      if (millis()-last_update_time>1000){ time_show(); }//every second
    }
  }
  PRINTDEBUG("Error in function mdns1, program stopped");
  LED_blink_all(0,CRGB::Red,0);
}
//---------------------------------------------END SETUP---------------------------------------------

//---------------------------------
//--------WEB FUNCTIONS------------
//---------------------------------

void check_reset_button(){
     if (!digitalRead(D3)) //button pressed
    {
      LED_blink_all(1,CRGB::Yellow,0);
      setup();
    }
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
      LED_blink_all(3,CRGB::Green,0);
      return (20);
    } else if ( (current_status==WL_NO_SSID_AVAIL)| //after while, when SSID that was saved in memory not found
                (current_status==WL_IDLE_STATUS)  | //when no SSID is in memory
                (current_status==WL_CONNECT_FAILED)){ //fail is fail
                  PRINTDEBUG(current_status); 
      break; //breaks before time is left if it is sure we will not connect
    }
    //Loading animation!!!
    if (millis()-last_update_LED_H>UPDATE_LED_MS){
      if (++dot_H==NUM_LEDS_H){ dot_H = 0;  }
      LED_loading_anim(dot_H,0);
      last_update_LED_H = millis();
    }
    delay(2);
  }
  LED_blink_all(1,CRGB::Red,0);
  PRINTDEBUG("\nConnect timed out, opening AP\n");
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
    LED_blink_all(0,CRGB::Red,0);
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
    PRINTDEBUG("networks found:\n");
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
      LED_blink_all(1,CRGB::Blue,0);
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
        LED_blink_all(1,CRGB::Red,0);
      }else{
        qsid = special_char_fix(qsid);
        qpass = special_char_fix(qpass);
        
        PRINTDEBUG("\nclearing eeprom");
        for (unsigned int i = 0; i < 512; ++i) {
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
        LED_blink_all(2,CRGB::Green,0); 
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
      LED_blink_all(1,CRGB::Green,0);
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
      LED_blink_all(1,CRGB::Yellow,0);
      setup();
    }
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n";
      PRINTDEBUG("\nSending 404");
      LED_blink_all(1,CRGB::Red,0);  //get rid of this maybe, because browser asks for an icon every load and it is error
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
      LED_blink_all(1,CRGB::Blue,0);
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
      LED_blink_all(1,CRGB::Green,0);
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
      LED_blink_all(1,CRGB::Yellow,0);
      setup();
    }
    else if ( req.startsWith("/time") ) {
      // getting time from time server, value returned to debug serial link only yet
      //gettime();
      LED_blink_all(1,CRGB::Purple,0);
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

      setTime(tm.Hour,tm.Minute,tm.Second,tm.Day,tm.Month,tm.Year);
      
      time_show();
      
      //if success
      s = buildHTTPResponse(strResponseToSyn);
      PRINTDEBUG("\nSending:");
      PRINTDEBUG(strResponseToSyn);
    }
    else if ( req.startsWith("/?ADD") ) { //Example:  /?ADD,2,A,0951,O,0,a      ?ADD,1,A,0825,234,0,name; HTTP/1.1  //234 means monday tuesday and wednesday repetition
      //if success
      
      //find commas
      commaIndex[0] = req.indexOf(','); //first Index
      for (byte i=1;i<6;i++){
        commaIndex[i] = req.indexOf(',',commaIndex[i-1]+1); //next 5 commas
      }
      byte Alarm_ID = req.substring(commaIndex[0]+1, commaIndex[1]).toInt();
      time_t t = now();
      breakTime(t,tm);
      bool addDay;
      addDay =  (tm.Hour > 5)?true:false;//time is not between midnight and 5AM. (in this time you do not want to add day, cause you are setting alarm for the same day)
      tm.Hour = req.substring(commaIndex[2]+1, commaIndex[2]+3).toInt();
      tm.Minute = req.substring(commaIndex[2]+3, commaIndex[3]).toInt();
      tm.Second = 0;
      
      String Alarm_Active = req.substring(commaIndex[1]+1, commaIndex[2]);
      String Alarm_Repeat = req.substring(commaIndex[3]+1, commaIndex[4]);
      String Alarm_Sound = req.substring(commaIndex[4]+1, commaIndex[5]);
      String Alarm_Name = req.substring(commaIndex[5]+1);
            
      PRINTDEBUG("\nParsing data:");
      PRINTDEBUG("\nID:");
      PRINTDEBUG(Alarm_ID);
      PRINTDEBUG("\nActive:");
      PRINTDEBUG(Alarm_Active);
      PRINTDEBUG("\nTime:");
      PRINTDEBUG(tm.Hour);
      PRINTDEBUG(":");
      PRINTDEBUG(tm.Minute);
      PRINTDEBUG("\nRepeat:");
      PRINTDEBUG(Alarm_Repeat);
      PRINTDEBUG("\nSound:");
      PRINTDEBUG(Alarm_Sound);
      PRINTDEBUG("\nName:");
      PRINTDEBUG(Alarm_Name);
      PRINTDEBUG("\nYear:");
      PRINTDEBUG(tm.Year-30);



      if (Alarm_Active == "A"){
        PRINTDEBUG("\nIs active, saving: \t");
        Alarm[Alarm_ID-1] = makeTime(tm);//type time_t //first alarm ID is sent as 1, array starts at 0
        PRINTDEBUG(Alarm[Alarm_ID-1]);
        PRINTDEBUG("\nCurrent time:      \t");
        PRINTDEBUG(now());
      }else{
        PRINTDEBUG("\nNot active - deleting");
        Alarm[Alarm_ID-1] = 0;      
      }
      
      if (addDay){ //after 5AM, take it that you are setting the next day alarm, TEMPORARY
        Alarm[Alarm_ID-1] += 86400; //add 24 hours thus 86400 seconds
      }
      
      String responseAdd_w_ID = strResponseToAdd;
      responseAdd_w_ID += Alarm_ID;//Alarm_ID;
      s = buildHTTPResponse(responseAdd_w_ID);
      PRINTDEBUG("\nSending:");
      PRINTDEBUG(responseAdd_w_ID);
      LED_blink_all(Alarm_ID,CRGB::Green,0);
    }
    else if (req.startsWith("/?DEL") ) { //Example: /?DEL,2 HTTP/1.1
      commaIndex[0] = req.indexOf(','); //first Index
      byte del_ID   = req.substring(commaIndex[0]+1, commaIndex[0]+3).toInt();

      Alarm[del_ID-1] = 0;
      
      //if success
      s = buildHTTPResponse(strResponseToDel);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToDel);
      LED_blink_all(1,CRGB::Green,0);
    }
    else if (req.startsWith("/?SET") ) { //Sync   //Example: /?SET,5,Name+SecondWord; HTTP/1.1
      //if success
      s = buildHTTPResponse(strResponseToSet);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToSet);
      LED_blink_all(1,CRGB::Green,0);
    }
    else if (req.startsWith("/?RES") ) { //Reset memory
      //if success
      s = buildHTTPResponse(strResponseToRes);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToRes);
      LED_blink_all(1,CRGB::Green,0);
    }
    else if (req.startsWith("/?DEB") ) { //Debugging screen
      //if success
      s = buildHTTPResponse("Debugging will be here");
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToRes);
      LED_blink_all(1,CRGB::Green,0);
    }
    else
    {
      s = buildHTTPResponse("ERR");
      PRINTDEBUG("\nSending ERR");
      LED_blink_all(1,CRGB::Red,0);
    }
  }
  client.print(s);
  PRINTDEBUG("\nDone with client");
  return (20);
}

////////////////////////////////
//////LED strip functions///////
////////////////////////////////

void LED_loading_anim(byte dot,byte strip_num){
    byte dot_loopback = dot;
    byte tail_leds = (strip_num ? TAIL_LEDS_H : TAIL_LEDS_M);
    byte num_leds = (strip_num ? NUM_LEDS_H : NUM_LEDS_M);
    
    for(byte t=0;t<=(tail_leds);t++){ //set max brightness gradually decreasing, not last one, that have to be 0, computing takes 180 us
      if (t>dot) {
        dot_loopback = num_leds + dot; //t-dot gives how much off scale (negative it would be), go from end instead 
        //NUM_LEDS_H-(t-dot) addresses correct led. -t will be subtracted, so NUM+dot
      }
      if (t==tail_leds) {
        if (strip_num)
          leds_h[dot_loopback-t].setRGB(0,0,0); //turn off the last one so it keeps off after the TAIL goes away
        else
          leds_m[dot_loopback-t].setRGB(0,0,0); //turn off the last one so it keeps off after the TAIL goes away
      }else{    //for all tail leds
        //first full, next 4 each 42 less
        if (strip_num)
          leds_h[dot_loopback-t].setRGB(max_bright[0]/(3*t+1),max_bright[1]/(3*t+1),max_bright[2]/(3*t+1));
        else
          leds_m[dot_loopback-t].setRGB(max_bright[0]/(3*t+1),max_bright[1]/(3*t+1),max_bright[2]/(3*t+1));
      }
    }
    FastLED.show();
}

void LED_blink_all(byte count_blinks, struct CRGB TheColor,byte strip_num){//0 means forever
  byte blinks=0;
  while(1){ //forever when zero
    if (strip_num) { fill_solid( leds_h, NUM_LEDS_H, TheColor); }
    else {          fill_solid( leds_m, NUM_LEDS_M, TheColor); }
    FastLED.show();
    delay(100);
    FastLED.clear ();
    FastLED.show();
    delay(200);
    if (count_blinks>0){//should be exiting some time
      blinks++;
      if (blinks==count_blinks) return;
    }else //else forever, else is only when count_blinks is 0
    {
      check_reset_button();
    }//else forever, else is only when count_blinks is 0
  }
}

void time_show(){
  byte hour12h;
  hour12h = hourFormat12();
  //hour12h = (hour24>11 ? hour24-12 : hour24); //12 hour time conversion
  bool hour_array[12];
  bool minute_array[24];
  bool second_array[24];
  byte minute_show = minute();
  byte second_show = second();
  
  for(byte l=0;l<12;l++){
    hour_array[l] = !(hour12h<=l); //true is shine
  }
  for(byte l=0;l<24;l++){
    minute_array[l] = !((minute_show/2.5)<=l);
    #ifdef SHOW_SECONDS
      second_array[l] = !((second_show/2.5)<=l);
    #endif
  }
  for(byte l=0;l<12;l++){
    leds_h[l].setRGB(0*(int)hour_array[l],2*(int)hour_array[l],0);
  }
  for(byte l=0;l<24;l++){
    leds_m[l].setRGB(1*(int)second_array[l],2*(int)minute_array[l],0);
  }
  FastLED.show();
}

//////////////////////////////////////////////////////////////////
///////////TIME functions ////////////////////////////////////////
//////////////////////////////////////////////////////////////////

void alarm_shine(){
  static byte br_div = 0;
  if (br<255) {
    if (br_div<10){ //can be parameter of this sort, now cca 300 seconds to full
      br_div++;
    }else{
      br_div = 0;
      br++; //rise to full intensity
    }
  }
  fill_solid( leds_m, NUM_LEDS_M, CRGB(br,br,br));
  fill_solid( leds_h, NUM_LEDS_H, CRGB(br,br,br));
  FastLED.show();
  delay(100);
  PRINTDEBUG("\nBright:");
  PRINTDEBUG(br);
}

void loop() {
  //nothing
}

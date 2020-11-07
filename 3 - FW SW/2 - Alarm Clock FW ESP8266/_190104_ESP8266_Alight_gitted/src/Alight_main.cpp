#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ESP8266httpUpdate.h>
#include <Time.h>
#include <TimeLib.h>
#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ALLOW_INTERRUPTS 0 //to avoid flickering
#include "FastLED.h"
//#define RTC_DS3231
#define RTC_DS1307
#ifdef RTC_DS3231
  #include <DS3231.h>
#endif
#ifdef RTC_DS1307
  #include <RtcDS1307.h>
#endif
#include <Wire.h>
#include "JR_FastLed_alarm_clock.h"
#include <SparkFun_MMA8452Q.h> // Includes the SFE_MMA8452Q library
#include "i2s.h"
#include "i2s_reg.h"
#include <PubSubClient.h> //https://iotdesignpro.com/projects/how-to-connect-esp8266-with-mqtt

#define VERSION "V1.2"
//V1.0 200205 added more delays to ext trigger
//V1.1 added clock show yellow while in alarm shine mode
//V1.2 adding MQTT


// Define the array of leds
CRGB leds[NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED];

//accelerometer
MMA8452Q accel;

byte br = 0; //alarm brightness begin
//ADC
uint16_t LightValue = 0;        // value read from the light sensor


//WEB stuff
String chip_id;
MDNSResponder mdns;
IPAddress ip;
const int req_buffer_size = 20;
String req_value_buf[req_buffer_size];
String req_name_buf[req_buffer_size];

WiFiServer server(80);

WiFiClient espClient;
PubSubClient client(espClient);
const char* mqttServer = "192.168.1.12";
const int mqttPort = 1883;
const char* mqttUser = "homeassistant";
const char* mqttPassword = "quoh2CheiWah1akire8ehee0Pegik0Shietu7rePhojaetoh1shephaegeiPheaz";

bool summer_mode = false;

TimeElements tm;
unsigned long last_update_time;
#ifdef RTC_DS3231
  DS3231 Clock;
#endif
#ifdef RTC_DS1307
  RtcDS1307<TwoWire> Rtc(Wire);
#endif

bool h12=false;
bool PM;
bool Century=false;

//time stuff
#define ALARM_STEP_BRIGHTNESS_MS 1000
byte Alarm_buffer[40]; //4 items per alarm, 10 alarms available
#define ALARM_MEMORY_OFFSET 256
bool Alarm_shining = false;
#define ALARM_TIMEOUT_MS 3600000 // from start of alarm, one hour, has to be more than 60s
unsigned long alarm_timeout_from_ms = 0;



//FUNCTION PROTOTYPES
void check_reset_button();
void program_restart();
void time_show(byte rot,byte status_led); //rotation and status led (middle one) - 0xRGB full intensity
void alarm_shine();

void launchWeb(int webtype); //webtype 1 = AP host, 0 = guest



void req_inter(String in); //split recevied string to command and value and put to global buffers
void http_update_handle(String req);
int mdns1(int webtype, String WifiList);//main web function that do everything. responds to requests etc..
byte getOrientation(void);
byte clock_face_rotation = 0;

#if defined(RTC_DS1307) || defined(RTC_DS3231)
  time_t RTC_time_read();
#endif
bool check_alarm();
void delete_alarm(byte Alarm_ID);
void MQTTcallback(char* topic, byte* payload, unsigned int length);

void setup() {
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  delay(100);
  chip_id = String(ESP.getFlashChipId()); //get Chip ID, used for AP SSID name
  //FastLED
  FastLED.addLeds<WS2812, DATA_PIN_LED_M_H, GRB>(leds, NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED);//GBR?

  last_update_time = millis(); //for clock second tick
  
  ESP_on_board_LED_off(); //turn off on board LED so it does not interfere with Light sensor

  #ifdef DEBUG
    //Enable Serial TX only, RX is used for i2s DAC
    Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  #endif 
  
  // Start the I2C interface
  Wire.begin(4,0);
  #ifdef RTC_DS1307
    Rtc.Begin();
    Rtc.SetSquareWavePin(DS1307SquareWaveOut_High); 
  #endif
  #if defined(RTC_DS1307) || defined(RTC_DS3231)
    setSyncProvider(RTC_time_read);
    setSyncInterval(30);//seconds
  #endif

  //Enable EEPROM
  EEPROM.begin(512);
  delay(10);

  PRINTDEBUG(VERSION);
  PRINTDEBUG("...\nStartup");

  if(accel.init(SCALE_4G,ODR_400)==1)
    PRINTDEBUG("\nInit accelerometer OK!\n");
  else{
    LED_blink_all(leds,2,CRGB::Red);
    PRINTDEBUG("\nInit accelerometer NOK!\n");
  }
  clock_face_rotation = getOrientation();

  //for i2s sound generator
  i2s_begin();
  i2s_set_rate(22050);
  ESP_on_board_LED_off(); 

  //Output interface - external trigger for alarm
  pinMode(16,OUTPUT);
  pinMode(14,OUTPUT);
  pinMode(5,OUTPUT);
  pinMode(15,OUTPUT);
  digitalWrite(16,0);
  digitalWrite(14,0);
  digitalWrite(5,0);
  digitalWrite(15,0);
  //now flipped at sync - will be used in alarm firing.

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

  byte webtype = 0; //0 local, 1 AP
  String WifiList;
  if ((esid.length() > 2)) {
    // test esid from eeprom if more than 2 characters
    WiFi.begin(esid.c_str(), epass.c_str());
    //use DHCP, no IP address is set.

    if (testWifi(leds) == 20) { //20 connected, 10 not connected     this takes about ten seconds
      WiFi.mode(WIFI_STA);
      webtype = 0; //0 local, 1 AP
    } else {
      WifiList = scanWifi_list();
      setupAP(chip_id);
      webtype = 1; //web server on on AP
    }
  } else { //esid from EEPROM less than 2 chars..
    WifiList = scanWifi_list();
    setupAP(chip_id);
    webtype = 1; //web server on on AP
  }

  launchWeb(webtype); //0 local, 1 AP

  if(webtype == 0){
    client.setServer(mqttServer, mqttPort);
    client.setCallback(MQTTcallback);
    uint8_t mqtt_connectcount = 0;
    while (!client.connected()) {
      Serial.println("\nConnecting to MQTT...");
      if (client.connect("A-light", mqttUser, mqttPassword )) {
        Serial.println("connected"); 
      } else {
        mqtt_connectcount++;
        Serial.print("failed with state ");
        Serial.print(client.state());
        delay(100);
        if (mqtt_connectcount == 20) break;
      }
    }
  }
  
  int b = 20;
  while (b == 20) {
    b = mdns1(webtype,WifiList); //this function must run periodically always
    //if not return OK_VAL program stops
    check_reset_button();
    
    if (Alarm_shining){
      if (millis()-last_update_time>ALARM_STEP_BRIGHTNESS_MS){
        check_alarm();
        alarm_shine();
        last_update_time = millis();
      }
      if (accel.available()){
          if (accel.readTap()){
            //double tap event
            leds[0].setRGB(0,0,255);
            FastLED.show();
            last_update_time = millis(); //shine for ALARM_STEP_BRIGHTNESS_MS
            Alarm_shining = false;
          }
        }
    }else{ //alarm not shining
      if (accel.available()){
          if (accel.readTap()){
            //double tap event - turn on status LED
            leds[0].setRGB(0,255,0);
            FastLED.show();
            last_update_time = millis(); //shine 1 second
            while (millis()-last_update_time<2000)
            {//play for 2 seconds
              write_audio_sample(); 
            }
          }
        }
      
      br = 0;
      if (millis()-last_update_time>1000){//every second
         PRINTDEBUG(".");
         check_alarm(); //check alarm every second only
         last_update_time = millis();

         LightValue = exp_moving_average(LightValue,analogRead(A0)); //calculate average range 0 to 1024
         //clock face orientation read
         clock_face_rotation = getOrientation();  //can be less often
         if (webtype)
          time_show(clock_face_rotation,(byte)0b100); //red status LED when AP mode (lost WiFi connection)
         else
          time_show(clock_face_rotation,(byte)0b000); 
        
         //checking if WiFi connection is still active
         if ((summer_mode == 0) && (webtype == 0) && (WiFi.status() != WL_CONNECTED))  { //only for local Wifi Connection when in client mode
            LED_blink_all(leds,1,CRGB::Orange);
            PRINTDEBUG("\nConnection lost, trying to reconnect");
            WiFi.disconnect(); //check if still connected, if lost or anything, disconnect
            delay(1000);
            WiFi.begin(esid.c_str(), epass.c_str()); //use DHCP, no IP address is set.
            if (testWifi(leds) == OK_VAL) { //20 connected, 10 not connected     this takes about ten seconds
              WiFi.mode(WIFI_STA);
            } else { //ERROR_VAL
              WifiList = scanWifi_list();
              setupAP(chip_id);
              webtype = 1; //web server on on AP
              LED_blink_all(leds,10,CRGB::Red); //error red blink 10 times
            }
         }
      }//end every second
    }
  }
  PRINTDEBUG("Error in function mdns1, program stopped");
  LED_blink_all(leds,20,CRGB::Red);
  program_restart();
}
//---------------------------------------------END SETUP---------------------------------------------

void time_show(byte rot, byte status_led){
  float bright_on = LightValue / 1024.0; //0 to 1 range
  //PRINTDEBUG("L=");
  //PRINTDEBUG(bright_on);
  //PRINTDEBUG("\n");
  if (bright_on <= 0.02) bright_on = 0.00;
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
  byte lr;
  for(byte l=0;l<12;l++){ //loop for hour ring
    //face rotation, rotate by 3LEDS * rot, lr is index that is rotated
    lr = l+(3*rot);
    if (lr>11) lr = lr-12;
    //LEDs array
    uint8_t intensityR = 0;
    uint8_t intensityG = (1+(uint16_t)(bright_on*128.0))*(uint16_t)hour_array[l];
    uint8_t intensityB = 0;
    leds[lr+NUM_LEDS_M + SACRIFICIAL_LED].setRGB(intensityR,intensityG,intensityB);
  }
  for(byte l=0;l<24;l++){ //loop for minutes and seconds
    //face rotation, rotate by 6LEDS * rot
    lr = l+(6*rot);
    if (lr>23) lr = lr-24;
    //LEDs array
    uint8_t intensityR = (1+(uint16_t)(bright_on*96.0))*(uint16_t)minute_array[l];  //was 4+
    uint8_t intensityG = (0+(uint16_t)(bright_on*96.0))*(uint16_t)minute_array[l];  //was 3+
    uint8_t intensityB = (0+(uint16_t)(bright_on*8.0))*(uint16_t)second_array[l];  //was 3+
    leds[lr+SACRIFICIAL_LED].setRGB(intensityR,intensityG,intensityB);
  }
  leds[0].setRGB(255*((status_led&0b100)>>2),255*((status_led&0b010)>>1),255*(status_led&0b001));
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
  //section turn off hour and minute LED of currnet time
  static byte cfrot = getOrientation();  //can be less often
  byte hour12h;
  hour12h = hourFormat12();
  hour12h = (hour12h==12) ? 0 : hour12h; //noon or midnight is zero
  byte timeled_index = 0;
  timeled_index = hour12h;
  timeled_index = timeled_index+(3*cfrot); //rotation
  if (timeled_index>11) timeled_index = timeled_index-12; //rotation
  leds[timeled_index+NUM_LEDS_M + SACRIFICIAL_LED].setRGB(255,255,0); //hours
  byte minute_show = minute();
  timeled_index = (byte)(minute_show/2.5);
  timeled_index = timeled_index+(6*cfrot); //rotation
  if (timeled_index>23) timeled_index = timeled_index-24; //rotation
  leds[timeled_index+SACRIFICIAL_LED].setRGB(255,255,0); //minutes
  //LED for time are off
  FastLED.show();
  //PRINTDEBUG("\nBright:");
  //PRINTDEBUG(br);
  //trigger external light
  if (br == 5) ext_trigger(PIN_TRG_KEY1);
  if (br == 64) ext_trigger(PIN_TRG_KEY2);
  if (br == 180) ext_trigger(PIN_TRG_KEY4);
  if (br == 254) {ext_trigger(PIN_TRG_ONOFF); delay(1500); ext_trigger(PIN_TRG_ONOFF);}
}

//---------------------------------
//--------WEB FUNCTIONS------------
//---------------------------------

void check_reset_button(){
    if (!digitalRead(D3)) //button pressed
    {
      LED_blink_all(leds,1,CRGB::Yellow);
      program_restart();
    }
}

void program_restart(){
  Alarm_shining = false;
  br = 0; //alarm brightness begin
  for (int i=0;i<40;i++)
    Alarm_buffer[i] = 0;
  String chip_id = String(ESP.getFlashChipId());
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  delay(100);
  setup();
}

void launchWeb(int webtype) {
  PRINTDEBUG("\nWiFi connected : ");
  if (webtype) {
    PRINTDEBUG("\nAP SSID:");
    PRINTDEBUG(AP_SSID);
    PRINTDEBUG(chip_id);
    PRINTDEBUG("\nPASS: ");
    PRINTDEBUG(AP_PASS);
    PRINTDEBUG("\nIP: ");
    ip = WiFi.softAPIP();
  } else {
    PRINTDEBUG("\nLocal WiFi, IP: ");
    ip = WiFi.localIP();
  }
  PRINTDEBUG(ip);
  
  delay(10);
  if (!mdns.begin("Alight",ip)) {
    PRINTDEBUG("\nError setting up MDNS responder! deadlock, restarting");
    LED_blink_all(leds,20,CRGB::Red);
    program_restart();
    }
  PRINTDEBUG("\nmDNS responder started");
  // Start the server
  server.begin();
  mdns.addService("http","tcp",80);
  PRINTDEBUG("\nServer started");
  delay(100);
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

int mdns1(int webtype, String WifiList) //main web function that do everything. responds to requests etc..
//if returned OK_VAL, everything is ok, ERROR_VAL is bad
{
  // Check for any mDNS queries and send responses
  mdns.update();
  WiFiClient client = server.available();
  if (!client) {
    return OK_VAL;
  }
  PRINTDEBUG("");
  PRINTDEBUG("\n\nNew client request: ");
  String req = client.readStringUntil('\r');
  PRINTDEBUG(req);
  // Wait for data from client to become available
  if (client.connected() && !client.available()) {
    PRINTDEBUG("\nGone");
    return OK_VAL;
  }

  // Read the first line of HTTP request

  // First line of HTTP request looks like "GET /path HTTP/1.1"
  // Retrieve the "/path" part by finding the spaces
  int addr_start = req.indexOf(' ');
  int addr_end = req.indexOf(' ', addr_start + 1);
  if (addr_start == -1 || addr_end == -1) {
    Serial.print("\nInvalid request: ");
    PRINTDEBUG(req);
    return OK_VAL;
  }
  req = req.substring(addr_start + 1, addr_end);
  Serial.print("\nRequest: ");
  PRINTDEBUG(req);
  client.flush();

  String s;
  int commaIndex[6];
  String temp;
  

  if ( webtype == 1 ) {
    PRINTDEBUG(req);
    if (req == "/")
    {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>A_light";
      s += "<p>";
      s += "<form method='get' action='a'><label>SSID: </label><input name='ssid' length=32><label><br><br>PASS: </label><input name='pass' length=64><label><br><br><input type='submit'><br><br>";
      s += WifiList;
      s += "</form>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      LED_blink_all(leds,1,CRGB::Blue);
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
        LED_blink_all(leds,1,CRGB::Red);
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
        LED_blink_all(leds,2,CRGB::Green); 
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
      LED_blink_all(leds,1,CRGB::Green);
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
      LED_blink_all(leds,1,CRGB::Yellow);
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
      LED_blink_all(leds,1,CRGB::Green);
      summer_mode=true;
    }
    else
    {
      s = "HTTP/1.1 404 Not Found\r\n\r\n";
      PRINTDEBUG("\nSending 404");
      LED_blink_all(leds,1,CRGB::Red);  //get rid of this maybe, because browser asks for an icon every load and it is error
    }
  }
  else //web type 0 is when connected to local wifi
  {
    if (req == "/")
    {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>ALight active<br>";
      s += "<p>";
      s += "</html>\r\n\r\n";
      PRINTDEBUG("\nSending 200");
      LED_blink_all(leds,1,CRGB::Blue);
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
      LED_blink_all(leds,1,CRGB::Green);
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
      LED_blink_all(leds,1,CRGB::Yellow);
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
      LED_blink_all(leds,1,CRGB::Green);
      summer_mode=true;
    }
    else if ( req.startsWith("/time") ) {
      // getting time from time server, value returned to debug serial link only yet
      //gettime();
      LED_blink_all(leds,1,CRGB::Purple);
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
      tm.Year = temp.toInt(); //years from 2000, set time function needs 2019 or 19.
      temp = req.substring(commaIndex[1]+1, commaIndex[1]+3);
      tm.Hour = temp.toInt();
      temp = req.substring(commaIndex[1]+4, commaIndex[1]+6);
      tm.Minute = temp.toInt();
      temp = req.substring(commaIndex[1]+7, commaIndex[1]+9);
      tm.Second = temp.toInt();
      temp = req.substring(commaIndex[1]+10);
      tm.Wday = temp.toInt();

      /*#if not defined(RTC_DS1307) || defined(RTC_DS3231)
        setTime(tm.Hour,tm.Minute,tm.Second,tm.Day,tm.Month,tm.Year);
      #endif*/
      //run always, not only when no RTC is defined. Helps time library recognize that time is set.
      setTime(tm.Hour,tm.Minute,tm.Second,tm.Day,tm.Month,tm.Year);
      
      #ifdef RTC_DS3231
        Clock.setClockMode(false);  // set to 24h
        Clock.setYear(tm.Year);
        Clock.setMonth(tm.Month);
        Clock.setDate(tm.Day);
        Clock.setDoW(tm.Wday);
        Clock.setHour(tm.Hour);
        Clock.setMinute(tm.Minute);
        Clock.setSecond(tm.Second);
      #endif
      #ifdef RTC_DS1307
        Rtc.SetDateTime(RtcDateTime(tm.Year,tm.Month,tm.Day,tm.Hour,tm.Minute,tm.Second)); //needs year as 2019 or 19
        if (!Rtc.GetIsRunning())
        {
          Rtc.SetIsRunning(true);
        }
      #endif
      
      time_show(clock_face_rotation,0);
      
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
        LED_blink_all(leds,Alarm_ID,CRGB::Green);
      }else{
        LED_blink_all(leds,Alarm_ID,CRGB::Orange);
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
      LED_blink_all(leds,1,CRGB::Orange);
    }
    else if (req.startsWith("/?SET") ) { //Sync   //Example: /?SET,5,Name+SecondWord; HTTP/1.1
      //if success
      s = buildHTTPResponse(strResponseToSet);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToSet);
      LED_blink_all(leds,1,CRGB::Green);
    }
    else if (req.startsWith("/?RES") ) { //Reset memory
      //if success
      s = buildHTTPResponse(strResponseToRes);
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToRes);
      LED_blink_all(leds,1,CRGB::Green);
    }
    else if (req.startsWith("/?DEB") ) { //Debugging screen
      //if success
      s = buildHTTPResponse("Debugging will be here");
      PRINTDEBUG("Sending:");
      PRINTDEBUG(strResponseToRes);
      LED_blink_all(leds,1,CRGB::Green);
    }
    else
    {
      s = buildHTTPResponse("ERR");
      PRINTDEBUG("\nSending ERR");
      LED_blink_all(leds,1,CRGB::Red);
    }
  }
  client.print(s);
  PRINTDEBUG("\nDone with client");
  return OK_VAL;
}


time_t RTC_time_read(){
  //PRINTDEBUG("RTC read\n");
  #ifdef RTC_DS3231
    tm.Year = Clock.getYear()+30; //offset from 1970
    tm.Month = Clock.getMonth(Century);
    tm.Day = Clock.getDate();
    tm.Wday = Clock.getDoW(); //1-7 Sunday to Saturday
    tm.Hour = Clock.getHour(h12,PM); //24h = false, AM/PM thing for 12h does not matter
    tm.Minute = Clock.getMinute();
    tm.Second = Clock.getSecond();
  #endif
  #ifdef RTC_DS1307
    RtcDateTime ttime;
    ttime = Rtc.GetDateTime();
    tm.Year = (uint8_t)(ttime.Year()-1970);
    tm.Month = ttime.Month();
    tm.Day = ttime.Day();
    tm.Wday = ttime.DayOfWeek()+1; //1-7 Sunday to Saturday
    tm.Hour = ttime.Hour();
    tm.Minute = ttime.Minute();
    tm.Second = ttime.Second();
  #endif

  time_t tRTC = makeTime(tm);
  if (tRTC < 1512330480) //rudimentary check if time is correct, not older than this code
    {return 0;
    PRINTDEBUG("\nRTC Time not valid!");}
  else
    return tRTC;
}


bool check_alarm(){ //need to be run at least every 60 seconds (time of alarm have to match exactly on hour,minute and Wday)
  if (Alarm_shining == true){
    //PRINTDEBUG("\nAlarm timeout in: \t");PRINTDEBUG((ALARM_TIMEOUT_MS - (millis() - alarm_timeout_from_ms))/1000);PRINTDEBUG(" s");
    if (millis() - alarm_timeout_from_ms > ALARM_TIMEOUT_MS){
      Alarm_shining = false; //timeout happened
      LED_blink_all(leds,20,CRGB::Blue);
      //turn off external light. 
      //this will not work if only "1" position is on - then it will tirn it on full
      //will work in all other cases
      ext_trigger(PIN_TRG_KEY1);
      delay(3000);
      ext_trigger(PIN_TRG_ONOFF);
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

byte getOrientation(void)
{
  // accel.readPL() will return a byte containing information
  // about the orientation of the sensor. It will be either
  // PORTRAIT_U, PORTRAIT_D, LANDSCAPE_R, LANDSCAPE_L, or
  // LOCKOUT.
  byte pl = accel.readPL();
  switch (pl)
  {
  case PORTRAIT_U:
    //Serial.print("Portrait Up"); //00
    return 0; 
    break;
  case PORTRAIT_D:
    //Serial.print("Portrait Down"); //02
    return 2;
    break;
  case LANDSCAPE_R:
    //Serial.print("Landscape Right"); //01
    return 1;
    break;
  case LANDSCAPE_L:
    //Serial.print("Landscape Left"); //03
    return 3;
    break;
  case LOCKOUT:
    //Serial.print("Flat");
    return 0;
    break;
  }
  return 0;
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
 
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
 
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
 
  Serial.println();
  Serial.println("-----------------------");
 
}

////////////////////////////////////////////////////
///////////////////DEBUG////////////////////////////
////////////////////////////////////////////////////
#if DEBUG == 1

#endif

void loop() {
  //nothing
}

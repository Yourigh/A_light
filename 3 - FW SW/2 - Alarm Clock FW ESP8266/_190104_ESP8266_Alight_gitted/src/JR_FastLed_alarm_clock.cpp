#include <ESP8266WiFi.h>
#include "JR_FastLed_alarm_clock.h"
#include "FastLED.h"
#include <Time.h>
#include <TimeLib.h>

#include "i2s.h"
#include "i2s_reg.h"
#include "Wave_const.h"

void ESP_on_board_LED_off(){
  pinMode(LED_BUILTIN_JR, OUTPUT); 
  pinMode(D3, INPUT); //D3 or GPIO0 is button
  digitalWrite(LED_BUILTIN_JR, HIGH); //do not interfere with light sensor, inverted logic
}

void ext_trigger(uint8_t GPIOtrg){
  digitalWrite(GPIOtrg,1);
  delay(250);
  digitalWrite(GPIOtrg,0);
}

////////////////////////////////
//////LED strip functions///////
////////////////////////////////




void LED_loading_anim_timed(struct CRGB *leds){
    static unsigned long last_update_LED_M;
    static byte dot_H = 0;
    static byte dot_M = 0;
        //Loading animation!!!
    #define UPDATE_LED_MS 40 //for loading animation
    if (millis()-last_update_LED_M>UPDATE_LED_MS){
      if (++dot_M==NUM_LEDS_M){ dot_M = 0;  }
      if (++dot_H==NUM_LEDS_H){ dot_H = 0;  }
      LED_loading_anim(leds, dot_M, 0);
      LED_loading_anim(leds, dot_H, 1);
      last_update_LED_M = millis();
    }
}
void LED_loading_anim(struct CRGB *leds, byte dot, byte strip_num) {
  #define TAIL_LEDS_H 4
  #define TAIL_LEDS_M 8
  const byte max_bright[3] = {128,63,3};// {3,128,63};
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

void LED_blink_all(struct CRGB *leds, byte count_blinks, struct CRGB TheColor) { //0 means forever
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
    } else //else none, else is only when count_blinks is 0
    {
      return;
    }
  }
}

String special_char_fix(String input) {
  //search for first % in input
  char* b;
  char to_replace[4] = {'+', '\0', '\0', '\0'};
  String replace_with; //= "X";
  unsigned long number = 0;
  uint16_t i = 0;

  replace_with = String(' ');
  i = 0;
  while(i++<100){ //fix space issue - when space is in password, it goes through as + and get saved. + is %0B - 43 code
    b = strchr(input.c_str(), '+');
    if (b != NULL) { //found
      input.replace(String(to_replace), replace_with); //replace + to space
    }
  }
  i=0;
  to_replace[0] = '%';
  to_replace[1] = '0';
  to_replace[2] = '0';
  to_replace[3] = '\0';
  while (i++<1000) { 
    b = strchr(input.c_str(), '%');
    //found % ?
    if (b == NULL) { //% not found
      input.replace(html_replace[254], '%'); //replacing back  //'�'
      return input; //NOT FOUND
    }
    //string a begins with % and follows by two char long code
    char code[3] =  {'0', '0', '\0'};
    code[0] = *(b + 1); //next character after %
    code[1] = *(b + 2); //next char after % and hex number MSB
    number = strtoul( code, 0, 16); //get number from hex code
    if (number == 37)
    { number = 254; //replace character % with �. no one uses � and % interferes with algorithm,
      //so � will be replaced back to % in the end.
    }
    to_replace[1] = code[0];
    to_replace[2] = code[1];
    replace_with = String(html_replace[number]); //lookup character representation on the code %code
    input.replace(String(to_replace), replace_with);
  }
  return input; //found something
}

int testWifi(struct CRGB *leds) { //20 - connected to local, 10 - not connected to localint testWifi(void) { //20 - connected to local, 10 - not connected to local
  unsigned long last_check_wifi = millis();
  
  PRINTDEBUG("\nWaiting for Wifi to connect\nStatus:");
  wl_status_t current_status;
  while (millis()-last_check_wifi<25000) { //change to 50*500ms=25s - connection check timeout
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
      LED_blink_all(leds,3,CRGB::Green);
      return OK_VAL;
    } else if ( (current_status==WL_NO_SSID_AVAIL)| //after while, when SSID that was saved in memory not found
                (current_status==WL_IDLE_STATUS)  | //when no SSID is in memory
                (current_status==WL_CONNECT_FAILED)){ //fail is fail
                  PRINTDEBUG(current_status); 
      break; //breaks while before time is left if it is sure we will not connect
    }
    //Loading animation!!!
    LED_loading_anim_timed(leds); //keep in loop that repeats at least every 5ms
    delay(2);
  }
  LED_blink_all(leds,1,CRGB::Red);
  PRINTDEBUG("\nConnect timed out, opening AP\n");
  PRINTDEBUG("password:");
  PRINTDEBUG(AP_PASS);PRINTDEBUG("\n");
  return ERROR_VAL;
}

String scanWifi_list(void){
  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED)  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  PRINTDEBUG("\nScan done\n");
  String st = "<ul>"; //string with networks for http page beginning
  if (n == 0)
  {
    PRINTDEBUG("\nno networks found");
    st += "<li>No Wifi found </li>";
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
  }
  PRINTDEBUG("");
  st += "</ul>";
  return st;
}

void setupAP(String chip_id) {
  WiFi.mode(WIFI_AP);
  delay(100);
  String ssid = AP_SSID + chip_id;
  String password = AP_PASS;
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

uint16_t exp_moving_average(uint16_t avg_in,uint16_t new_sample){
  //PRINTDEBUG(new_sample);
  //PRINTDEBUG("\n");
  avg_in -= (avg_in >> 3); // div 8
  avg_in += (new_sample >> 3);
  return avg_in;
}

bool writeDAC(uint16_t DAC=0x8000) {
  static uint32_t i2sACC;
  static uint16_t err;

 for (uint8_t i=0;i<32;i++) { 
  i2sACC=i2sACC<<1;
  if(DAC >= err) {
    i2sACC|=1;
    err += 0xFFFF-DAC;
  }
    else
  {
    err -= DAC;
  }
 }
 bool flag=i2s_write_sample(i2sACC);
 return flag;
}

void write_audio_sample(){
  static uint32_t phase = 0;
  bool flag;
  while (!(i2s_is_full()))
      {
        flag = writeDAC(0x8000 + (256 * (uint16_t)wave[phase++]));
        yield();//reset WDT, otherwise occational resets.
        if (phase == LENGTH_WAVE) phase = 0;
        if (!flag) {PRINTDEBUG("\nERROR IN PLAYBACK\n");break;}
      }
}
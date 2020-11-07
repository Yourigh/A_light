#if !defined(JR_CLOCK)
#define JR_CLOCK 1

#include <ESP8266WiFi.h>
#include "FastLED.h"
#include "Alight_web.h"
#include "i2s.h"
#include "i2s_reg.h"
#include "Wave_const.h"

////////////////////////
// USER SERRINGS 1
////////////////////////
#define SHOW_SECONDS 1
#define AP_SSID "A_light"
#define AP_PASS "setupsetup"

#define DEBUG 1 //UART logs

#if DEBUG == 1
  #define PRINTDEBUG Serial.print
#else
  #define PRINTDEBUG
#endif 

#define OK_VAL 20
#define ERROR_VAL 10
////////////////////////
// WEB Related
////////////////////////
/***Commands**/
#define strResponseToSet "CT"
#define strResponseToAdd "AT"
#define strResponseToDel "DT"
#define strResponseToSyn "ST"
#define strResponseToRes "RT"
/*************/

#define LED_BUILTIN_JR D4
void ESP_on_board_LED_off();

//FastLED
#define NUM_LEDS_H 12
#define NUM_LEDS_M 24
#define SACRIFICIAL_LED 1 //0 or 1 if additinal LED for level shift is used.
#define DATA_PIN_LED_M_H 12

//Output trigger
#define PIN_TRG_ONOFF 16
#define PIN_TRG_KEY4 14
#define PIN_TRG_KEY1 5
#define PIN_TRG_KEY2 15

void ext_trigger(uint8_t GPIOtrg);

void LED_loading_anim_timed(struct CRGB *data); //place function in fast loop, timed internally
void LED_loading_anim(struct CRGB *data, byte dot, byte strip_num);

void LED_blink_all(struct CRGB *leds, byte count_blinks, struct CRGB TheColor);

String special_char_fix(String input); //html fix for %HEXHEX special chars, used in passwords

int testWifi(struct CRGB *leds); //tests WiFi

String scanWifi_list(void); //scans Wifi Networks and returns HTTP string bullet point list
void setupAP(String chip_id); //creates access point with AP_SSID + chip_id name and AP_PASS password

String buildHTTPResponse(String content);

uint16_t exp_moving_average(uint16_t avg_in,uint16_t new_sample);

bool writeDAC(uint16_t DAC);
void write_audio_sample();

#endif
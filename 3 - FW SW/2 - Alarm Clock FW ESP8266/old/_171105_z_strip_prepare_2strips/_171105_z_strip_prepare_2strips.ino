#include <Time.h>
#include <TimeLib.h>

//#define NANO
#define ESP

#ifdef ESP
#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#include "FastLED.h"

// How many leds in your strip?
#define NUM_LEDS_H 12
#define TAIL_LEDS_H 5
#define NUM_LEDS_M 24 //minutes are connected first
#define TAIL_LEDS_M 23
#define SACRIFICIAL_LED 1 //0 or 1 if additinal LED for level shift is used.

#ifdef ESP
//#define DATA_PIN_LED_H 2//12
#define DATA_PIN_LED_M_H 12//2
#endif
#ifdef NANO
#define DATA_PIN_LED_H 13
#define DATA_PIN_LED_M 2 //not tested this pin
#endif

#define UPDATE_LED_MS 40
#define SHOW_SECONDS

int LightValue = 250;

const byte max_bright[3] = {220, 60, 6};
unsigned long last_update_LED;
byte dot_H = 0;
byte dot_M = 0;
int led = HIGH;
time_t t = 1510523146; //21:45

byte style = 0;
byte count =0;

CRGB leds[NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED];

void setup() {
  setTime(t);
  FastLED.addLeds<WS2812, DATA_PIN_LED_M_H, GRB>(leds, NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED);
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  dot_H = 0;
  last_update_LED = millis();
  delay(1);
  dot_M = 0;

  LED_blink_all(2, CRGB::White);

  TimeElements tm;

  
  for (int x=0;x<24;x++){
    //if (x==24) x=0;
    Serial.print("\n24Time:");
    Serial.print(x);
    tm.Hour = x;
    setTime(tm.Hour,tm.Minute,tm.Second,tm.Day,tm.Month,tm.Year);
    time_show();
    delay(1000);
  }
}

void loop() {

  

  if (millis() - last_update_LED > UPDATE_LED_MS) {
    //style++;
    if (style) {
      //rolling 2 circles at once, one 2 times faster
      if (++dot_H >= NUM_LEDS_H) {
        dot_H = 0;
        if (count++==7) {
          count = 0;
          style = 0;
        }
      }

      LED_loading_anim(dot_H, 1);
      if (++dot_M == NUM_LEDS_M) {
        dot_M = 0;
      }
      LED_loading_anim(dot_M, 0);
      last_update_LED = millis();

    } else {
      //joining 2 together 
      if (++dot_H == NUM_LEDS_H + NUM_LEDS_M) {
        dot_H = 0;
        if (count++==5) {
          count = 0;
          style = 1;
        }
      }

      LED_loading_anim_joined(dot_H);
      last_update_LED = millis();
    }

  }



}

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
  FastLED.show();
}

void LED_loading_anim_joined(byte dot) {
  byte dot_loopback = dot;
  byte tail_leds = TAIL_LEDS_H;
  byte num_leds = NUM_LEDS_H + NUM_LEDS_M;
  byte led_begin = 1;
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
  FastLED.show();
}


void LED_blink_all(byte count_blinks, struct CRGB TheColor) { //0 means forever
  byte blinks = 0;
  while (1) { //forever when zero
    fill_solid( leds, NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED, TheColor);
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

void check_reset_button() {
  int a = 0;
  a++;
}

void time_show(){
  byte bright_on = (LightValue > 190) ? 1 : 0;
  byte hour12h;
  hour12h = hourFormat12();
  hour12h = (hour12h==12) ? 0 : hour12h; //noon or midnight is zero
  bool hour_array[12];
  bool minute_array[24];
  bool second_array[24];
  byte minute_show = minute();
  byte second_show = second();

  Serial.print("\nh:");
  Serial.print(hour12h);
  Serial.print("\nm:");
  Serial.print(minute_show);
  Serial.print("\ns:");
  Serial.print(second_show);

Serial.print("\nharray:");
  for(byte l=0;l<12;l++){
    hour_array[l] = !((hour12h+1)<=l); //true is shine
    if (hour()==12)
      hour_array[0]=true;
    else
      hour_array[0]=false;
    
    Serial.print(hour_array[l]);
    Serial.print("\t,");
  }
  for(byte l=0;l<24;l++){
    minute_array[l] = !((minute_show/2.5)<=l);
    #ifdef SHOW_SECONDS
      second_array[l] = !((second_show/2.5)<=l);
    #endif
  }
  Serial.print("\nnh  idx:");
  for(byte l=0;l<12;l++){
    Serial.print(l+NUM_LEDS_M + SACRIFICIAL_LED);
    Serial.print("\t,");
    leds[l+NUM_LEDS_M + SACRIFICIAL_LED].setRGB((1+(bright_on*128))*(int)hour_array[l],(1+(bright_on*128))*(int)hour_array[l],0);
  }
  for(byte l=0;l<24;l++){
    leds[l+SACRIFICIAL_LED].setRGB(0,(4+(bright_on*128))*(int)minute_array[l],(3+(bright_on*16))*(int)second_array[l]);
  }
  FastLED.show();
}

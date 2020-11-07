//#define NANO
#define ESP

#ifdef ESP
  #define FASTLED_ESP8266_RAW_PIN_ORDER
  #define FASTLED_ALLOW_INTERRUPTS 0
#endif

#include "FastLED.h"

// How many leds in your strip?
#define NUM_LEDS_H 24//60
#define TAIL_LEDS_H 8//10
#ifdef ESP
  #define DATA_PIN_LED_H 2//12
#endif
#ifdef NANO
  #define DATA_PIN_LED_H 13
#endif

#define UPDATE_LED_MS 30

const byte max_bright[3] = {3,128,63};
unsigned long last_update_LED_H;
byte dot_H = 0;
int led = HIGH;

// Define the array of leds
CRGB leds[NUM_LEDS_H];

void setup() { 
      // FastLED.addLeds<WS2812, DATA_PIN_LED_H, RGB>(leds, NUM_LEDS_H);
      FastLED.addLeds<WS2812B, DATA_PIN_LED_H, GRB>(leds, NUM_LEDS_H);
      Serial.begin(9600);
      pinMode(LED_BUILTIN, OUTPUT);
      digitalWrite(LED_BUILTIN, HIGH); 
      dot_H = 0;
      last_update_LED_H = millis();

      delay(500);

      LEDH_blink_all(2,CRGB::Red);
}

void loop() { 
  if (millis()-last_update_LED_H>UPDATE_LED_MS){
    if (++dot_H==NUM_LEDS_H){ dot_H = 0;  }
    digitalWrite(LED_BUILTIN, led); 
    if (led==HIGH){led = LOW;}
    loading_anim(dot_H);
    last_update_LED_H = millis();
  }
}

void loading_anim(byte dot_H){
    byte dot_H_loopback = dot_H;
    for(byte t=0;t<=(TAIL_LEDS_H);t++){ //set max brightness gradually decreasing, not last one, that have to be 0, computing takes 180 us
      if (t>dot_H) {
        dot_H_loopback = NUM_LEDS_H+dot_H; //t-dot_H gives how much off scale (negative it would be), go from end instead 
        //NUM_LEDS_H-(t-dot_H) addresses correct led. -t will be subtracted, so NUM+dot_H
      }
      if (t==TAIL_LEDS_H) {
        leds[dot_H_loopback-t].setRGB(0,0,0); //turn off the last one so it keeps off after the TAIL goes away
      }else{    //for all tail leds
        //leds[dot_H_loopback-t].setRGB(max_bright[0]-t*step_brightness[0],max_bright[1]-t*step_brightness[1],max_bright[2]-t*step_brightness[2]); //previous version
        //first full, next 4 each 42 less
        leds[dot_H_loopback-t].setRGB(max_bright[0]/(3*t+1),max_bright[1]/(3*t+1),max_bright[2]/(3*t+1));
      }
    }
    FastLED.show();
}

void LEDH_blink_all(byte count_blinks, struct CRGB TheColor){//0 means forever
  byte blinks=0;
  while(1){ //forever when zero
    fill_solid( leds, NUM_LEDS_H, TheColor);
    FastLED.show();
    delay(10);//100
    FastLED.clear ();
    FastLED.show();
    delay(20);//200
    if (count_blinks>0){//should be exiting some time
      blinks++;
      if (blinks==count_blinks) return;
    } //else forever, else is only when count_blinks is 0
  }
}


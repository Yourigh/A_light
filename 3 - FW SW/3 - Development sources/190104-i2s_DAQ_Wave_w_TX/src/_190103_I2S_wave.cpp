#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "i2s.h"
#include "i2s_reg.h"
#include "HardwareSerial.h"

#include "Wave_const.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"
//FastLED
#define NUM_LEDS_H 12
#define NUM_LEDS_M 24
#define SACRIFICIAL_LED 1 //0 or 1 if additinal LED for level shift is used.
#define DATA_PIN_LED_M_H 12
CRGB leds[NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED];
byte br = 1;


bool writeDAC(uint16_t DAC);

uint32_t i2sACC;
uint8_t i2sCNT=32;
uint16_t DAC=0x8000;
uint16_t err;

uint16_t phase;
uint16_t a;
uint16_t full = 0;
uint32_t Nfull = 0;

unsigned long ms_i2s_buf_fill = 0;
unsigned long ms_log = 0;
uint16_t avail_before_writing = 0;
 
void setup() {
  //initial send PowerON
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  Serial.print("\nPower On!\n");

  //initial blink
  pinMode(D4,OUTPUT);
  for(int i=0;i<2;i++){
    digitalWrite(D4,0);
    delay(200);
    digitalWrite(D4,1);
    delay(200);
  }
  FastLED.addLeds<WS2812B, DATA_PIN_LED_M_H, GRB>(leds, NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED);//GBR?
  FastLED.setBrightness(  255 );
  fill_solid( leds, NUM_LEDS_H + NUM_LEDS_M + SACRIFICIAL_LED, CRGB(0,50,0));
  FastLED.show();
  delay(2000);

  //WiFi.forceSleepBegin();
  //delay(1);
  //system_update_cpu_freq(160); //cannot run, interferes with fastled

  fill_solid( leds, NUM_LEDS_H + NUM_LEDS_M + SACRIFICIAL_LED, CRGB(0,0,50));
      FastLED.show();
      delay(2000);
 
  i2s_begin();
  i2s_set_rate(22050);

  delay(150);
  fill_solid( leds, NUM_LEDS_H + NUM_LEDS_M + SACRIFICIAL_LED, CRGB(50,0,0));
      FastLED.show();
      delay(2000);
}

void loop() {
  //phase=phase+5; //+5 is 1kHz
  
  
  if (millis()-ms_i2s_buf_fill>0){ //have to come here every 20ms or more often (for 160MHz). But 160Mhz cannot be issued cause it interferes with FastLED
    if (!(i2s_is_full()))
    {
      Nfull++;
       // returns the number of samples than can be written before blocking
      avail_before_writing = i2s_available();
      while (!(i2s_is_full()))
      {
        writeDAC(0x8000 + ((int8_t)wave[phase++] * 256));
        if (phase == LENGTH_WAVE) phase = 0;
      }
    }
    else
    {
      full++;
    }
    ms_i2s_buf_fill = millis();
  }


  if (millis()-ms_log>500) {
    Serial.print("f\t");
    Serial.print(full,DEC);
    Serial.print("\tNf\t");
    Serial.print(Nfull,DEC);
    Serial.print("\tAv\t");
    Serial.print(avail_before_writing,DEC);
    Serial.print("\n");
    avail_before_writing = 0;
    full = 0;
    Nfull = 0;
    ms_log = millis();
  }
  delayMicroseconds(1); //15 is making gaps, 14 is ok
}


bool writeDAC(uint16_t DAC) {
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

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "HardwareSerial.h"
#include <Wire.h>
#include <SparkFun_MMA8452Q.h> // Includes the SFE_MMA8452Q library
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"

MMA8452Q accel;
//FastLED
#define NUM_LEDS_H 12
#define NUM_LEDS_M 24
#define SACRIFICIAL_LED 1 //0 or 1 if additinal LED for level shift is used.
#define DATA_PIN_LED_M_H 12

CRGB leds[NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED];
byte br = 1;

void printAccels();
void printCalculatedAccels();
void printOrientation();


void setup() {
  //initial send PowerON
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  Wire.begin(4,0);

  FastLED.addLeds<WS2812B, DATA_PIN_LED_M_H, GRB>(leds, NUM_LEDS_M + NUM_LEDS_H + SACRIFICIAL_LED);//GBR?
  FastLED.setBrightness(  255 );
  fill_solid( leds, NUM_LEDS_H + NUM_LEDS_M + SACRIFICIAL_LED, CRGB(0,255,0));
  FastLED.show();

    //initial blink
  pinMode(D4,OUTPUT);
  for(int i=0;i<5;i++){
    digitalWrite(D4,0);
    delay(200);
    digitalWrite(D4,1);
    delay(200);
  }
  

  Serial.print("\nPower On!\n");

  if(accel.init(SCALE_4G,ODR_400)==1)
  Serial.print("\nInit OK!\n");
  else
  Serial.print("\nInit NOK!\n");

  pinMode(13,INPUT);

  //WiFi.forceSleepBegin();
  //delay(1);
}
// The loop function will simply check for new data from the
//  accelerometer and print it out if it's available.
void loop()
{
  if (digitalRead(13)==LOW){
    Serial.println("ISR!!!!!!!!!!!!!!!!");
    delay(100);
  }
  //delay(200);
  // Use the accel.available() function to wait for new data
  //  from the accelerometer.
  if (accel.available())
  {
    //INTENSITY BY x ACCELERATION
    br = (byte)(122.0*(accel.cx < 0 ? (-1.0*accel.cx) : accel.cx)+5.0);
    Serial.println(br,DEC);
    fill_solid( leds, NUM_LEDS_H + NUM_LEDS_M + SACRIFICIAL_LED, CRGB(0,0,br));
    FastLED.show();

    if (accel.readTap()){
      //tap detected
      fill_solid( leds, NUM_LEDS_H + NUM_LEDS_M + SACRIFICIAL_LED, CRGB(100,0,0));
      FastLED.show();
      delay(150);
    }

    // First, use accel.read() to read the new variables:
    accel.read();
    
    // accel.read() will update two sets of variables. 
    // * int's x, y, and z will store the signed 12-bit values 
    //   read out of the accelerometer.
    // * floats cx, cy, and cz will store the calculated 
    //   acceleration from those 12-bit values. These variables 
    //   are in units of g's.
    // Check the two function declarations below for an example
    // of how to use these variables.
    printCalculatedAccels();
    //printAccels(); // Uncomment to print digital readings
    
    // The library also supports the portrait/landscape detection
    //  of the MMA8452Q. Check out this function declaration for
    //  an example of how to use that.

    printOrientation();
    
    Serial.println(); // Print new line every time.
  }
}

// The function demonstrates how to use the accel.x, accel.y and
//  accel.z variables.
// Before using these variables you must call the accel.read()
//  function!
void printAccels()
{
  Serial.print(accel.x, 3);
  Serial.print(",");
  Serial.print(accel.y, 3);
  Serial.print(",");
  Serial.print(accel.z, 3);
  Serial.print("");
}

// This function demonstrates how to use the accel.cx, accel.cy,
//  and accel.cz variables.
// Before using these variables you must call the accel.read()
//  function!
void printCalculatedAccels()
{ 
  Serial.print(accel.cx, 3);
  Serial.print(",");
  Serial.print(accel.cy, 3);
  Serial.print(",");
  Serial.print(accel.cz, 3);
  Serial.print("");
}

// This function demonstrates how to use the accel.readPL()
// function, which reads the portrait/landscape status of the
// sensor.
void printOrientation()
{
  // accel.readPL() will return a byte containing information
  // about the orientation of the sensor. It will be either
  // PORTRAIT_U, PORTRAIT_D, LANDSCAPE_R, LANDSCAPE_L, or
  // LOCKOUT.
  byte pl = accel.readPL();
  switch (pl)
  {
  case PORTRAIT_U:
    Serial.print("Portrait Up"); //00 
    break;
  case PORTRAIT_D:
    Serial.print("Portrait Down"); //02
    break;
  case LANDSCAPE_R:
    Serial.print("Landscape Right"); //01
    break;
  case LANDSCAPE_L:
    Serial.print("Landscape Left"); //03
    break;
  case LOCKOUT:
    Serial.print("Flat");
    break;
  }
}

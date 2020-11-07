//#include "http_replace.h"
#include "Alight_web.h"
/*
  AnalogReadSerial

  Reads an analog input on pin 0, prints the result to the Serial Monitor.
  Graphical representation is available using Serial Plotter (Tools > Serial Plotter menu).
  Attach the center pin of a potentiometer to pin A0, and the outside pins to +5V and ground.

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/AnalogReadSerial
*/

// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(115200);
}

// the loop routine runs over and over again forever:
void loop() {
  String input = "TyrNET2%3F";
  String output;
  //!   @   #   $   %   ^   &   %   +   ?   §
  //%21 %40 %23 %24 %25 %5E %26 %25 %2B %3F %A7
  
  Serial.print("\n\nInput :");
  Serial.print(input);

  output = special_char_fix(input);

  Serial.print("\nOutput :");
  Serial.print(output);
  
  delay(7000);        // delay in between reads for stability
}



String special_char_fix(String input){
  //search for first % in input
  char* b;
  char* to_replace = "%00";
  char* replace_with = "X";
  unsigned long number = 0;
  
  while (1) {
    b = strchr(input.c_str(),'%');
  
    //found
    Serial.print("\nFound %:");
    Serial.print(b); //print character that should be replaced with
    
    if (b==NULL){
      input.replace('þ','%');
      return input; //NOT FOUND
    }
    
    //string a begins with % and follows by two char long code
    char code[3] =  {'0','0','\n'};
    code[0] = *(b+1);
    code[1] = *(b+2);
    number = strtoul( code, nullptr, 16); //get number from hex code

    if (number == 37)
    {
      number = 254; //replace character % with þ. no onw uses þ and % interferes with algorithm, so þ will be replaced back to % in the end.
    }
    
    *(to_replace+1)=code[0];
    *(to_replace+2)=code[1];
  

    *(replace_with) = html_replace[number]; //lookup character representation on the code %code
    input.replace(to_replace,replace_with);
  
    Serial.print("\nDebug1 to replace this:");
    Serial.print(to_replace); //print character that should be replaced with
  
    Serial.print("\nChar number DEC:");
    Serial.print(number,DEC);
    Serial.print("\nChar number HEX:");
    Serial.print(number,HEX);
    Serial.print("\nChar:");
    Serial.print(html_replace[number]);
  
    
    Serial.print("\nDebug2 with this:");
    Serial.print(replace_with);

    Serial.print("\n--Output:");
    Serial.print(input);
    
    Serial.print("\n-----------------next char");    
    delay(500);
  }
}


#include <ESP8266WiFi.h>

#define way 1

void setup() {
  Serial.begin(115200);
  Serial.println("Reset start");
  // put your setup code here, to run once:

  if (way == 1) {
    //first this
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
  } else {
    //if does not help then this
    ESP.eraseConfig();
    ESP.reset();
  }
  Serial.println("Reset done");
}

void loop() {
  // put your main code here, to run repeatedly:

}

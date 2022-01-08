#include <WiFi.h>
//#include <ESP8266WiFi.h>
 
void setup(){
  Serial.begin(9600);
  WiFi.mode(WIFI_MODE_STA);
  Serial.println();
  Serial.println(WiFi.macAddress());
}
 
void loop(){

}

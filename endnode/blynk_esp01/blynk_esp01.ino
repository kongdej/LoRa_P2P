
#define BLYNK_TEMPLATE_ID "TMPLgb6AkxGS"
#define BLYNK_DEVICE_NAME "relay"
#define BLYNK_AUTH_TOKEN "g616S90kXEsAvXXeuLh_eLlH8ijSZ77C"

#define BLYNK_FIRMWARE_VERSION        "0.1.3"

#define BLYNK_PRINT Serial

//#define APP_DEBUG

#include "BlynkEdgent.h"


#define CH_PD 44 //sinal de controle de CH_PD
#define RST 46 //sinal de controle de RST
#define GPIO0 48 //sinal de controle de GPIO0
uint8_t R1On[]  = {0xA0, 0x01, 0x01, 0xA2};
uint8_t R1Off[] = {0xA0, 0x01, 0x00, 0xA1};
uint8_t R2On[]  = {0xA0, 0x02, 0x01, 0xA3};
uint8_t R2Off[] = {0xA0, 0x02, 0x00, 0xA2};
uint8_t R3On[]  = {0xA0, 0x03, 0x01, 0xA4};
uint8_t R3Off[] = {0xA0, 0x03, 0x00, 0xA3};
uint8_t R4On[]  = {0xA0, 0x04, 0x01, 0xA5};
uint8_t R4Off[] = {0xA0, 0x04, 0x00, 0xA4};


BLYNK_WRITE(V2) {
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  Serial.printf("Relay 1 is: %d\n", pinValue);
  if (pinValue) 
    Serial.write(R1On, 4);
  else
    Serial.write(R1Off, 4);
}

BLYNK_WRITE(V3) {
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  //Serial.printf("Relay 2 is: %d\n", pinValue);
  if (pinValue) 
    Serial.write(R2On, 4);
  else
    Serial.write(R2Off, 4);
}

BLYNK_WRITE(V4) {
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  //Serial.printf("Relay 3 is: %d\n", pinValue);
  if (pinValue) 
    Serial.write(R3On, 4);
  else
    Serial.write(R3Off, 4);
}

BLYNK_WRITE(V5) {
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  //Serial.printf("Relay 4 is: %d\n", pinValue);
  if (pinValue) 
    Serial.write(R4On, 4);
  else
    Serial.write(R4Off, 4);
}

BLYNK_WRITE(V6) {
  TimeInputParam t(param); 
  if (t.hasStartTime()) {
    int Start_hour = t.getStartHour();
    int Start_minute = t.getStartMinute();
    int Start_sencond = t.getStartSecond();
    Serial.write(R1On, 4);
  }
  if (t.hasStopTime()) {
    int Stop_hour = t.getStopHour() ;
    int Stop_minute = t.getStopMinute() ;
    int Stop_sencond = t.getStopSecond();
  
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(CH_PD,OUTPUT);
  pinMode(RST,OUTPUT);
  pinMode(GPIO0,OUTPUT);
  digitalWrite(CH_PD,HIGH); //Setado em alto - funcionamento normal
  digitalWrite(RST,HIGH); //RST em alto - funcionamento normal
  digitalWrite(GPIO0,HIGH); //GPIO0 em alto - funcionamento no rmal

  BlynkEdgent.begin();
 
}

void loop() {
  BlynkEdgent.run();
}

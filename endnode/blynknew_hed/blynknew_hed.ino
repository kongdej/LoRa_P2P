#define BLYNK_TEMPLATE_ID "TMPLgb6AkxGS"
#define BLYNK_DEVICE_NAME "SMFhed"
#define BLYNK_FIRMWARE_VERSION        "0.1.1"
#define BLYNK_PRINT Serial
#define USE_WROVER_BOARD
#include "BlynkEdgent.h"

#include "DHT.h"
#define DHTPIN 15
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

 
BlynkTimer timer;

float sp = 60;
bool mode = 0; // 0 manual

BLYNK_WRITE(V10) {
  if (mode == 0) {
    int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
    
    Serial.print("value is: "); Serial.println(pinValue);
    digitalWrite(5, pinValue);
  }
}

BLYNK_WRITE(V11) {
  sp = param.asFloat(); // assigning incoming value from pin V1 to a variable
  Serial.print("value is: ");
  Serial.println(sp);
}

BLYNK_WRITE(V12) {
  mode = param.asInt(); // assigning incoming value from pin V1 to a variable
}

void myTimerEvent() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
 
  Serial.printf("t=%f, h=%f\n", t ,h);

  if (mode == 1) {
    if (h <= sp) {
      digitalWrite(5, HIGH);
      Blynk.virtualWrite(V10, 1);
    }
    else {
      digitalWrite(5, LOW);
      Blynk.virtualWrite(V10, 0);
    }
  }
  
  Blynk.virtualWrite(V0, t);
  Blynk.virtualWrite(V1, h);
}

void setup() {
  Serial.begin(9600);
  
  pinMode(5, OUTPUT);
 
  BlynkEdgent.begin();
  
  dht.begin();
  
  timer.setInterval(2000L, myTimerEvent);
}

void loop() {
  BlynkEdgent.run();
  timer.run();
}

// https://sgp1.blynk.cloud/external/api/update?token=YYlNjSzbXqW4NQHGViyyO6JZfhzszdif&V3=1



  

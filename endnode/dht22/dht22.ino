#include <SPI.h>
#include <LoRa.h>

#include "DHT.h"
#define DHTPIN  13
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6   //915E6
#define DCT     3*1000  // duty cycle time 3 sec. 
#define SF      7       // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab    // SyncWord: ranges from 0-0xFF, default 0x34

byte localAddress = 0x02;     // address of this device
byte destination = 0x00;      // destination to send to
long lastSend = 0;            // last send time
int interval = DCT;           // interval between sends
String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  Serial.println("LoRa Start..");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (true);
  }
  
  LoRa.setSpreadingFactor(SF); // ranges from 6-12,default 7 
  LoRa.setSyncWord(SW);  // ranges from 0-0xFF, default 0x34
  
  Serial.println("LoRa init succeeded.");
  
  dht.begin();
  Serial.println("DHT initiated");
}

void loop() {
  char buff[9];
  if (millis() - lastSend > interval) {
    lastSend = millis();            // timestamp the message
    float t = dht.readTemperature();
    float h = dht.readHumidity();
  
    if (isnan(h) || isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
    }
    else {
      Serial.println("temp="+String(t)+" hum="+String(h));   
      int temperature = t*10;
      int humidity = h*10;
      Serial.println("temp="+String(temperature)+" hum="+String(humidity));   
      sprintf(buff,"%04x%04x",temperature,humidity);
      sendMessage(buff);
      Serial.println("Sending "+String(buff));
    }
    interval = DCT + random(2000);     // 5-7 seconds
  }
}

void sendMessage(String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}

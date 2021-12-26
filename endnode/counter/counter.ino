#include <SPI.h>
#include <LoRa.h>

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6   //915E6
#define DCT     3*1000  // duty cycle time 3 sec. 
#define SF      7       // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab    // SyncWord: ranges from 0-0xFF, default 0x34

byte localAddress = 0x01;     // address of this device
byte destination = 0x00;      // destination to send to
long lastSend = 0;            // last send time
int interval = DCT;           // interval between sends
String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
uint16_t counter = 0;

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
}

void loop() {
  char buff[5];
  if (millis() - lastSend > interval) {
    lastSend = millis();            // timestamp the message
    counter++;
    sprintf(buff,"%04x",counter);
    sendMessage(buff);
    Serial.println("Sending "+String(buff));
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

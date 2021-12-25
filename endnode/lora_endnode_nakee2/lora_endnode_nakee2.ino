#include <SPI.h>
#include <LoRa.h>
#include <FlowerCare_BLE.h>

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6
#define DCT     30000   // duty cycle time 2 sec. 
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab   // SyncWord: ranges from 0-0xFF, default 0x34

#define FLORA_ADDR "C4:7C:8D:66:10:41"
#define PLANT FICUS_GINSEGN
FlowerCare* flora;
FC_RET_T ret;

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0x02;     // address of this device
byte destination = 0x00;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = DCT;          // interval between sends

void setup() {
  flora = new FlowerCare(FLORA_ADDR, PLANT);

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
  
  LoRa.onReceive(onReceive);
  LoRa.receive();
  
  Serial.println("LoRa init succeeded.");
}

void loop() {
  if (millis() - lastSendTime > interval) {
    lastSendTime = millis();            // timestamp the message
    Serial.print("Getting data from sensor");
    ret = flora->getData();
    while (ret != FLCARE_OK) {
      Serial.print(".");
      delay(1000);
      ret = flora->getData();
    }
    Serial.println("\nData acquired!");
    // print data
    Serial.print("Ambient temperature: ");
    Serial.print(flora->temp());
    Serial.println("°C");
    Serial.print("Soil moisture: ");
    Serial.print(flora->moist());
    Serial.println("%");
    Serial.print("Ambient light: ");
    Serial.print(flora->light());
    Serial.println("lux");
    Serial.print("Soil EC: ");
    Serial.print(flora->fert());
    Serial.println("us/cm");

    char buff[17];
    sprintf(buff,"%04x%04x%04x%04x",(int)(flora->temp()*10), flora->moist(), flora->light(), flora->fert());
    sendMessage(buff);
    Serial.println("Sending "+String(buff));
    interval = DCT + random(2000);     // 5-7 seconds
    LoRa.receive();                    // go back into receive mode
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

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String incoming = "";                 // payload of packet

  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             // skip rest of function
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }

  // if message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println();
}

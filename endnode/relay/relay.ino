#include <SPI.h>
#include <LoRa.h>

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6
#define DCT     9000   // duty cycle time 2 sec.
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab   // SyncWord: ranges from 0-0xFF, default 0x34

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0x0a;     // address of this device
byte destination = 0x00;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = DCT;          // interval between sends

int relays[] = {13,14};
 
void setup() {
  Serial.begin(9600);

  for (int i=0; i < 2; i++)
    pinMode(relays[i], OUTPUT); 
    
  while (!Serial);
  
  Serial.println("LoRa Node 1 Starting..");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initial OK!");
  Serial.println("Request to join!");
  
  Serial.println("join done");
  
  LoRa.setSpreadingFactor(SF); // ranges from 6-12,default 7 
  LoRa.setSyncWord(SW);  // ranges from 0-0xFF, default 0x34
  
  Serial.println("LoRa init succeeded.");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return;          // if there's no packet, return
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

  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message from " + String(sender,HEX) + " is not for me.");
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
  
  int rno = (int) strtol( &incoming.substring(0,2)[0], NULL, 16);
  int cmd = (int) strtol( &incoming.substring(2,4)[0], NULL, 16);
  if (sender == 0) {
    digitalWrite(relays[rno-1], cmd);
  }
}

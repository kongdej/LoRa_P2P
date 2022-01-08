#include <SPI.h>
#include <LoRa.h>
#include <SSD1306.h>
#include <CRC32.h>

#define BLYNK_TEMPLATE_ID "TMPLL5sfJiD7"
#define BLYNK_DEVICE_NAME "GATEWAY"
#define BLYNK_FIRMWARE_VERSION        "0.9.5"
#define BLYNK_PRINT Serial
#define APP_DEBUG

#include "BlynkEdgent.h"
#include <ThingsBoard.h>

byte localAddress = 0x00;     // address of this device


// Thing.egat.co.th -------------------------
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"
#define TOKEN "p21tc9nc6xvgmxzx6iei"

WiFiClient espClient;
ThingsBoard tb(espClient);
//---------------------------------------------

// LoRa -------------------------------------
#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  // 915E6
#define DCT     5000   // duty cycle time 2 sec. 
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab   // SyncWord: ranges from 0-0xFF, default 0x34
//-------------------------------------------

SSD1306  display(0x3c, 21, 22);

// Blynk -----------------------------------
BLYNK_CONNECTED() {
  Serial.println("Blynk connected");
}
//------------------------------------------


void setup() {
  Serial.begin(9600);
  delay(100);
  
  // OLED ---------------------------------------
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH);   // while OLED is running, must set GPIO16 in high
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  
  // LoRa --------------------------------------
  Serial.println("LoRa Gateway Start..");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (true);
  }
  
  LoRa.setSpreadingFactor(SF);  // ranges from 6-12,default 7 
  LoRa.setSyncWord(SW);         // ranges from 0-0xFF, default 0x34
  Serial.println("LoRa init succeeded.");
  display.drawString(10, 0, "LoRa Initial OK!");
  display.display();
  delay(1000);  
  //---------------------------------------------
  
  BlynkEdgent.begin();
  display.drawString(10, 20, "Blynk connected!");
  display.display();
  delay(1000);
}


void loop() { 
  //- Things board ------------------
  if(!tb.connected() && WiFi.status() == WL_CONNECTED ) {
      Serial.printf("Connecting to: %s ",THINGSBOARD_SERVER);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
        Serial.println("Failed to connect");
        display.drawString(0, 50, "** Things not connected!");
        display.display();
      }
   }  
  
  //- LoRa Received Packets -----------
  onReceive(LoRa.parsePacket()); 

  // Thingsboard -----
  tb.loop();

  // Blynk -----------
  BlynkEdgent.run();
}

void onReceive(int packetSize) {
  
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();             // sender address
  byte incomingMsgId = LoRa.read();      // incoming msg ID
  byte incomingLength = LoRa.read();     // incoming msg length

  String incoming = "";                    // payload of packet
  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }


  Serial.flush();
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println();
  
  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }

 
  // Calculate a checksum 
  String in_crc_str = incoming.substring(incoming.length()-8,incoming.length());
  
  CRC32 crc;  
  for (uint8_t i=0; i < incomingLength-8;i++) {
      crc.update(incoming[i]);  
  }
  uint32_t checksum    = crc.finalize();
  char checksum_str[9];
  itoa(checksum, checksum_str, 16);
  
  Serial.printf("Checksum: %s = %s\n", in_crc_str, checksum_str);  
  if (in_crc_str != String(checksum_str)) {
    Serial.println("Checksum Error.");
    return;          
  }
  
  
  display.clear();
  display.drawString(0, 0, "NID : "+ String(sender) + ", RSSI: "+ String( LoRa.packetRssi()));
  display.drawString(0, 12, "Msg : "+ String(incomingMsgId));
  display.drawString(0, 24, "Len : "+ String(incomingLength));
  if (incoming.length() > 15)  
    display.drawString(0, 36, "Data: "+ incoming.substring(0,15) + "..");
  else
    display.drawString(0, 36, "Data: "+ incoming);
  display.display();
  switch (sender) {
     case 0x02: 
          uint8_t id  = (int) strtol( &incoming.substring(0,2)[0], NULL, 16);
          float t     = (int) strtol( &incoming.substring(2,6)[0], NULL, 16)/10.;
          float h     = (int) strtol( &incoming.substring(6,10)[0], NULL, 16)/10.;
          Serial.printf("%d: t=%3.1f, h=%3.0f\n",id, t,h);
          Blynk.virtualWrite(V1, h);
          Blynk.virtualWrite(V2, t);
          Blynk.virtualWrite(V103, LoRa.packetRssi());
          tb.sendTelemetryFloat("T300", t);
          tb.sendTelemetryFloat("H300", h);
          break;          
  }  
}

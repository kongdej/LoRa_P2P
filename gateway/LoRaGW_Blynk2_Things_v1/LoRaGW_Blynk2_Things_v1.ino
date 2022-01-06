#include <SPI.h>
#include <LoRa.h>
#include <BlynkEdgent.h>

//#include <WiFi.h>
//#include <WiFiClient.h>
//#include <BlynkSimpleEsp32.h>
//#include <TimeLib.h>
//#include <WidgetRTC.h>
#include "SSD1306.h"
#include <ThingsBoard.h>

// Thing.egat.co.th -------------------------
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"
#define TOKEN "xFIDsmf35NN29CGZtJJ7"

WiFiClient espClient;
ThingsBoard tb(espClient);
//---------------------------------------------

// Blynk ------------------------------------
//#define BLYNK_PRINT Serial                         // Comment this out to disable prints and save space
//#define BLYNK_TEMPLATE_ID "TMPLTGDuOu5D"
//#define BLYNK_DEVICE_NAME "GATEWAY"
//#define BLYNK_AUTH_TOKEN  "R1Wgj_VrFdxTorBsr2s6h-0gLVwl7yN-"

#define BLYNK_TEMPLATE_ID       "TMPLTGDuOu5D"
#define BLYNK_DEVICE_NAME       "GATEWAY"
#define BLYNK_FIRMWARE_VERSION  "0.1.0"
#define BLYNK_PRINT Serial
#define APP_DEBUG

// Blynk token
char auth[] = BLYNK_AUTH_TOKEN;  

BlynkTimer timer;

//---------------------------------------------

// Wifi -------------------------------------
const char* ssid       = "ZAB";
const char* password   = "Gearman1";
//-------------------------------------------

// LoRa -------------------------------------
#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  // 915E6
#define DCT     5000   // duty cycle time 2 sec. 
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab   // SyncWord: ranges from 0-0xFF, default 0x34
//-------------------------------------------

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0x00;     // address of this device
//byte destination = 0xFF;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = DCT;          // interval between sends


bool subscribed = false;
byte currentMsgId[10],prevMsgId[10];

String incoming;
int  recipient;          // recipient address
byte sender;            // sender address
byte incomingMsgId;     // incoming msg ID
byte incomingLength;    // incoming msg length
String incomingClock;
int incomingRSSI;

// Helper macro to calculate array size, thingsboard
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

SSD1306  display(0x3c, 21, 22);

// Blynk -----------------------------------
BLYNK_CONNECTED() {
  Serial.println("Blynk connected");
}
//------------------------------------------

//*** SETUP ***********************************
void setup() {
  Serial.begin(9600);
  
  while (!Serial);
  
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

  wifiConnect(); 
  
}

//**** LOOP **********************************
void loop() {
  if (WiFi.status() != WL_CONNECTED && !tb.connected() && !Blynk.connected()) {
    wifiConnect();
  }
  else {
    tb.loop();
    Blynk.run();
    timer.run();
  }
  
  //- LoRa Received Packets -----------
  onReceive(LoRa.parsePacket());
}

void showDisplay() {
  display.clear();
  display.drawString(10, 0, incomingClock);
  display.drawString(0, 14, "NID : "+ String(sender) + ", RSSI: "+ incomingRSSI);
  display.drawString(0, 26, "Msg : "+ String(incomingMsgId));
  display.drawString(0, 38, "Len : "+ String(incomingLength));
  if (incoming.length() > 15)  
    display.drawString(0, 50, "Data: "+ incoming.substring(0,15) + "..");
  else
    display.drawString(0, 50, "Data: "+ incoming);
  display.display();
}

void sendMessage(byte destination, String outgoing) {  // destination 0xFF to all node
  while (LoRa.beginPacket() == 0) {
    Serial.print("waiting for radio ... ");
    delay(100);
  }
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
  char clockb[20];
  
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  recipient = LoRa.read();          // recipient address
  sender = LoRa.read();             // sender address
  incomingMsgId = LoRa.read();      // incoming msg ID
  incomingLength = LoRa.read();     // incoming msg length
  
  incoming = "";                    // payload of packet

  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }
  
  Serial.flush();
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println();
  
  sprintf(clockb,"%02d-%02d-%02d %02d:%02d:%02d",day(),month(),year(),hour(),minute(),second());
  incomingClock = String(clockb);
  incomingRSSI = LoRa.packetRssi();
  
  switch (sender) {
     case 0x01: 
          break;
          
  }
  Blynk.virtualWrite(V100, millis() / 1000);
  showDisplay();
}

void wifiConnect() { 
  //connect to WiFi ---------------------------
  Serial.printf("Connecting to %s ", ssid); 
  int status = WiFi.status();
  if ( status != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    display.clear();
    display.drawString(10, 0, "Connect to "+String(ssid) +"..");
    display.display();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
    display.clear();
    display.drawString(10, 0, "Connected -> "+String(ssid));
    display.display();
    delay(1000);

    //- Thing Board ----------------------------
    uint8_t retry = 10;
    while (!tb.connected() && retry) {          // Connect to the ThingsBoard
      Serial.print("Connecting to: ");Serial.print(THINGSBOARD_SERVER);Serial.print(" with token ");Serial.println(TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
        Serial.println("Failed to connect");
        retry--;
      }
      delay(500);
    }
    if (retry > 0) {
      display.drawString(10, 25, "TB connected!");
      Serial.println("TB connected.");
    }
    else {
      display.drawString(50, 25, "** TB not connected!");
      Serial.println("TB not connected.");
    }
    
    //Blynk connect -----------------------------
    retry = 10;
    Blynk.config(auth, "blynk.cloud", 80);  
    while (!Blynk.connect() && retry) { 
      Serial.println(".");
      delay(500);
      Blynk.config(auth, "blynk.cloud", 80); 
      retry--;
    }
    if (retry > 0) {
      display.drawString(10, 25, "Blynk connected!");
    }
    else {
      display.drawString(50, 25, "** Blynk not connected!");
    }
    
    display.drawString(10, 45, "Waiting a packet..");
    display.display();
  }
}

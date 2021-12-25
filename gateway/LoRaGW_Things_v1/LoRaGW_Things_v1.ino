#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "SSD1306.h"
//#include <ThingsBoard.h>
#include <PubSubClient.h>

// Thing.egat.co.th -------------------------
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"
#define TOKEN "xFIDsmf35NN29CGZtJJ7"

WiFiClient wifiClient;
//ThingsBoard tb(espClient);
PubSubClient client(wifiClient);
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
int  led_delay = 111;

// Helper macro to calculate array size, thingsboard
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

SSD1306  display(0x3c, 21, 22);
/*
//- Things.egat ----------------------------------------
RPC_Response processDelayChange(const RPC_Data &data) {
  led_delay = data;
  Serial.println("Received the set delay RPC method");
  Serial.print("Set new delay: "); Serial.println(led_delay);

  return RPC_Response(NULL, led_delay);
}

RPC_Response processGetDelay(const RPC_Data &data) {
  Serial.println("Received the get value method");

  return RPC_Response(NULL, led_delay);
}

int st = 0;
RPC_Response processCheckStatus(const RPC_Data &data) {
  Serial.println("Received the get checkstatus");
  
  return RPC_Response(NULL, st);
}

RPC_Response processSet(const RPC_Data &data) {
  char buff[11];
  Serial.print("Set relay ");
  st = data;
  Serial.println(st);
  sprintf(buff,"%02x%02x", 0x01, st); // relay no, on/off
  sendMessage(0x01, String(buff));
  
  return RPC_Response(NULL, st);
}

RPC_Callback callbacks[] = {
  { "setValue", processDelayChange },
  { "getValue", processGetDelay },
  { "checkStatus", processCheckStatus },
  { "setValue1", processSet },
  { "getValue1", processCheckStatus },
};
//---------------------------------------------
*/

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
  
  //connect to WiFi ---------------------------
   Serial.printf("Connecting to %s ", ssid);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
   }
   Serial.println(" CONNECTED");
   display.drawString(10, 15, "Wifi Connected!");
   display.display();
   delay(1000);
   client.setServer( THINGSBOARD_SERVER, 1883 );
       
}

//**** LOOP **********************************
int retry = 3;
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }
  /*
  //- Thing Board ----------------------------
  while (!tb.connected() && retry) {          // Connect to the ThingsBoard
    Serial.print("Connecting to: ");Serial.print(THINGSBOARD_SERVER);Serial.print(" with token ");Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      retry--;
    }
  } 
  
  if (!subscribed) {              // Subscribe for RPC, if needed
    Serial.println("Subscribing for RPC...");
    if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }
    Serial.println("Subscribe done");
    subscribed = true;
  }
  if (tb.connected()) {
    tb.loop();
  }
  */

  //- LoRa Received Packets -----------
  onReceive(LoRa.parsePacket());
}
/*
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
*/
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
  // if message is for this device, or broadcast, print details:
  
  Serial.flush();
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println();
}

void reconnect() {
  int status = WiFi.status();
  if ( status != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    display.clear();
    display.drawString(10, 0, "Reconnect Wifi..");
    display.display();
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to AP");
    display.clear();
    display.drawString(10, 0, "Connected to AP!");
    display.display();
    delay(3000);
  }
}
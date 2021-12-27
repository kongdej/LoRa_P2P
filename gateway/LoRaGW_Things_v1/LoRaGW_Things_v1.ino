#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ThingsBoard.h>
#include "SSD1306.h"

// Thing.egat.co.th -------------------------
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"
#define TOKEN "p21tc9nc6xvgmxzx6iei"

WiFiClient wifiClient;
ThingsBoard tb(wifiClient);
//---------------------------------------------

// Wifi -------------------------------------
#define WIFI_AP "ZAB"
#define WIFI_PASSWORD "Gearman1"

int status = WL_IDLE_STATUS;
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

byte gatewayId = 0x02;        // gateway id
byte localAddress = 0x00;     // address of this device
long lastSendTime = 0;        // last send time
int interval = DCT;           // interval between sends
String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages


bool subscribed = false;
byte currentMsgId[10],prevMsgId[10];

// Helper macro to calculate array size, thingsboard
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

SSD1306  display(0x3c, 21, 22);

//- Things.egat ----------------------------------------
RPC_Response processSet(const RPC_Data &data) {
  int d = data;
  Serial.print("Received the set  RPC method:"); Serial.println(d);
  return RPC_Response(NULL, 1);
}

RPC_Response processGet(const RPC_Data &data) {
  int d = data;
  Serial.println("Received the get value method:"); Serial.println(d);
  return RPC_Response(NULL, 1);
}

RPC_Callback callbacks[] = {
  { "setValue", processSet },
  { "getValue", processGet },
};
//---------------------------------------------

void reconnect() {
  showdisplay(0, 10, "Connecting to AP..", false);
  while (!tb.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
      showdisplay(0, 20, "-> Wifi Connected!", false);
    }
    Serial.print("Connecting to ThingsBoard node ...");
    showdisplay(0, 30, "Connecting to Tb..", false);
    if ( tb.connect(THINGSBOARD_SERVER, TOKEN) ) {
      Serial.println( "[DONE]" );
      showdisplay(0, 40, "-> Tb Connected!", false);
      
      if (!subscribed) {              // Subscribe for RPC, if needed
        Serial.println("Subscribing for RPC...");
        if (!tb.RPC_Subscribe(callbacks, COUNT_OF(callbacks))) {
          Serial.println("Failed to subscribe for RPC");
          return;
        }
        Serial.println("Subscribe done");
        subscribed = true;
      }
    } 
    else {
      Serial.print( "[FAILED]" );
      Serial.println( " : retrying in 5 seconds]" );
      delay( 5000 );
    }
  }
  showdisplay(0, 53, "Waiting a packet..", false);
}

void showdisplay(int col, int row, String text, bool clr) {
  if (clr) display.clear();
  display.drawString(col, row, text);
  display.display();
}

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
  
  // LoRa --------------------------------------
  Serial.println("LoRa Gateway Start..");
  showdisplay(0, 0, "LoRa Gateway Start..", true);
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (true);
  }
  
  LoRa.setSpreadingFactor(SF);  // ranges from 6-12,default 7 
  LoRa.setSyncWord(SW);         // ranges from 0-0xFF, default 0x34
  
  Serial.println("LoRa init succeeded.");
  showdisplay(0, 0, "LoRa Initial OK!", true);
  delay(1000);
}

//**** LOOP **********************************
void loop() {  
  if ( !tb.connected() ) {
    reconnect();
  }
  tb.loop();
  
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }

  //- LoRa Received Packets -----------
  onReceive(LoRa.parsePacket());
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
  
  int incomingRSSI = LoRa.packetRssi();
  
  Serial.flush();
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(incomingRSSI));
  Serial.println();
  
  
  char buff[13+incomingLength];
  sprintf(buff,"aa%02x%02x%02x%02x%02x%s",gatewayId,sender,incomingMsgId,incomingLength,-1*incomingRSSI,incoming);
  Serial.println("buff = "+String(buff));
  
  tb.sendTelemetryString("nakee", buff);
  
  memset(buff,0,sizeof(buff));
  
  display.clear();
  display.drawString(0, 14, "NID : "+ String(sender) + ", RSSI: "+ incomingRSSI);
  display.drawString(0, 26, "Msg : "+ String(incomingMsgId));
  display.drawString(0, 38, "Len : "+ String(incomingLength));
  if (incoming.length() > 15)  
    display.drawString(0, 50, "Data: "+ incoming.substring(0,15) + "..");
  else
    display.drawString(0, 50, "Data: "+ incoming);
  display.display();
    
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

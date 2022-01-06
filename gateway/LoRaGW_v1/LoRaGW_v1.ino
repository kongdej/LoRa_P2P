#include <SPI.h>
#include <LoRa.h>
#include "SSD1306.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ThingsBoard.h>

// Wifi -------------------------------------
const char* ssid       = "EGAT-IoT";
const char* password   = "";
//-------------------------------------------

// Thing.egat.co.th -------------------------
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"
#define TOKEN "xFIDsmf35NN29CGZtJJ7"

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

byte gatewayId = 0x03;        // gateway id
byte localAddress = 0x00;     // address of this device
long lastSendTime = 0;        // last send time
int interval = DCT;           // interval between sends
uint8_t retry;
SSD1306  display(0x3c, 21, 22);

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
  
  //connect to WiFi ---------------------------
   Serial.printf("Connecting to %s ", ssid);
   WiFi.begin(ssid, password);
   retry = 20;
   while (WiFi.status() != WL_CONNECTED && retry) {
     delay(500);
     Serial.print(".");
     retry--;
   }
   if(WiFi.status() == WL_CONNECTED) {
    Serial.println(" CONNECTED");
    display.drawString(0, 15, "Wifi Connected!");
   }
   else {
    Serial.println(" Not CONNECTED");
    display.drawString(0, 15, "Wifi not connected!"); 
   }
   display.display();
   delay(1000);     
}

//**** LOOP **********************************
void loop() {  
  //- LoRa Received Packets -----------
  onReceive(LoRa.parsePacket());

  if (!tb.connected() && WiFi.status() == WL_CONNECTED) {
    // Connect to the ThingsBoard
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN)) {
      Serial.println("Failed to connect");
      return;
    }
  }
  
  tb.loop();
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
  
  
  
  char header[13];
  char buf[13+incomingLength];
  
  sprintf(header,"aa%02x%02x%02x%02x%02x",gatewayId,sender,incomingMsgId,incomingLength,-1*incomingRSSI);
  String payload = String(header)+incoming;
  payload.toCharArray(buf, 13+incomingLength);
  Serial.println("payload -> " + String(buf));
  Serial.println();
    
  display.clear();
  display.drawString(0, 0, "NID : "+ String(sender) + ", RSSI: "+ incomingRSSI+ " -> "+ String(incomingMsgId));
  int wsi,wsii,wdi,wdii,ti,hi,sm,st,sec,v,i,r;
  wsi  = (int) strtol( &incoming.substring(0,4)[0], NULL, 16);
  wsii = (int) strtol( &incoming.substring(4,8)[0], NULL, 16);
  wdii = (int) strtol( &incoming.substring(8,12)[0], NULL, 16);
  wdi  = (int) strtol( &incoming.substring(12,16)[0], NULL, 16);
  ti   = (int) strtol( &incoming.substring(16,20)[0], NULL, 16);
  hi   = (int) strtol( &incoming.substring(20,24)[0], NULL, 16);
  sm   = (int) strtol( &incoming.substring(24,28)[0], NULL, 16);
  st   = (int) strtol( &incoming.substring(28,32)[0], NULL, 16);
  sec  = (int) strtol( &incoming.substring(32,36)[0], NULL, 16);
  v    = (int) strtol( &incoming.substring(36,40)[0], NULL, 16);
  i    = (int) strtol( &incoming.substring(40,44)[0], NULL, 16);
  r    = (int) strtol( &incoming.substring(44,45)[0], NULL, 16);
  
  tb.sendTelemetryFloat("WS301", wsi/10.);
  tb.sendTelemetryFloat("WD301", wdi);
  tb.sendTelemetryFloat("T301", ti/10.);
  tb.sendTelemetryFloat("H301", hi/10.);
  tb.sendTelemetryFloat("SM301", sm/10.);
  tb.sendTelemetryFloat("ST301", st/10.);
  tb.sendTelemetryFloat("EC301", sec);
  tb.sendTelemetryFloat("V301", v/10.);
  tb.sendTelemetryFloat("I301", i/10.);
  tb.sendTelemetryFloat("R301", r);
  
  display.drawString(0, 10, "wind: "+ String(wsi/10.) + ", "+ String(wdi));  
  display.drawString(0, 20, "T: "+ String(ti/10.) + ", H: "+ String(hi/10.));  
  display.drawString(0, 30, "M: "+ String(sm/10.) + ", T: "+ String(st/10.) + ", EC: "+ String(sec) );  
  display.drawString(0, 40, "V: "+ String(v/10.) + ", I: "+ String(i/10.) + ", R: "+ String(r) );  
  display.display();
  Serial.println("wind : "+ String(wsi/10.) + ", "+ String(wdi));
  Serial.println("T: "+ String(ti/10.) + ", H: "+ String(hi/10.));
    
}

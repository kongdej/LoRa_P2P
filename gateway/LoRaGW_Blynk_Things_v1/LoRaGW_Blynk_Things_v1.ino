#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include "SSD1306.h"
#include <ThingsBoard.h>

// Thing.egat.co.th -------------------------
#define THINGSBOARD_SERVER  "mqtt.egat.co.th"
#define TOKEN "xFIDsmf35NN29CGZtJJ7"

WiFiClient espClient;
ThingsBoard tb(espClient);
//---------------------------------------------

// Blynk ------------------------------------
#define BLYNK_PRINT Serial                         // Comment this out to disable prints and save space
#define BLYNK_TEMPLATE_ID "TMPLFAzXo6-B"
#define BLYNK_DEVICE_NAME "LoraGateway"

// Blynk token
//char auth[] = "Zcu3M0ogYJo8QzEexSD9GB7PGY1achb2";  
char auth[] = "ulcMeErLFzjLoVNcPw0DSc_uNCr6xPjG";  
char blynk_host[] = "kongdej.trueddns.com";
int blynk_port = 40949;

BlynkTimer timer;
WidgetRTC rtc;
WidgetLED led1(V111);
WidgetLED led2(V112);
WidgetLED led3(V113);

//---------------------------------------------

// Wifi -------------------------------------
const char* ssid       = "ABZ";
const char* password   = "gearman1";
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
int  led_delay = 111;

// Helper macro to calculate array size, thingsboard
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

SSD1306  display(0x3c, 21, 22);

// Blynk -----------------------------------

BLYNK_CONNECTED() {
  rtc.begin();
}

BLYNK_WRITE(V1) {
  char buff[11];
  int pinValue = param.asInt();
  Serial.print("V1  value is: ");Serial.println(pinValue);
  sprintf(buff,"%08x%02x", 0x01, pinValue); // relay no, on/off
  sendMessage(0x01, String(buff)); //%02x;  
}

void clockDisplay() {
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + " " + month() + " " + year();
  Serial.print("Current time: ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.print(currentDate);
  Serial.println();
  Blynk.virtualWrite(V3, currentTime);
  Blynk.virtualWrite(V4, currentDate);
}

//------------------------------------------

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
       
   //Blynk connect -----------------------------
    Blynk.config(auth,blynk_host,blynk_port);  // in place of Blynk.begin(auth, ssid, pass);
    int mytimeout = millis() / 1000;
    while (Blynk.connect() == false) { // try to connect to server for 10 seconds
      Serial.println(".");
      if((millis() / 1000) > mytimeout + 8){ // try local server if not connected within 9 seconds
         display.drawString(50, 25, "** not connected!");
         break;
      }
    }
    display.drawString(10, 25, "Blynk connected!");
    display.drawString(10, 45, "Waiting a packet..");
    display.display();
        
    // RTC blynk
    setSyncInterval(10 * 60); // Sync interval in seconds (10 minutes)
  
    // Timer 
    timer.setInterval(10000L, clockDisplay); // Display digital clock every 10 seconds
    timer.setInterval(300*1000L, TimerSend); // 5 minute check node 
}

//**** LOOP **********************************
int retry = 3;
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnect();
    return;
  }
  
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

  //- Blynk ----------------------------
  if (Blynk.connected()) {
    Blynk.run();
    timer.run();
  }

  //- LoRa Received Packets -----------
  onReceive(LoRa.parsePacket());
}


void TimerSend() {
  for (int i=1; i < 4; i++) {
    if ( currentMsgId[i] == prevMsgId[i]) {
      Serial.println(String(currentMsgId[i])+"-"+String(prevMsgId[i])+" -> "+String(i) + " Down!");
      if (i == 1) led1.setValue(50);
      if (i == 2) led2.setValue(50);
      if (i == 3) led3.setValue(50);
    }
    else {
      prevMsgId[i] = currentMsgId[i];
      if (i == 1) led1.on();
      if (i == 2) led2.on();
      if (i == 3) led3.on();
    }
  }
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
  // if message is for this device, or broadcast, print details:
  
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
  
  float t,h,soiltemp;
  int r,no;
  int moist,light,fert;
  switch (sender) {
    case 0x01:
          t = (int) strtol( &incoming.substring(0,4)[0], NULL, 16)/10.0; 
          h = (int) strtol( &incoming.substring(4,8)[0], NULL, 16)/10.0;
          r = (int) strtol( &incoming.substring(8,10)[0], NULL, 16);
          Blynk.virtualWrite(V10, t);
          Blynk.virtualWrite(V11, h);
          Blynk.virtualWrite(V101, LoRa.packetRssi());
          tb.sendTelemetryFloat("T001-01", t);
          tb.sendTelemetryFloat("H001-01", h);
          currentMsgId[sender] = incomingMsgId;
          break;
          
    case 0x02:
          soiltemp = (int) strtol( &incoming.substring(0,4)[0], NULL, 16)/10.;
          moist    = (int) strtol( &incoming.substring(4,8)[0], NULL, 16);
          light    = (int) strtol( &incoming.substring(8,12)[0], NULL, 16);
          fert     = (int) strtol( &incoming.substring(12,16)[0], NULL, 16);
          
          Blynk.virtualWrite(V20, soiltemp);
          Blynk.virtualWrite(V21, moist);
          Blynk.virtualWrite(V22, light);
          Blynk.virtualWrite(V23, fert);
          Blynk.virtualWrite(V102, LoRa.packetRssi());
          tb.sendTelemetryFloat("T002-01", soiltemp);
          tb.sendTelemetryFloat("M002-01", moist);
          tb.sendTelemetryFloat("L002-01", light);
          tb.sendTelemetryFloat("F002-01", fert);
          currentMsgId[sender] = incomingMsgId;
          break;
          
    case 0x03:
          no = (int) strtol( &incoming.substring(0,2)[0], NULL, 16);
          t = (int) strtol( &incoming.substring(2,6)[0], NULL, 16)/10.;
          h = (int) strtol( &incoming.substring(6,10)[0], NULL, 16)/10.;
          Blynk.virtualWrite(V30, t);
          Blynk.virtualWrite(V31, h);
          Blynk.virtualWrite(V103, LoRa.packetRssi());
          tb.sendTelemetryFloat("T003-01", t);
          tb.sendTelemetryFloat("H003-01", h);
          currentMsgId[sender] = incomingMsgId;
          break;
     
     case 0x05: 
          int windspeed2_i, windspeed2_index, winddir2_index, winddir2, temperature2_i, humidity2_i,moisture2_i,soiltemp_i,soilec,volt2_i,current2_i;
          float windspeed2,temperature2,humidity2,moisture2,soiltemp,volt2,current2;
          windspeed2_i = (int) strtol( &incoming.substring(0,4)[0], NULL, 16); windspeed2 = windspeed2_i/10.0;
          windspeed2_index = (int) strtol( &incoming.substring(4,8)[0], NULL, 16);
          winddir2_index = (int) strtol( &incoming.substring(8,12)[0], NULL, 16);
          winddir2 = (int) strtol( &incoming.substring(12,16)[0], NULL, 16); 
          temperature2_i = (int) strtol( &incoming.substring(16,20)[0], NULL, 16); temperature2 = temperature2_i/10.0;
          humidity2_i = (int) strtol( &incoming.substring(20,24)[0], NULL, 16); humidity2 = humidity2_i/10.0;
          moisture2_i = (int) strtol( &incoming.substring(24,28)[0], NULL, 16); moisture2 = moisture2_i/10.0;
          soiltemp_i = (int) strtol( &incoming.substring(28,32)[0], NULL, 16); soiltemp = soiltemp_i/10.0;
          soilec = (int) strtol( &incoming.substring(32,36)[0], NULL, 16); 
          volt2_i = (int) strtol( &incoming.substring(36,40)[0], NULL, 16); volt2 = volt2_i/10.0;
          current2_i = (int) strtol( &incoming.substring(40,44)[0], NULL, 16); current2 = current2_i/10.0;

          Serial.println("wind speed: "+String(windspeed2));
          Serial.println("wind index: "+String(windspeed2_index));
          Serial.println("windir index: "+String(winddir2_index));
          Serial.println("windir: "+String(winddir2));
          Serial.println("temperature2: "+String(temperature2));
          Serial.println("humidity2: "+String(humidity2));
          Serial.println("moisture: "+String(moisture2));
          Serial.println("soiltemp: "+String(soiltemp));
          Serial.println("ec: "+String(soilec));
          Serial.println("volt: "+String(volt2));
          Serial.println("current: "+String(current2));
          
          Blynk.virtualWrite(V50, windspeed2);        tb.sendTelemetryFloat("X005-01", windspeed2);
          Blynk.virtualWrite(V51, windspeed2_index);  tb.sendTelemetryFloat("G005-01", windspeed2_index);
          Blynk.virtualWrite(V52, winddir2_index);    tb.sendTelemetryFloat("F005-01", winddir2_index);
          Blynk.virtualWrite(V53, winddir2);          tb.sendTelemetryFloat("W005-01", winddir2);
          Blynk.virtualWrite(V54, temperature2);       tb.sendTelemetryFloat("T005-01", temperature2);
          Blynk.virtualWrite(V55, humidity2);        tb.sendTelemetryFloat("H005-01", humidity2);
          Blynk.virtualWrite(V56, moisture2);         tb.sendTelemetryFloat("M005-01", moisture2);
          Blynk.virtualWrite(V57, soiltemp);          tb.sendTelemetryFloat("U005-01", soiltemp);
          Blynk.virtualWrite(V58, soilec);            tb.sendTelemetryFloat("E005-01", soilec);
          Blynk.virtualWrite(V59, volt2);             tb.sendTelemetryFloat("V005-01", volt2);
          Blynk.virtualWrite(V60, current2);          tb.sendTelemetryFloat("I005-01", current2);
          
          break;
          
     case 0x04:
          int pm2_5,pm10,winddir,windspeed_i,temperature_i,humidity_i,volt_i,current_i;
          float windspeed,temperature,humidity,pressure,altitude,light,volt,current;
          unsigned long pressure_i,altitude_i,light_i;
          pm2_5 = (int) strtol( &incoming.substring(0,4)[0], NULL, 16);
          pm10 = (int) strtol( &incoming.substring(4,8)[0], NULL, 16);
          winddir = (int) strtol( &incoming.substring(8,12)[0], NULL, 16);
          windspeed_i = (int) strtol( &incoming.substring(12,16)[0], NULL, 16);  windspeed = windspeed_i/10.0;
          temperature_i = (int) strtol( &incoming.substring(16,20)[0], NULL, 16);  temperature = temperature_i/100.0;
          humidity_i = (int) strtol( &incoming.substring(20,24)[0], NULL, 16);  humidity = humidity_i/100.0;
          pressure_i =  strtol( &incoming.substring(24,32)[0], NULL, 16);  pressure = pressure_i/100.0;
          altitude_i =  strtol( &incoming.substring(32,40)[0], NULL, 16);  altitude = altitude_i/100.0;
          light_i =  strtol( &incoming.substring(40,48)[0], NULL, 16);  light = light_i/100.0;
          volt_i = (int) strtol( &incoming.substring(48,52)[0], NULL, 16);  volt = volt_i/10.0;
          current_i = (int) strtol( &incoming.substring(52,56)[0], NULL, 16);  current = current_i/10.0;
          Serial.println("pm25: "+String(pm2_5));
          Serial.println("pm10: "+String(pm10));
          Serial.println("wdir: "+String(winddir));
          Serial.println("wspd: "+String(windspeed));
          Serial.println("temp: "+String(temperature));
          Serial.println("humi: "+String(humidity));
          Serial.println("pssr: "+String(pressure));
          Serial.println("altd: "+String(altitude));
          Serial.println("luxx: "+String(light));
          Serial.println("volt: "+String(volt));
          Serial.println("curr: "+String(current));
          
          Blynk.virtualWrite(V40, pm2_5);       tb.sendTelemetryFloat("X004-01", pm2_5);
          Blynk.virtualWrite(V41, pm10);        tb.sendTelemetryFloat("Z004-01", pm10);
          Blynk.virtualWrite(V42, winddir);     tb.sendTelemetryFloat("D004-01", winddir);
          Blynk.virtualWrite(V43, windspeed);   tb.sendTelemetryFloat("W004-01", windspeed);
          Blynk.virtualWrite(V44, temperature); tb.sendTelemetryFloat("T004-01", temperature);
          Blynk.virtualWrite(V45, humidity);    tb.sendTelemetryFloat("H004-01", humidity);
          Blynk.virtualWrite(V46, pressure);    tb.sendTelemetryFloat("P004-01", pressure);
          Blynk.virtualWrite(V47, altitude);    tb.sendTelemetryFloat("A004-01", altitude);
          Blynk.virtualWrite(V48, light);       tb.sendTelemetryFloat("L004-01", light);
          Blynk.virtualWrite(V49, volt);        tb.sendTelemetryFloat("V004-01", volt);
          Blynk.virtualWrite(V50, current);     tb.sendTelemetryFloat("I004-01", current);
          break; 
          
  }
  Blynk.virtualWrite(V100, millis() / 1000);
  showDisplay();
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

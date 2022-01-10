#include <SPI.h>
#include <LoRa.h>
#include "ModbusMaster.h"
#include <Wire.h>

#include "PMS.h"

PMS pms(Serial);
PMS::DATA data;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR  byte msgCount = 0;

#define MAX485_RE_NEG  4
#define HT_ID     0x02
#define WS_ID     0x06
#define WD_ID     0x04
#define SOIL_ID   0x08
#define RX_PIN    16 //RX2 
#define TX_PIN    17  //TX2 
#define RAIN_PIN  13

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6  //915E6
#define DCT     5000   // duty cycle time 2 sec.
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab   // SyncWord: ranges from 0-0xFF, default 0x34

String outgoing;              // outgoing message
//byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0x05;     // address of this device
byte destination = 0x00;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = DCT;          // interval between sends

uint8_t result;
int t[9] = {};

ModbusMaster ht,soil,wd,ws;

// Volt&Amp
#include <Adafruit_INA219.h>
Adafruit_INA219 ina219;
 
void preTransmission() {
  digitalWrite(MAX485_RE_NEG, HIGH); //Switch to transmit data
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, LOW); //Switch to receive data
}

void sendMessages() {  
  // Wind Speed
  delay(200);
  ws.readHoldingRegisters(0, 2);
  if (result == ws.ku8MBSuccess) {
    t[0] = ws.getResponseBuffer(0); //speed
    t[1] = ws.getResponseBuffer(1); // index
    Serial.print("Wind Speed: ");Serial.print(String(t[0]/10.0)); Serial.print("-");Serial.println(t[1]);
  }
  
  // Wind Direction
  delay(200);
  wd.readHoldingRegisters(0, 2);
  if (result == wd.ku8MBSuccess) {
    t[2] = wd.getResponseBuffer(0); // direction index
    t[3] = wd.getResponseBuffer(1); // degree
    Serial.print("Wind Direction: ");Serial.print(t[2]); Serial.print("-");Serial.println(t[3]);
  }
  
  // Temperature & Humidiy
  delay(200);
  ht.readHoldingRegisters(0, 2);
  if (result == ht.ku8MBSuccess) {
    t[4] = ht.getResponseBuffer(0);
    t[5] = ht.getResponseBuffer(1);
   
   Serial.println("Temp = " + String(t[4]/10.));
   Serial.println("Hump = " + String(t[5]/10.));
  }


  // Soil : Moisture,Temperature,EC -----------
  delay(200);
  soil.readHoldingRegisters(0, 3);
  if (result == soil.ku8MBSuccess) {
    t[6] = soil.getResponseBuffer(0);
    t[7] = soil.getResponseBuffer(1);
    t[8] = soil.getResponseBuffer(2);
    Serial.print("Moisture: ");Serial.println(t[6]/10.0);
    Serial.print("Temp: ");Serial.println(t[7]/10.0);
    Serial.print("EC: ");Serial.println(t[8]);
  }
  
 
  // INA219 voltage and current --------------
  delay(200);
  float shuntvoltage = ina219.getShuntVoltage_mV();
  float busvoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  float loadvoltage = busvoltage + (shuntvoltage / 1000);
  Serial.println("Volt = " + String(loadvoltage));
  Serial.println("Imp = " + String(current_mA));
  

  // Rain sensor -------------------------------
  delay(200);
  int rain = digitalRead(RAIN_PIN);
  Serial.println("Rain = " + String(rain));
  
  Serial.println();
  
  char buff[50];
  sprintf(buff,"%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%01x", t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], (int)(loadvoltage*10), (int)(current_mA*10), (byte)rain );
  Serial.print("Sending ");Serial.println(buff);
  Serial.println();
  String outgoing = String(buff);
  
  LoRa.beginPacket();                   // start packet
    LoRa.write(destination);              // add destination address
    LoRa.write(localAddress);             // add sender address
    LoRa.write(msgCount);                 // add message ID
    LoRa.write(outgoing.length());        // add payload length
    LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  
  msgCount++;         
  
}
uint8_t retry = 3;

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  Serial1.begin(9600, SERIAL_8N1, 12, 35);
  
  pinMode(RAIN_PIN, INPUT_PULLUP);
  
  if (pms.read(data)) {
    Serial.print("PM 1.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_1_0);

    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5);

    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_10_0);

    Serial.println();
  }
  
  // Deep Sleep --------------------------
  Serial.println();
  print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +" Seconds");

/*
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
    // Initialize I2C bus
  Wire.begin();
  Wire.setClock(400000);

  // INA219 -----------------------------
  while (! ina219.begin() && retry) {
    Serial.println("Failed to find INA219 chip .. " + String(retry));
    delay(1000);
    retry--;
  }

  // Modbus -----------------------------
  pinMode(MAX485_RE_NEG, OUTPUT);
  digitalWrite(MAX485_RE_NEG, LOW);
  
  ht.begin(HT_ID, Serial2);
  ht.preTransmission(preTransmission);
  ht.postTransmission(postTransmission);

  ws.begin(WS_ID, Serial2);
  ws.preTransmission(preTransmission);
  ws.postTransmission(postTransmission);
  
  wd.begin(WD_ID, Serial2);
  wd.preTransmission(preTransmission);
  wd.postTransmission(postTransmission);
  
  soil.begin(SOIL_ID, Serial2);
  soil.preTransmission(preTransmission);
  soil.postTransmission(postTransmission);
   
  Serial.println("LoRa Node 1 Starting..");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initial OK!");  
  LoRa.setSpreadingFactor(SF); // ranges from 6-12,default 7 
  LoRa.setSyncWord(SW);  // ranges from 0-0xFF, default 0x34
  Serial.println("LoRa init succeeded.");

  sendMessages();
  */
  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush(); 
  esp_deep_sleep_start();
  Serial.println("This will never be printed");

}

void loop() {
}

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

#include <esp_now.h>
#include <WiFi.h>

#include "PMS.h"
PMS pms(Serial2);
PMS::DATA data;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR  byte msgCount = 0;

//84:CC:A8:7E:BE:A4
uint8_t broadcastAddress[] = {0x08, 0x3A, 0xF2, 0x66, 0x9C, 0x34};  // ttgo  - 08:3A:F2:66:9C:34
typedef struct struct_message {
    uint16_t pm1;
    uint16_t pm25;
    uint16_t pm10;
} struct_message;

struct_message Payload;

String success;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}

long lastSend = 0;            // last send time
int interval = 3*1000;        // interval between sends

void setup() {
  Serial.begin(9600);   
  Serial2.begin(9600);  
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Mac = "); Serial.println(WiFi.macAddress());
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
   // Deep Sleep --------------------------
  Serial.println();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +" Seconds");
}

void loop() {
  if (pms.read(data)) {
    Serial.print("PM 1.0 (ug/m3): ");Serial.println(data.PM_AE_UG_1_0);
    Serial.print("PM 2.5 (ug/m3): ");Serial.println(data.PM_AE_UG_2_5); 
    Serial.print("PM 10.0 (ug/m3): ");Serial.println(data.PM_AE_UG_10_0);
    Serial.println();
    Payload.pm1 = data.PM_AE_UG_1_0;
    Payload.pm25 = data.PM_AE_UG_2_5;
    Payload.pm10 = data.PM_AE_UG_10_0;
    
    // Send message via ESP-NOW  
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &Payload, sizeof(Payload));   
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    
    Serial.println("Going to sleep now");
    delay(1000);
    Serial.flush(); 
    esp_deep_sleep_start();
    Serial.println("This will never be printed");
  }
}

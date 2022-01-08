#include <esp_now.h>
#include <WiFi.h>
#include "DHT.h"

#define DHTPIN  4
#define DHTTYPE DHT21

DHT dht(DHTPIN, DHTTYPE);

// REPLACE WITH THE MAC Address of your receiver 
uint8_t broadcastAddress[] = {0x08, 0x3A, 0xF2, 0x66, 0x9C, 0x34};  // ttgo  - 08:3A:F2:66:9C:34
//uint8_t broadcastAddress[] = {0x84, 0xCC, 0xA8, 0x7E, 0xBE, 0xA4};  // esp32 - 84:CC:A8:7E:BE:A4

typedef struct struct_message {
    uint8_t id;
    float temp;
    float hum;
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
  while (!Serial);
  
  dht.begin();
  Serial.println("DHT initiated");
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

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
}
 
void loop() {
  if (millis() - lastSend > interval) {
    lastSend = millis();
    Payload.id = 1;
    Payload.temp = dht.readTemperature();
    Payload.hum = dht.readHumidity();
    
    Serial.printf("%d: temp=%f, hum=%f\n",Payload.id, Payload.temp, Payload.hum);   
      
    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &Payload, sizeof(Payload));
   
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
  }

}

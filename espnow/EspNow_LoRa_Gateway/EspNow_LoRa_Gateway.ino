#include <SPI.h>
#include <LoRa.h>
#include <CRC32.h>
#include <esp_now.h>
#include <WiFi.h>

#define SS      18
#define RST     14
#define DI0     26
#define BAND    923E6   //915E6
#define DCT     3*1000  // duty cycle time 3 sec. 
#define SF      7       // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab    // SyncWord: ranges from 0-0xFF, default 0x34

byte localAddress = 0x02;     // espnow gateway address
byte destination = 0x00;      // destination to send to gateway
byte msgCount = 0;            // count of outgoing messages
String success;

// REPLACE WITH THE MAC Address of your receiver 
// mac -> 08:3A:F2:66:9C:34
// Node1 --------------------------------------------------------
uint8_t node1macAddress[] = {0x84, 0xCC, 0xA8, 0x7E, 0xBE, 0xA4};  // esp32 - 84:CC:A8:7E:BE:A4
String  node1macStr = "84:cc:a8:7e:be:a4";

typedef struct struct_message {
    uint8_t id;
    float   temp;
    float   hum;
} struct_message;
struct_message node1Payload;
//-----------------------------------------------------------------

// Callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  CRC32 crc;  
  char address[18];
  char buff[200];

  snprintf(address, sizeof(address), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println();
  Serial.printf("Recv from: %s\n",address);
  
  if (String(address) == node1macStr) {
    Serial.print("Bytes received: "); Serial.println(len);
    
    //- get payload-------------------------
    memcpy(&node1Payload, incomingData, sizeof(node1Payload));  
    uint8_t id = node1Payload.id;
    float incomingTemp = node1Payload.temp;
    float incomingHum  = node1Payload.hum;
    Serial.printf("id=%d, temp=%f, hum=%f\n", id, incomingTemp, incomingHum);   
  
    int temperature = incomingTemp*10;
    int humidity = incomingHum*10;

    //-- payload = id/temp/hum/crc----
    sprintf(buff,"%02x%04x%04x", node1Payload.id,temperature, humidity);
    
    // calculate CRC32
    for (size_t i = 0; i < 10; i++) { //(*) payload length = 10
      crc.update(buff[i]);
    }
    sendMessage(buff, crc.finalize()); 
    //--------------------------------- 
  }
}

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

//-SETUP--------------------------------------------------
void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  Serial.println("LoRa Start..");
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (true);
  }
  
  LoRa.setSpreadingFactor(SF); // ranges from 6-12,default 7 
  LoRa.setSyncWord(SW);  // ranges from 0-0xFF, default 0x34
  
  Serial.println("LoRa init succeeded.");

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, node1macAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv);
}

//-LOOP------------------------------------
void loop() {
}
//-----------------------------------------

void sendMessage(char incoming[200], uint32_t checksum) {
  char buff[200];
  String outgoing;
  
  Serial.printf("Checksum = %0x ",checksum);
  sprintf(buff,"%s%08x", incoming, checksum);
  Serial.println("Sending "+String(buff));
  outgoing = String(buff);
  
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}

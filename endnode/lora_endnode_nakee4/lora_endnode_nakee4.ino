 /*
  * Nakee Weather Station 2022
  */  
#include <LowPower.h>
#include <SPI.h>
#include <LoRa.h>
#include <SoftwareSerial.h>
#include <Wire.h>


// Lora 
#define BAND    923E6  //915E6
#define DCT     5      // duty cycle time n sec. 
#define SF      7      // SpreadingFactor: ranges from 6-12,default 7 
#define SW      0xab   // SyncWord: ranges from 0-0xFF, default 0x34

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0x04;     // ** address of this device
byte destination = 0x00;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = DCT;           // interval between sends

#define PM25RX    7   // RX
#define WINDDIR   A3  //
#define WINDSPEED A0  //

// Light
#include <BH1750.h>
BH1750 lightMeter;

// Volt&Amp
#include <Adafruit_INA219.h>
Adafruit_INA219 ina219;
//float voltageConversionConstant = .004882814; //This constant maps the value provided from the analog read function,


// BMP280, temperature, humiditypressure
#include <ErriezBMX280.h>
#define SEA_LEVEL_PRESSURE_HPA      1026.25  // Adjust sea level for altitude calculation
ErriezBMX280 bmx280 = ErriezBMX280(0x76);    // I2C address 0x76 or 0x77

// PMS5003, PM2.5 PM10
SoftwareSerial pmSerial(PM25RX, 5); // RX, TX
unsigned int pm1 = 0;
unsigned int pm2_5 = 0;
unsigned int pm10 = 0;

const int windspeedPin = WINDSPEED ;
float total_mA=0;
float total_sec=0;

uint8_t retry = 3;

void setup() {
  pmSerial.begin(9600);
  Serial.begin(9600);
  while (!Serial);
}

void loop() {
  
  // PMS5003 --------------
  
  // LoRa -----------------
  Serial.println("LoRa Start..");
  while (!LoRa.begin(BAND) && retry) {
    Serial.println("Starting LoRa failed! ..  "+ String(retry));
    retry--;
  }
  if (retry) {
    LoRa.setSpreadingFactor(SF); // ranges from 6-12,default 7 
    LoRa.setSyncWord(SW);  // ranges from 0-0xFF, default 0x34
    Serial.println("LoRa init succeeded.");
  }
  
  // Initialize I2C bus
  Wire.begin();
  Wire.setClock(400000);
  
  // BMP280 ----------------
  retry = 3;  
  while (!bmx280.begin() && retry) {
     Serial.println("Error: Could not detect sensor .. " + String(retry));
     delay(3000);
     retry--; 
  }
  if (retry) {    
    Serial.print(F("\nSensor type: "));
    switch (bmx280.getChipID()) {
        case CHIP_ID_BMP280:
            Serial.println(F("BMP280\n"));
            break;
        case CHIP_ID_BME280:
            Serial.println(F("BME280\n"));
            break;
        default:
            Serial.println(F("Unknown\n"));
            break;
    }
    bmx280.setSampling(BMX280_MODE_NORMAL,    // SLEEP, FORCED, NORMAL
                       BMX280_SAMPLING_X16,   // Temp:  NONE, X1, X2, X4, X8, X16
                       BMX280_SAMPLING_X16,   // Press: NONE, X1, X2, X4, X8, X16
                       BMX280_SAMPLING_X16,   // Hum:   NONE, X1, X2, X4, X8, X16 (BME280)
                       BMX280_FILTER_X16,     // OFF, X2, X4, X8, X16
                       BMX280_STANDBY_MS_500);// 0_5, 10, 20, 62_5, 125, 250, 500, 1000
  }

  // Light BH1700 ------------------------
   lightMeter.begin();
   
  // INA219 -----------------------------
  retry = 3;
  while (! ina219.begin() && retry) {
    Serial.println("Failed to find INA219 chip .. " + String(retry));
    delay(1000);
    retry--;
  }
 
    // wind direction ---------------------
    int winddirection_raw = analogRead(WINDDIR);
    int winddir = map(winddirection_raw, 200, 1023, 0, 360);
    Serial.print("Wind Direction = "); Serial.println(winddir);
    
    // wind speed -----------
    int windspeed_raw = analogRead(A0); 
    float outvoltage = windspeed_raw * (5.0 / 1023.0);
    float windspeed = mapfloat(outvoltage,0.0,5.0,0.0,30.0);
    Serial.print("Wind Speed = "); Serial.println(windspeed);
    Serial.println();
 
    // BMP280 , temp, hum, pressure
    float temperature = bmx280.readTemperature();
    float humidity;
    if (bmx280.getChipID() == CHIP_ID_BME280) {
      humidity = bmx280.readHumidity();
    }
    float pressure = bmx280.readPressure() / 100.0F; // hPa
    float altitude = bmx280.readAltitude(SEA_LEVEL_PRESSURE_HPA);  // m
    Serial.print("Temperature = ");Serial.println(temperature);
    Serial.print("Humidty = ");Serial.println(humidity);
    Serial.print("Pressure = ");Serial.println(pressure);
    Serial.print("Altitude = ");Serial.println(altitude);
    Serial.println();
    
    // Light -----------------------------------
    float lux = lightMeter.readLightLevel();
    Serial.print("Light = "); Serial.print(lux); // lux
    Serial.println();
    
    // INA219 voltage and current --------------
    float shuntvoltage = ina219.getShuntVoltage_mV();
    float busvoltage = ina219.getBusVoltage_V();
    float current_mA = ina219.getCurrent_mA();
    float loadvoltage = busvoltage + (shuntvoltage / 1000);
    float power_mW = loadvoltage * current_mA;
    float total_mAH = total_mA / 3600.0;
    Serial.println("Volt = " + String(loadvoltage));
    Serial.println("Imp = " + String(current_mA));
    Serial.println();

     // PMS5003 -----------------------------
    int index = 0;
    char value;
    char previousValue;
    Serial.println("Get PMS5003 ..");
    while (pmSerial.available()) {
      value = pmSerial.read();
      if ((index == 0 && value != 0x42) || (index == 1 && value != 0x4d)){
        Serial.println("Cannot find the data header.");
        break;
      }
      if (index == 4 || index == 6 || index == 8 || index == 10 || index == 12 || index == 14) {
        previousValue = value;
      }
      else if (index == 5) {
        pm1 = 256 * previousValue + value;
        Serial.print("\"pm1\": ");Serial.print(pm1);Serial.println(" ug/m3");
      }
      else if (index == 7) {
        pm2_5 = 256 * previousValue + value;
        Serial.print("\"pm2_5\": ");Serial.print(pm2_5);Serial.println(" ug/m3");
      }
      else if (index == 9) {
        pm10 = 256 * previousValue + value;
        Serial.print("\"pm10\": ");Serial.print(pm10);Serial.println(" ug/m3");
      } else if (index > 15) {
        break;
      }
      index++;
    }
    while(pmSerial.available()) {
      Serial.print("*");
      pmSerial.read();
    }
    Serial.println();
    
    /*
     * make payload -------------------------
     * pm25,pm10,windir,windspeed,temp,hum,pressure,alt,lux,volt,amp
     */
    
    char buff[67];
    unsigned int windspeed_i = (int) (windspeed * 10);
    unsigned int temperature_i = (int) (temperature * 100);
    unsigned int humidity_i = (int) (humidity * 100);
    unsigned long pressure_i = (long) (pressure*100);
    unsigned long altitude_i = (long) (altitude*100);
    unsigned long light_i = (long) (lux * 100);
    unsigned int volt_i = (int) (loadvoltage * 10);
    unsigned int current_i = (int) (current_mA * 10); 
    sprintf(buff,"%04x%04x%04x%04x%04x%04x%08lx%08lx%08lx%04x%04x",pm2_5, pm10, winddir, windspeed_i, temperature_i, humidity_i, (long)pressure_i, (long)altitude_i, (long)light_i, volt_i, current_i);
    sendMessage(buff);
    Serial.println("Sending "+String(buff));
    
    delay(DCT*1000 + random(3000));   // random DCT -> DCT + 3000  ms
     
    for (int i=0; i < DCT/8; i++) {
     LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF,SPI_OFF, USART0_OFF, TWI_OFF);
    }
    
    Serial.println("Arduino:- Hey I just Woke up");
    Serial.println("");
    delay(1000);
    
}

void sendMessage(String outgoing) {
  while (LoRa.beginPacket() == 0) {
    Serial.println("waiting for radio ... ");
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

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}



//000a 000a 00b5 0000 09ad 1129 00008b74 00000001 00002cd8 0000 014d
//0000 0000 00c5 0001 09e9 1036 00018a68 0000358d 00003a44 007a 0152

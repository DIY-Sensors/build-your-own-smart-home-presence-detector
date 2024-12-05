/*
 *     Maker      : DIYSensors                
 *     Project    : Presemce detector with temp
 *     Version    : 1.0
 *     Date       : 11-2024
 *     Programmer : Ap Matteman
 *
 *     * Parts of this code is from the example from Nick Reynolds (maker of the library)
 *     
 */    

#include <ld2410.h>
#include <Wire.h>
#include <WiFi.h>    //ESP8266WiFi.h for ESP8266 - WiFi.h for ESP32
#include <PubSubClient.h>
#include "arduino_secrets.h"

// Code for LD2410 radar
#if defined(ESP32)
  #ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
    #if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
      #define MONITOR_SERIAL Serial
      #define RADAR_SERIAL Serial1
      #define RADAR_RX_PIN 32
      #define RADAR_TX_PIN 33
    #elif CONFIG_IDF_TARGET_ESP32S2
      #define MONITOR_SERIAL Serial
      #define RADAR_SERIAL Serial1
      #define RADAR_RX_PIN 9
      #define RADAR_TX_PIN 8
    #elif CONFIG_IDF_TARGET_ESP32C3
      #define MONITOR_SERIAL Serial
      #define RADAR_SERIAL Serial1
      #define RADAR_RX_PIN 4
      #define RADAR_TX_PIN 5
    #else 
      #error Target CONFIG_IDF_TARGET is not supported
    #endif
  #else // ESP32 Before IDF 4.0
    #define MONITOR_SERIAL Serial
    #define RADAR_SERIAL Serial1
    #define RADAR_RX_PIN 32
    #define RADAR_TX_PIN 33
  #endif
#elif defined(__AVR_ATmega32U4__)
  #define MONITOR_SERIAL Serial
  #define RADAR_SERIAL Serial1
  #define RADAR_RX_PIN 0
  #define RADAR_TX_PIN 1
#endif

ld2410 radar;


uint32_t lastReading = 0;
bool radarConnected = false;

// Code for WiFi, MQTT and OTA
int iWiFiTry = 0;          
int iMQTTTry = 0;
String sClient_id;

int iTargetDistance = 0;
int iCount = 0;


unsigned long lPmillis = 0;        // will store last time MQTT has published
const long lInterval = 1000; // 1000 ms => 1 Second

const char* ssid = YourSSID;
const char* password = YourWiFiPassWord;
const char* HostName = "Arduino_OTA_DEMO";

const char* mqtt_broker = YourMQTTserver;
const char* mqtt_user = YourMQTTuser;
const char* mqtt_password = YourMQTTpassword;

WiFiClient espClient;
PubSubClient MQTTclient(espClient); // MQTT Client

void Connect2WiFi() { 
  //Connect to WiFi
  WiFi.mode(WIFI_STA);  //in case of an ESP32
  iWiFiTry = 0;
  WiFi.begin(ssid, password);
  WiFi.setHostname(HostName);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED && iWiFiTry < 11) { //Try to connect to WiFi for 11 times
    ++iWiFiTry;
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  
  Serial.println("Ready");

  //Unique MQTT Device name
  sClient_id = "esp-client-" + String(WiFi.macAddress());
  Serial.print("ESP Client name: "); Serial.println(sClient_id);
}

void Connect2MQTT() {
  // Connect to the MQTT server
  iMQTTTry=0;
  if (WiFi.status() != WL_CONNECTED) { 
    Connect2WiFi; 
  }

  Serial.print("Connecting to MQTT ");
  MQTTclient.setServer(mqtt_broker, 1883);
  while (!MQTTclient.connect(sClient_id.c_str(), mqtt_user, mqtt_password) && iMQTTTry < 11) { //Try to connect to MQTT for 11 times
    ++iMQTTTry;
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
}

void setup(void)
{
  MONITOR_SERIAL.begin(115200); //Feedback over Serial Monitor
  //radar.debug(MONITOR_SERIAL); //Uncomment to show debug information from the library on the Serial Monitor. By default this does not show sensor reads as they are very frequent.
  #if defined(ESP32)
    RADAR_SERIAL.begin(256000, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN); //UART for monitoring the radar
  #elif defined(__AVR_ATmega32U4__)
    RADAR_SERIAL.begin(256000); //UART for monitoring the radar
  #endif
  delay(500);
  MONITOR_SERIAL.print(F("\nConnect LD2410 radar TX to GPIO:"));
  MONITOR_SERIAL.println(RADAR_RX_PIN);
  MONITOR_SERIAL.print(F("Connect LD2410 radar RX to GPIO:"));
  MONITOR_SERIAL.println(RADAR_TX_PIN);
  MONITOR_SERIAL.print(F("LD2410 radar sensor initialising: "));
  if(radar.begin(RADAR_SERIAL))
  {
    MONITOR_SERIAL.println(F("OK"));
    MONITOR_SERIAL.print(F("LD2410 firmware version: "));
    MONITOR_SERIAL.print(radar.firmware_major_version);
    MONITOR_SERIAL.print('.');
    MONITOR_SERIAL.print(radar.firmware_minor_version);
    MONITOR_SERIAL.print('.');
    MONITOR_SERIAL.println(radar.firmware_bugfix_version, HEX);
  }
  else
  {
    MONITOR_SERIAL.println(F("not connected"));
  }

  
  Connect2WiFi();
  Connect2MQTT();

}

void loop()
{
  int iReadTargetDistance;

  radar.read();
  iReadTargetDistance = radar.stationaryTargetDistance();
  //Serial.print("Target Read Distance: "); Serial.println(iReadTargetDistance);

  if (iReadTargetDistance != iTargetDistance) {
    iTargetDistance = iReadTargetDistance;
    if (!MQTTclient.connect(sClient_id.c_str(), mqtt_user, mqtt_password)) { Connect2MQTT(); }

    if (iWiFiTry > 10){ 
      Serial.println(" Reboot in 2 seconds!!!");
      delay(2000);
      ESP.restart(); 
    }
    MQTTclient.publish("Office/presence/distance", String(iTargetDistance).c_str());
    Serial.print("Target Distance: "); Serial.println(iTargetDistance);
  }



  if( millis() - lastReading > 1000)  //Report every 1000ms
  {
    lastReading = millis();

    iCount++;
    if(iCount>10000) { iCount = 0; }
    if (!MQTTclient.connect(sClient_id.c_str(), mqtt_user, mqtt_password)) { Connect2MQTT(); }

    if (iWiFiTry > 10){ 
      Serial.println(" Reboot in 2 seconds!!!");
      delay(2000);
      ESP.restart(); 
    }
    MQTTclient.publish("Office/presence/Counter", String(iCount).c_str());
    Serial.print("iCount: "); Serial.println(iCount);
    
  }
}

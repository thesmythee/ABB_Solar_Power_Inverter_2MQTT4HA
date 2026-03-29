// This is based on library by xreef/ABB_Aurora_Solar_Inverter_Library
// For clarification on how this works, see their repository

#include <ESP8266WiFi.h> // or <WiFi.h> for ESP32
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ArduinoOTA.h>
#include "Aurora.h"      // File and dependencies found in the xreef/ABB_Aurora_Solar_Inverter_Library repository

// UPDATE THIS INVERTER ADDRESS IF YOUR INVERTER IS NOT SET TO ADDRESS 2
#define ABB_INVERTER_ADDRESS 2

// *** Replace with your network credentials
const char* ssid =     "YOUR_WIFI_SSID";  // UPDATE THIS ENTRY
const char* password = "YOUR_WIFI_PSWD";  // UPDATE THIS ENTRY

// *** MQTT Broker settings
const char* mqtt_server = "YOUR_MQTT_IP";
const char* mqtt_user = "YOUR_MQTT_USER";
const char* mqtt_password = "YOUR_MQTT_PSWD"; 
const char* mqtt_base_topic = "abb_inverter/"; 

// *** Aurora Inverter setup
Aurora::DataSystemSerialNumber systemSN;
Aurora::DataCumulatedEnergy    cumulatedEnergy;
Aurora::DataDSP                dataDSP;
Aurora::DataSystemPN           systemPN;
Aurora::DataVersion            systemVersion;

WiFiClient espClient;
PubSubClient client(espClient);
SoftwareSerial inverterSerial(12, 14); // RX, TX pins for RS485 converter
Aurora inverter = Aurora(ABB_INVERTER_ADDRESS, &inverterSerial, 13);
// Aurora inverter = Aurora(2, &Serial, 13);  // For hardware serial

unsigned long timerMillis = 0;
unsigned long inverter_update_delay = 300000; // 1000 ms/s * 60 s/min * 5 min = 300000 ms

void setup() {
  Serial.begin(115200);
  inverter.begin();

  setupWiFi();
  client.setServer(mqtt_server, 1883);
  // client.setCallback(callback);

  // Initialize OTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch"; // or "filesystem"
    } else { // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin("CHANGE THIS PASSWORD");  // UPDATE THIS ENTRY

  Serial.println("Sending config message");
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Handle OTA
  ArduinoOTA.handle();
  
  // First Run Setup
  if(timerMillis == 0){
    if(!systemSN.readState) {
      systemSN = inverter.readSystemSerialNumber();
    } else {
      Serial.print(".");
      if(setupMQTTAutodiscovery(systemSN.SerialNumber)) {
        Serial.println("sent");
        timerMillis++;
      }
    }
  }
  // Data collection and publishing
  else if(millis()-timerMillis > inverter_update_delay){
    publishData(readInverterData(), systemSN.SerialNumber);
    timerMillis = millis();
  }
}

void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println(" connected");
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ABB_inverter", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

bool setupMQTTAutodiscovery(String systemSerNum) {
  // systemPN = inverter.readSystemPN();
  systemVersion = inverter.readVersion();

  String payload[27];
  payload[0] = "{ \"device\": { ";
  payload[1] = "\"identifiers\": \"inverter_" + systemSerNum + "\", ";
  payload[2] = "\"name\": \"ABB Inverter " + String(ABB_INVERTER_ADDRESS) + "\", ";
  payload[3] = "\"manufacturer\": \"ABB Solar One\", ";
  if(systemVersion.state.readState) payload[4] = "\"model\": \"" + systemVersion.getModelName().name + "\", ";
  else payload[4] = "\"model\": \"PVI\", ";
  payload[5] = "\"serial_number\": \"" + systemSerNum + "\" ";
  payload[6] = "}, \"origin\": {";
  payload[7] = "\"name\": \"ABB_Aurora_Solar_Inverter_Library\", ";
  payload[8] = "\"sw_version\": \"1.0.3\", ";
  payload[9] = "\"support_url\": \"https://github.com/xreef/ABB_Aurora_Solar_Inverter_Library\" ";
  payload[10] = "}, \"components\": { ";
  payload[11] = "\"" + systemSerNum + "_energy\": {";
  payload[12] = "\"platform\": \"sensor\", ";
  payload[13] = "\"device_class\": \"energy\", ";
  payload[14] = "\"unit_of_measurement\": \"Wh\", ";
  payload[15] = "\"state_class\": \"total_increasing\", ";
  payload[16] = "\"value_template\": \"{{ value_json.energy }}\", ";
  payload[17] = "\"unique_id\": \"" + systemSerNum + "_energy\" ";
  payload[18] = "}, \"" + systemSerNum + "_power\": {";
  payload[19] = "\"platform\": \"sensor\", ";
  payload[20] = "\"device_class\": \"power\", ";
  payload[21] = "\"state_class\": \"measurement\", ";
  payload[22] = "\"unit_of_measurement\": \"W\", ";
  payload[23] = "\"value_template\": \"{{ value_json.power }}\", ";
  payload[24] = "\"unique_id\": \"" + systemSerNum + "_power\" } }, ";
  payload[25] = "\"state_topic\": \"" + String(mqtt_base_topic) + systemSerNum + "/state\", ";
  payload[26] = "\"qos\": 1 }";

  int payloadLength = 0;
  for(int i = 0; i<27; i++){
    payloadLength += payload[i].length();
  }

  String mqtt_snconfig_topic = "homeassistant/device/abb_inverter_" + systemSerNum + "/config";
  client.beginPublish(mqtt_snconfig_topic.c_str(), payloadLength, 1);
  for(int i = 0; i < 27; i++){
    client.write(reinterpret_cast<const uint8_t*>(payload[i].c_str()), payload[i].length());
  }
  return client.endPublish();
}

String readInverterData() {
  bool emptyJson = true;
  String data = "{ ";

  cumulatedEnergy = inverter.readCumulatedEnergy(CUMULATED_TOTAL_ENERGY_LIFETIME);
  if(cumulatedEnergy.state.readState == true ) {
    data += "\"energy\": \"" + String(cumulatedEnergy.energy) + "\"";
    emptyJson = false;
  }

  dataDSP = inverter.readDSP(DSP_GRID_POWER_ALL);
  if(dataDSP.state.readState == true) {
    if(!emptyJson) data += ", ";
    data += "\"power\": \"" + String(dataDSP.value) + "\" ";
    emptyJson = false;
  }

  data += "}";
  return data;
}

void publishData(String data, String systemSerNum) {
  String topic = String(mqtt_base_topic) + systemSerNum + "/state";
  client.publish(topic.c_str(), data.c_str());
  Serial.print("MQTT data sent: ");
  Serial.println(data);
}

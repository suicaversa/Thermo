/******************************************************************************
 * Copyright 2018 Google
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/
// This file contains static methods for API requests using Wifi / MQTT
#ifndef __ESP8266_MQTT_H__
#define __ESP8266_MQTT_H__

#define SETTING_FILE_PATH "/setting.json"
#define PRIMARY_CA_FILE_PATH "/primary_ca.pem"
#define BACKUP_CA_FILE_PATH "/backup_ca.pem"
#define PRIVATE_KEY_FILE_PATH "/private.key"

#include <ESP8266WiFi.h>
#include "FS.h"

// You need to set certificates to All SSL cyphers and you may need to
// increase memory settings in Arduino/cores/esp8266/StackThunk.cpp:
//   https://github.com/esp8266/Arduino/issues/6811
#include "WiFiClientSecureBearSSL.h"
#include <time.h>

#include <MQTT.h>
#include <ArduinoJson.h>

#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>
#include "ciotc_config.h" // Wifi configuration here

// !!REPLACEME!!
// The MQTT callback function for commands and configuration updates
// Place your message handler code here.
void messageReceived(String &topic, String &payload)
{
  Serial.println("incoming: " + topic + " - " + payload);
}
///////////////////////////////

// Initialize WiFi and MQTT for this board
MQTTClient *mqttClient;
BearSSL::WiFiClientSecure *netClient;
BearSSL::X509List certList;
CloudIoTCoreDevice *device;
CloudIoTCoreMqtt *mqtt;
unsigned long iss = 0;
String jwt;

///////////////////////////////
// Helpers specific to this board
///////////////////////////////
String getDefaultSensor()
{
  return "Wifi: " + String(WiFi.RSSI()) + "db";
}

String getJwt()
{
  // Disable software watchdog as these operations can take a while.
  ESP.wdtDisable();
  iss = time(nullptr);
  Serial.println("Refreshing JWT");
  jwt = device->createJWT(iss, jwt_exp_secs);
  ESP.wdtEnable(0);
  return jwt;
}

String readStringFromFile(char path[]) {
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    return String(' ');
  }

  File file = SPIFFS.open(path, "r");
  String result;
  if (file) {
    result = file.readString();
    Serial.print("Readfile: " + String(path) + ":");
    Serial.println(result);
    file.close();
  }

  return result;
}

void loadSettings() {
  String settingJsonString = readStringFromFile(SETTING_FILE_PATH);
  if (settingJsonString.length() > 0) {
    const size_t CAPACITY = JSON_OBJECT_SIZE(6) + 1024;
    StaticJsonDocument<CAPACITY> settings;
    DeserializationError error = deserializeJson(settings, settingJsonString);
    if (error) {
      Serial.print(F("setting file deserializeJson() failed: "));
      Serial.println(error.c_str());
    }

    project_id = strdup(settings["project_id"]);
    location = strdup(settings["location"]);
    registry_id = strdup(settings["registry_id"]);
    device_id = strdup(settings["device_id"]);
    ntp_primary = strdup(settings["ntp_primary"]);
    ntp_secondary = strdup(settings["ntp_secondary"]);
  }
}

void loadPrivateKey() {
  String privateKey = readStringFromFile(PRIVATE_KEY_FILE_PATH);
  private_key_str = strdup(privateKey.c_str());
}

void setupCert()
{
  // primary_ca = strdup(readStringFromFile(PRIMARY_CA_FILE_PATH).c_str());
  // backup_ca = strdup(readStringFromFile(BACKUP_CA_FILE_PATH).c_str());
  // // // Set CA cert on wifi client
  // // // If using a static (pem) cert, uncomment in ciotc_config.h:
  // certList.append(primary_ca);
  // certList.append(backup_ca);
  // netClient->setTrustAnchors(&certList);
  // return;
  Serial.print("Setting up certs...");
  certList.append(strdup(readStringFromFile(PRIMARY_CA_FILE_PATH).c_str()));
  certList.append(strdup(readStringFromFile(BACKUP_CA_FILE_PATH).c_str()));
  netClient->setTrustAnchors(&certList);
  Serial.println("Done.");
}

///////////////////////////////
// Orchestrates various methods from preceeding code.
///////////////////////////////
bool publishTelemetry(String data)
{
  return mqtt->publishTelemetry(data);
}

bool publishTelemetry(const char *data, int length)
{
  return mqtt->publishTelemetry(data, length);
}

bool publishTelemetry(String subfolder, String data)
{
  return mqtt->publishTelemetry(subfolder, data);
}

bool publishTelemetry(String subfolder, const char *data, int length)
{
  return mqtt->publishTelemetry(subfolder, data, length);
}

void connect()
{
  mqtt->mqttConnect();
}

void adjustClock() {
  configTime(0, 0, ntp_primary, ntp_secondary);
  Serial.println("Waiting on time sync...");
  while (time(nullptr) < 1510644967)
  {
    delay(10);
  }  
}

// TODO: fix globals
void setupCloudIoT()
{
  // ESP8266 WiFi setup
  netClient = new WiFiClientSecure();
    
  loadSettings();
  loadPrivateKey();
  setupCert();

  // Create the device
  device = new CloudIoTCoreDevice(
      project_id, location, registry_id, device_id,
      private_key_str);

  adjustClock();
  mqttClient = new MQTTClient(512);
  mqttClient->setOptions(180, true, 1000); // keepAlive, cleanSession, timeout
  mqtt = new CloudIoTCoreMqtt(mqttClient, netClient, device);
  mqtt->setUseLts(true);
  mqtt->startMQTT(); // Opens connection
}

#endif //__ESP8266_MQTT_H__

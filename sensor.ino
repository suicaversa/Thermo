#include <ArduinoJson.h>
#include <IoAbstraction.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <CloudIoTCore.h>
#include "esp8266_mqtt.h"
#include "FS.h"
#define WIFI_RESET_PIN 14
#define LED_PIN 4
#define MAX_WIFI_RETRY_SECONDS 10
#define MODULE_NAME "Thermae"

void initializeService() {
  Serial.print("Preparing Cloud IoT... ");
  setupCloudIoT();
  Serial.println("done.");

  initializeServer();

  taskManager.scheduleFixedRate(1, []{
    serverLoop();
  });

  taskManager.scheduleFixedRate(1000, []{
    Serial.println(ESP.getFreeHeap());
  });

  taskManager.scheduleFixedRate(10000, []{
    mqtt->loop();
    delay(10); // <- fixes some issues with WiFi stability

    if (!mqttClient->connected()) {
      Serial.print("connecting Cloud IoT...");
      ESP.wdtDisable();
      connect();
      ESP.wdtEnable(0);
      Serial.println("Done.");
    }

    String result = readDht();
    Serial.println(result);
    publishTelemetry(result);
  });
}

void initializeDevise() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("Starting Module...");
  initializeDHT();

  delay(100);
}

void setup() {
  initializeDevise();
  if (setupWifi()) {
    initializeService();
  } else {
    Serial.println("Could not started up devise.");
    errorBlink(LED_PIN);
    errorBlink(LED_PIN);
    errorBlink(LED_PIN);
  }
}

void loop() {
  taskManager.runLoop();
}

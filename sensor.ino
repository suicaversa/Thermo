#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <IoAbstraction.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include "FS.h"
#define WIFI_RESET_PIN 14
#define LED_PIN 4
#define DHTPIN 13
#define DHTTYPE DHT11
#define MAX_WIFI_RETRY_SECONDS 10
#define MODULE_NAME "Thermae"
#define ROOT_CA_CERT_PATH "/rootca.pem"
#define CERTIFICATE_PATH "/cert.pem.cert"
#define PRIVATE_KEY_PATH "/cert.pem.key"
#define MQTT_SERVER "mqtt.googleapis.com"
#define MQTT_PORT 8883
#define MQTT_TOPIC "Thermae"

ESP8266WebServer server(80);
DHT dht( DHTPIN, DHTTYPE );
WiFiClientSecure wifiClient;
PubSubClient pubsubClient(wifiClient);

void handleRootGet() {
  const int capacity = JSON_OBJECT_SIZE(4);
  DynamicJsonDocument doc(capacity);
  doc["message"] = String("Hello from ") + String(MODULE_NAME);
  doc["sensor"] = readDht();
  Serial.println("Accessed / ;");

  char output[128];
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleSettingPost() {
  const int capacity = JSON_OBJECT_SIZE(3) + 4096 * 3;
  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    server.send(400);
    return;
  }

  String cert = doc["cert"];
  if (cert.length() > 0) {
    Serial.println(cert);
    File certf = SPIFFS.open(CERTIFICATE_PATH, "w");
    if (certf) {
      certf.print(cert);
      certf.close();
    }
  }

  String privateKey = doc["privateKey"];
  if (privateKey.length() > 0) {
    Serial.println(privateKey);
    File privatef = SPIFFS.open(PRIVATE_KEY_PATH, "w");
    if (privatef) {
      privatef.print(privateKey);
      privatef.close();
    }
  }

  String rootCA = doc["rootCA"];
  if (rootCA.length() > 0) {
    Serial.println(rootCA);
    File rootcaf = SPIFFS.open(ROOT_CA_CERT_PATH, "w");
    if (rootcaf) {
      Serial.print("Writing to ");
      Serial.print(ROOT_CA_CERT_PATH);
      rootcaf.print(rootCA);
      rootcaf.close();
      Serial.println(" Done.");
    }
  }  

  handleSettingGet();
  doc.clear();
}

void handleSettingGet() {
  const int capacity = JSON_OBJECT_SIZE(3) + 4096 * 3;
  DynamicJsonDocument doc(capacity);
  File f = SPIFFS.open(CERTIFICATE_PATH, "r");
  if (f) {
    String cert = f.readString();
    doc["cert"] = cert;
    Serial.print("read from fs: ");
    Serial.println(cert);
    f.close();
  }

  File privateF = SPIFFS.open(PRIVATE_KEY_PATH, "r");
  if (privateF) {
    String privateKey = privateF.readString();
    doc["privateKey"] = privateKey;
    Serial.print("read from fs: ");
    Serial.println(privateKey);
    privateF.close();
  }

  File rootCAF = SPIFFS.open(ROOT_CA_CERT_PATH, "r");
  if (rootCAF) {
    String rootCA = rootCAF.readString();
    doc["rootCa"] = rootCA;
    Serial.print("read from fs: ");
    Serial.println(rootCA);
    rootCAF.close();
  }  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
  doc.clear();
  output.clear();
}

void prepareRouter() {
  server.on("/", HTTP_GET ,handleRootGet);
  server.on("/settings", HTTP_POST, handleSettingPost);
  server.on("/cert", HTTP_GET, handleSettingGet);
}

String openFileString(String path) {
  File f = SPIFFS.open(path, "r");
  String fileString;
  if (f) {
    fileString = f.readString();
  }
  f.close();
  return fileString;
}

String getCACert() {
  return openFileString(ROOT_CA_CERT_PATH);
}

String getCertificate() {
  return openFileString(CERTIFICATE_PATH);
}

String getPrivateKey() {
  return openFileString(PRIVATE_KEY_PATH);
}


void prepareMQTT() {
  // File caCertFile = SPIFFS.open(ROOT_CA_CERT_PATH, "r");
  File certificateFile = SPIFFS.open(CERTIFICATE_PATH, "r");
  File privateKeyFile = SPIFFS.open(PRIVATE_KEY_PATH, "r");
  // wifiClient.loadCACert(caCertFile, caCertFile.size());
  wifiClient.loadCertificate(certificateFile, certificateFile.size());
  wifiClient.loadPrivateKey(privateKeyFile, privateKeyFile.size());
  pubsubClient.setServer(MQTT_SERVER, MQTT_PORT);
  pubsubClient.setCallback(mqttCallback);
  // caCertFile.close();
  certificateFile.close();
  privateKeyFile.close();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Received. topic=");
    Serial.println(topic);
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.print("\n");
}

void connectMQTT(String message) {
  while (!pubsubClient.connected()) {
    if (pubsubClient.connect(MODULE_NAME)) {
        Serial.println("Connected.");
        pubsubClient.publish(MQTT_TOPIC, message.c_str());
        Serial.println("Subscribed.");
    } else {
        Serial.print("Failed. Error state=");
        Serial.print(pubsubClient.state());
        // Wait 5 seconds before retrying
        delay(5000);
    }
  }  
}

void initializeService() {
  Serial.println("Preparing httpd...");
  prepareRouter();
  server.begin();
  Serial.println("Server begins.");

  Serial.println("Preparing mqtt...");
  prepareMQTT();
  Serial.println("mqtt done...");

  taskManager.scheduleFixedRate(1, []{
    server.handleClient();
  });
  taskManager.scheduleFixedRate(10000, []{
    String result = readDht();
    Serial.println(result);
    // pubsubClient.publish(MQTT_TOPIC, result.c_str());
    connectMQTT(result.c_str());
  });
  taskManager.scheduleFixedRate(1, []{
    pubsubClient.loop();
  });
}

boolean isInternetAvailable() {
  return WiFi.status() == WL_CONNECTED;
}

boolean tryConnectWifi() {
  boolean isHighLED = false;
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < MAX_WIFI_RETRY_SECONDS) {
    digitalWrite(LED_PIN, isHighLED ? HIGH : LOW);
    isHighLED = !isHighLED;
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
    i++;
  }
  digitalWrite(LED_PIN, LOW);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Successfully connected: ");
    Serial.println(WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("Failed to connect.");
    return false;
  }
}

boolean connectWiFiViaSmartConfig() {
  boolean isHighLED = false;

  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  Serial.println("Waiting for SmartConfig.");
  while (!WiFi.smartConfigDone()) {
    digitalWrite(LED_PIN, isHighLED ? HIGH : LOW);
    isHighLED = !isHighLED;
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("SmartConfig received.");
  digitalWrite(LED_PIN, LOW);
  return tryConnectWifi();
}

boolean tryPreviousWifi() {
  Serial.println("Attempting to connect to previous connected ssid.");
  WiFi.begin();
  return tryConnectWifi();
}

boolean waitingWiFiReset() {
  boolean isResetWifi = false;
  boolean isHighLED = false;
  // 最初の30*100ms=3秒間でボタンが押されたらWifiをリセット
  for(int count=0; count < 30; count++) {
    digitalWrite(LED_PIN, isHighLED ? HIGH : LOW);
    isHighLED = !isHighLED;
    Serial.println(digitalRead(WIFI_RESET_PIN) == LOW);
    if (digitalRead(WIFI_RESET_PIN) == LOW) isResetWifi = true;
    delay(100);
  }
  digitalWrite(LED_PIN, LOW);
  return isResetWifi;
}

boolean setupWifi() {
  if(waitingWiFiReset()) {
    connectWiFiViaSmartConfig();
  } else {
    tryPreviousWifi();
  }
  return WiFi.status() == WL_CONNECTED;
}

void initializeDevise() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("Starting Module...");
  dht.begin();
  Serial.println("Starting SPIFS....");
  SPIFFS.begin();
  delay(100);
}

String readDht() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if( isnan(t) || isnan(h) )
  {
    return "Failed to read from DHT";
  }
  else
  {
    return "Humidity: " + String(h) + "%\t" + "Temperature: " + String(t) + " *C";
  }
}

void errorBlink() {
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);
}

void setup() {
  initializeDevise();
  if (setupWifi()) {
    initializeService();
  } else {
    Serial.println("Could not started up devise.");
    errorBlink();
    errorBlink();
    errorBlink();
  }
}

void loop() {
  taskManager.runLoop();
}

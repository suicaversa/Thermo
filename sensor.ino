#include <DHT.h>
#include <IoAbstraction.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#define WIFI_RESET_PIN 14
#define LED_PIN 4
#define DHTPIN 16
#define DHTTYPE DHT11
#define MAX_WIFI_RETRY_SECONDS 10
#define MODULE_NAME "Thermae"

ESP8266WebServer server(80);
DHT dht( DHTPIN, DHTTYPE );

void handleRootGet() {
  Serial.println("Accessed / ;");
  String message = "{ \"message\": \"Hello from ";
  message.concat(MODULE_NAME);
  message.concat("\", ");
  message.concat("\"sensor\": \"");
  message.concat(readDht());
  message.concat("\"}");
  server.send(200, "application/json", message);
}

void handleRoot() {
  handleRootGet();
}

void initializeService() {
  Serial.println("Prepareing httpd...");
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Server begins.");
  taskManager.scheduleFixedRate(1, []{
    server.handleClient();
  });
  taskManager.scheduleFixedRate(10000, []{
    Serial.println(readDht());
  });
}

boolean isInternetAvailable() {
  return WiFi.status() == WL_CONNECTED;
}

boolean tryConnectWifi() {
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < MAX_WIFI_RETRY_SECONDS) {
    Serial.print(".");
    // wait 1 second for re-trying
    delay(1000);
    i++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Successfully connected: ");
    Serial.println(WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("Failed to connect.");
    return false;
  }
}

boolean tryPreviousWifi() {
  Serial.println("Attempting to connect to previous connected ssid.");
  WiFi.begin();
  return tryConnectWifi();
}

boolean setupWifi(const char* ssid, const char* password) {
  Serial.print("Attempting to connect to ssid: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  return tryConnectWifi();
}

void initializeDevise() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("Starting Module...");
  // 最初の30*100ms=3秒間でボタンが押されたらWifiをリセット
  for(int count=0; count < 30; count++) {
    Serial.println(digitalRead(WIFI_RESET_PIN) == HIGH);
    delay(100);
  }
  dht.begin();
  // Serial.print("Starting Filesystem...");
  // SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);
  // loadSettings();
  // Serial.println("Done.");
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
}

void setup() {
  initializeDevise();
  if (tryPreviousWifi() || setupWifi("YSNO-WiFi", "AllIsWell")) {
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

#include <DHT.h>
#include <IoAbstraction.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#define WIFI_RESET_PIN 14
#define LED_PIN 4
#define DHTPIN 13
#define DHTTYPE DHT11
#define MAX_WIFI_RETRY_SECONDS 10
#define MODULE_NAME "Thermae"

ESP8266WebServer server(80);
DHT dht( DHTPIN, DHTTYPE );

void handleRootGet() {
  char message[256];
  const char* dhtText = readDht().c_str();
  sprintf(message, "{\"message\": \"Hello from %s\", \"sensor\": \"%s\"}\n", MODULE_NAME, dhtText);
  Serial.println("Accessed / ;");
  // String message = "{ \"message\": \"Hello from ";
  // message.concat(MODULE_NAME);
  // message.concat("\", ");
  // message.concat("\"sensor\": \"");
  // message.concat(readDht());
  // message.concat("\"}");

  server.send(200, "application/json", String(message));
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

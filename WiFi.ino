#include <ESP8266WiFi.h>

#define WIFI_RESET_PIN 14
#define MAX_WIFI_RETRY_SECONDS 10
#define LED_PIN 4

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

boolean setupWifi() {
  if(waitingWiFiReset()) {
    connectWiFiViaSmartConfig();
  } else {
    tryPreviousWifi();
  }
  return WiFi.status() == WL_CONNECTED;
}
#include <DHT.h>

#define DHTPIN 13
#define DHTTYPE DHT11

DHT dht( DHTPIN, DHTTYPE );

void initializeDHT() {
  Serial.print("Starting DHT Module... ");
  dht.begin();
  Serial.println("Done.");
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
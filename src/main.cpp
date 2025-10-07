#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ETH.h>

// DS18B20 data pin
#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting ESP32 ETH + DS18B20 example");

  // Initialize DS18B20
  sensors.begin();

  // Optional: Start Ethernet
  ETH.begin();  // auto-detects pins on esp32-ethernet-kit
  Serial.println("Ethernet starting...");
}

void loop() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  if (temperatureC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: DS18B20 not found!");
  } else {
    Serial.printf("Temperature: %.2f °C\n", temperatureC);
  }

  delay(2000);
}

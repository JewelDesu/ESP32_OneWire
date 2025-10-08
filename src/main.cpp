

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ETH.h>
#include <PubSubClient.h>

// DS18B20 data pin
#define ONE_WIRE_BUS 4

#define RELAY_FIRST 14

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

WiFiClient conClient;
PubSubClient client(conClient);

const char *mqtt_broker = "192.168.1.154";
const char *topic1 = "esp32/temp";
const char *topic2 = "esp32/relay";
const char *id = "esp32_Client";
const int mqtt_port = 1883;

bool relay = false;
float last_temp = 0;

static bool eth_connected = false;

void testClient(const char * host, uint16_t port)
{
  Serial.print("\nconnecting to ");
  Serial.println(host);

  WiFiClient client;
  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    return;
  }
  client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
  while (client.connected() && !client.available());
  while (client.available()) {
    Serial.write(client.read());
  }

  Serial.println("closing connection\n");
  client.stop();
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting ESP32 ETH + DS18B20");

  // Initialize DS18B20
  sensors.begin();
  pinMode(RELAY_FIRST, OUTPUT);
  digitalWrite(RELAY_FIRST, HIGH);
  //ETH.begin();
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, 16, ETH_CLOCK_GPIO0_IN);
  #else
  ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
  #endif
  Serial.println("Ethernet starting");

  client.setServer(mqtt_broker, mqtt_port);
}

void reconnect(){

  while(!client.connected())
  {
    Serial.println("Connecting to MQTT");
    
    if(client.connect(id))
    {
      Serial.println("connected");
      client.subscribe(topic1);
      client.subscribe(topic2);
    }
    else
    {
      Serial.println("failed to connect");
      Serial.print(client.state());
      Serial.println("retrying to connect");
      delay(5000);
    }
  }
}

void loop(){
  if(!client.connected())
  {
    reconnect();
  }
  client.loop();

  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  char tempString[8];
  dtostrf(temperatureC, 1, 2, tempString);
  if (temperatureC == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: DS18B20 not found!");
  } 
  else 
  {
    Serial.printf("Temperature: %.2f °C\n", temperatureC);
    client.publish(topic1,tempString);


    bool new_relay = (temperatureC > 26);

    if(new_relay != relay)
    {
      relay = new_relay;
      char relayMsg[32];
      
      digitalWrite(RELAY_FIRST, relay ? LOW : HIGH);
      Serial.printf("Relay: %s\n", relay ? "On" : "Off");
      snprintf(relayMsg, sizeof(relayMsg), "Relay: %s", relay ? "On" : "Off");
      client.publish(topic2,relayMsg);
    }
  }


  delay(10000);
}

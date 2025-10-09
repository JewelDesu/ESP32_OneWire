

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ETH.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

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

AsyncWebServer server(80);



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
  Serial.println(ETH.localIP());

  client.setServer(mqtt_broker, mqtt_port);


    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>"
                  "body{font-family:Arial;text-align:center;background:#111;color:#eee;}"
                  "button{font-size:22px;padding:15px 30px;margin:10px;border:none;border-radius:8px;cursor:pointer;}"
                  ".on{background:#2ecc71;color:white;}"
                  ".off{background:#e74c3c;color:white;}"
                  "</style></head><body>";
    html += "<h1>ESP32 Relay Control</h1>";
    html += "<p>Relay is <b>" + String(relay ? "ON" : "OFF") + "</b></p>";
    html += "<button class='on' onclick=\"fetch('/on')\">Turn ON</button>";
    html += "<button class='off' onclick=\"fetch('/off')\">Turn OFF</button>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request){
    relay = true;
    digitalWrite(RELAY_FIRST, LOW);
    request->send(200, "text/plain", "Relay ON");
  });

  server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request){
    relay = false;
    digitalWrite(RELAY_FIRST, HIGH);
    request->send(200, "text/plain", "Relay OFF");
  });

  delay(20000);
  server.begin();
}

void reconnect() {
  static bool connecting = false;

  if (client.connected() || connecting)
  {
    return;
  } 
  connecting = true;
  Serial.println("Reconnecting");

  if (client.connect(id)) 
  {
    Serial.println("Connected");
    client.subscribe(topic1);
    client.subscribe(topic2);
  } 
  else 
  {
    Serial.println("failed to connect"); 
    Serial.print(client.state()); 
    Serial.println("retrying to connect");
  }

  connecting = false;
}

void temp(){
  sensors.requestTemperatures();

  float temperatureC = sensors.getTempCByIndex(0);

  if (temperatureC == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: DS18B20 not found!");
    return;
  } 

  Serial.printf("Temperature: %.2f °C\n", temperatureC);
  bool new_relay = (temperatureC > 26);

  if(new_relay != relay)
  {
    relay = new_relay;
    
    digitalWrite(RELAY_FIRST, relay ? LOW : HIGH);
    Serial.printf("Relay: %s\n", relay ? "On" : "Off");

    if(client.connected())
    {
      char relayMsg[32];
      snprintf(relayMsg, sizeof(relayMsg), "Relay: %s", relay ? "On" : "Off");
      client.publish(topic2,relayMsg);
    }
  }
  if(client.connected())
  {
    char tempString[8];
    dtostrf(temperatureC, 1, 2, tempString);

    client.publish(topic1,tempString);
  }
}

void loop(){
  if(!client.connected())
  {
    reconnect();
  }
  if (client.connected()) {
    client.loop();
  }

  temp();


  delay(10000);
}

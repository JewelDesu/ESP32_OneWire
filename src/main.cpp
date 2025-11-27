

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ETH.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define I2C_SDA 33
#define I2C_SCL 32
TwoWire I2CDisplay = TwoWire(1);
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2CDisplay, -1);

// DS18B20 data pin
#define ONE_WIRE_BUS 4
#define RELAY_COUNT 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress sensor1 = { 0x28, 0x80, 0xC9, 0x4A, 0x4, 0x0, 0x0, 0x40 };
DeviceAddress sensor2 = { 0x28, 0xBB, 0xEA, 0x4A, 0x4, 0x0, 0x0, 0xE3 };
// DeviceAddress sensor3= { 0x28, 0xF3, 0xD5, 0x4A, 0x4, 0x0, 0x0, 0xDC };
DeviceAddress sensor4= { 0x28, 0x4B, 0xE2, 0x4A, 0x4, 0x0, 0x0, 0xAF };
DeviceAddress sensor3= { 0x28, 0x20, 0xE9, 0x4A, 0x4, 0x0, 0x0, 0xE4 };
// DeviceAddress sensor4= { 0x28, 0x6B, 0xAC, 0x4A, 0x4, 0x0, 0x0, 0x52 };

void callback(char* topic, byte* message, unsigned int length);

WiFiClient conClient;
PubSubClient client(conClient);

const char *mqtt_broker = "192.168.1.230";
const char *topic1 = "esp32/temp";
const char *topic2 = "esp32/relay";
const char *id = "esp32_Client";
const int mqtt_port = 1883;

bool relay = false;
float last_temp = 0;
char temp_string[50];

static bool eth_connected = false;

int relayIO[RELAY_COUNT] = {14,15,17,5};
bool relayStates[RELAY_COUNT] = {false, false, false, false};
bool autoEnabled[RELAY_COUNT] = {false, false, false, false};
float tempTriggersDown[RELAY_COUNT] = {20.0, 20.0, 20.0, 20.0};
float tempTriggersUp[RELAY_COUNT] = {26.0, 26.0, 26.0, 26.0};
//bool autoActive = false;

const char* PARAM_INPUT_RELAY  = "relay";  
const char* PARAM_INPUT_STATE  = "state";
const char* PARAM_INPUT_AUTO  = "enable_auto_input";
const char* PARAM_INPUT_TEMP = "temp";

String enableAutoChecked = "";
String inputMessage3 = "true";

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.0rem;}
    body {max-width: 700px; margin:0px auto; padding: 20px;}
    .switch {position: relative; display: inline-block; width: 100px; height: 54px} 
    .switch input {display:none}
    .slider {position:absolute; top:0; left:0; right:0; bottom:0; background-color:#ccc; border-radius:34px}
    .slider:before {position:absolute; content:""; height:40px; width:40px; left:7px; bottom:7px; background-color:white; transition:.4s; border-radius:50px}
    input:checked+.slider {background-color:#2196F3}
    input:checked+.slider:before {transform:translateX(46px)}
    input[type=number]{width:80px; font-size:1.1rem; text-align:center;}
  </style>
</head>
<body>
  <h2>ESP32 ETH Temp Automation</h2>
  %BUTTONPLACEHOLDER%

  <form action="/get">
    <table border="1" style="margin:auto;">
      <tr><th>Relay</th><th>Trigger Low C</th><th>Trigger High C</th><th>Automation</th></tr>
      %TEMPINPUTS%
    </table>
    <br>
    <input type="submit" value="Save">
  </form>

<script>
function toggleCheckbox(element){
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/update?relay="+element.id+"&state="+(element.checked?1:0), true);
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

bool getRelayState(int index) 
{
  return digitalRead(relayIO[index]) == LOW;
}

String relayState(int numRelay) 
{
  return getRelayState(numRelay - 1) ? "checked" : "";
}

String makeTempInputs() {
  String inputs = "";
  for (int i = 0; i < RELAY_COUNT; i++) {
    inputs += "<tr>";
    inputs += "<td>Relay " + String(i + 1) + "</td>";
    inputs += "<td><input type='number' step='0.1' name='tempLow" + String(i + 1) + "' value='" + String(tempTriggersDown[i]) + "'></td>";
    inputs += "<td><input type='number' step='0.1' name='tempHigh" + String(i + 1) + "' value='" + String(tempTriggersUp[i]) + "'></td>";
    inputs += "<td><input type='checkbox' name='auto" + String(i + 1) + "' " + (autoEnabled[i] ? "checked" : "") + "></td>";
    inputs += "</tr>";
  }
  return inputs;
}

void initOLED(int num, float temp) 
{


  bool currentState = getRelayState(num);

  memset(temp_string, 0, sizeof(temp_string));

	sprintf(temp_string, "Temp %.1f C |Rl: %s", temp, currentState ? "ON" : "OFF");


  display.setCursor(0, num*10);
  display.println(temp_string);
  display.display();
}

String processor(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER")
  {
    String buttons ="";
    for(int i=1; i<=RELAY_COUNT; i++)
    {
      String relayStateValue = relayState(i);
      buttons+= "<h4>Relay #" + String(i) + " - GPIO " + relayIO[i-1] + "</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"" + String(i) + "\" "+ relayStateValue +"><span class=\"slider\"></span></label>";
    }
    return buttons;
  }
  else if(var == "ENABLE_AUTO_INPUT")
  {
    return enableAutoChecked;
  }
  else if(var == "TEMPINPUTS"){
    return makeTempInputs();
  }

  return String();
}

void testClient(const char * host, uint16_t port)
{
  Serial.print("\nconnecting to ");
  Serial.println(host);

  WiFiClient client;
  if (!client.connect(host, port)) 
  {
    Serial.println("connection failed");
    return;
  }

  client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
  while (client.connected() && !client.available());
  while (client.available()) 
  {
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
  //pinMode(RELAY_FIRST, OUTPUT);
  //digitalWrite(RELAY_FIRST, HIGH);
  //ETH.begin();

  delay(1000);

  I2CDisplay.begin(I2C_SDA, I2C_SCL, 400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    return;
  }
  delay(1000);
  
  for (int i = 0; i < RELAY_COUNT; i++) 
  {
    pinMode(relayIO[i], OUTPUT);
    digitalWrite(relayIO[i], HIGH);
  }

  #if ESP_ARDUINO_VERSION_MAJOR >= 3
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, 16, ETH_CLOCK_GPIO0_IN);
  #else
  ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
  #endif
  Serial.println("Ethernet starting");
  

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
  request->send(200, "text/html", index_html, processor);
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) 
  {
    if (request->hasParam(PARAM_INPUT_RELAY) && request->hasParam(PARAM_INPUT_STATE)) 
    {
    int r = request->getParam(PARAM_INPUT_RELAY)->value().toInt() - 1;
    int s = request->getParam(PARAM_INPUT_STATE)->value().toInt();

    // Active LOW relay
    digitalWrite(relayIO[r], s ? LOW : HIGH);
    relayStates[r] = getRelayState(r);

    Serial.printf("%s\n", relayStates[r] ? "ON" : "OFF");

    //char relayMsg[32];
    //snprintf(relayMsg, sizeof(relayMsg), "Manual Relay %d -> %s\n", r + 1, relayStates[r] ? "ON" : "OFF");
    //client.publish(topic2,relayMsg);

    if (client.connected())
    {
      char topic[64];
      snprintf(topic, sizeof(topic), "esp32/relay%d", r + 1);

      char message[64];
      snprintf(message, sizeof(message), "%s", relayStates[r] ? "ON" : "OFF");

      client.publish(topic, message);
    }
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    // automation

  for (int i = 0; i < RELAY_COUNT; i++) 
  {
    String lowParam = "tempLow" + String(i + 1);
    String highParam = "tempHigh" + String(i + 1);
    String autoParam = "auto" + String(i + 1);

    if (request->hasParam(lowParam)) {
      tempTriggersDown[i] = request->getParam(lowParam)->value().toFloat();
    }
    if (request->hasParam(highParam)) {
      tempTriggersUp[i] = request->getParam(highParam)->value().toFloat();
    }
    autoEnabled[i] = request->hasParam(autoParam);

    Serial.printf(
      "Relay %d -> Low: %.2f°C | High: %.2f°C | Auto: %s\n",
      i + 1,
      tempTriggersDown[i],
      tempTriggersUp[i],
      autoEnabled[i] ? "ON" : "OFF"
  );
}

request->redirect("/");

  });

 

  // Start server

  delay(20000);
  server.begin();

  Serial.println(ETH.localIP());

  
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
    for (int i = 0; i < RELAY_COUNT; i++) 
    {
      char topic[32];
      snprintf(topic, sizeof(topic), "esp32/relayStatus%d", i + 1);
      client.subscribe(topic);
      Serial.print("Subscribed to: ");
      Serial.println(topic);
    }
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
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  float temps[RELAY_COUNT] = {
    sensors.getTempC(sensor1),
    sensors.getTempC(sensor2),
    sensors.getTempC(sensor3),
    sensors.getTempC(sensor4)
  };

  if (temps[0] == DEVICE_DISCONNECTED_C) 
  {
    Serial.println("Error: DS18B20 not found!");
    return;
  } 

  for(int i=0;i<RELAY_COUNT;i++)
  {
    Serial.printf("T%d: %.2f°C (Trig %.2f°C)\n", i+1, temps[i], tempTriggersDown[i]);
    Serial.printf("T%d: %.2f°C (Trig %.2f°C)\n", i+1, temps[i], tempTriggersUp[i]);

    initOLED(i,temps[i]);

      if (client.connected())
      {
        char payload[128];
        snprintf(payload, sizeof(payload), "%.2f", temps[i]);
        char topic1[64];
        snprintf(topic1, sizeof(topic1), "topic/smth%d", i + 1);

        client.publish(topic1, payload);
      }


    if (autoEnabled[i]) 
    {
      bool shouldBeOn = temps[i] > tempTriggersUp[i];
      bool shouldBeOff = temps[i] < tempTriggersDown[i];
      bool currentState = getRelayState(i);

      if (shouldBeOn && !currentState) 
      {
        digitalWrite(relayIO[i], LOW);
        relayStates[i] = true;
        Serial.printf("Auto Relay %d -> ON\n", i + 1);

        if(client.connected())
        {
          
          char relayMsg[32];
          snprintf(relayMsg, sizeof(relayMsg), "ON");
          char topic2[64];
          snprintf(topic2, sizeof(topic2), "esp32/relay%d", i + 1);
          Serial.printf(relayMsg);
          client.publish(topic2,relayMsg);
        }
      } 
      else if (shouldBeOff && currentState) 
      {
        digitalWrite(relayIO[i], HIGH);
        relayStates[i] = false;
        Serial.printf("Auto Relay %d -> OFF\n", i + 1);

        if(client.connected())
        {
          char relayMsg[32];
          snprintf(relayMsg, sizeof(relayMsg), "OFF");
          char topic2[64];
          snprintf(topic2, sizeof(topic2), "esp32/relay%d", i + 1);
          Serial.printf(relayMsg);
          client.publish(topic2,relayMsg);
        }
      }

      // for (int i = 0; i < RELAY_COUNT; i++) 
      // {
      //   bool shouldBeOn = (temps[i] < tempTriggersDown[i]);
      //   bool shouldBeOff = (temps[i] > tempTriggersUp[i]);
      //   bool currentState = getRelayState(i);

      //   if (shouldBeOn != currentState) 
      //   {
      //     digitalWrite(relayIO[i], shouldBeOn ? LOW : HIGH);
      //     relayStates[i] = shouldBeOn;
      //     Serial.printf("Auto Relay %d -> %s\n", i + 1, shouldBeOn ? "ON" : "OFF");

      //     if(client.connected())
      //     {
      //       char relayMsg[32];
      //       snprintf(relayMsg, sizeof(relayMsg), "Auto Relay %d -> %s\n", i + 1, shouldBeOn ? "ON" : "OFF");
      //       client.publish(topic2,relayMsg);
      //     }
      //   }
      //   else if (shouldBeOff != currentState) 
      //   {
      //     digitalWrite(relayIO[i], shouldBeOff ? HIGH : LOW);
      //     relayStates[i] = shouldBeOff;
      //     Serial.printf("Auto Relay %d -> %s\n", i + 1, shouldBeOff ? "OFF" : "ON");

      //     if(client.connected())
      //     {
      //       char relayMsg[32];
      //       snprintf(relayMsg, sizeof(relayMsg), "Auto Relay %d -> %s\n", i + 1, shouldBeOff ? "OFF" : "ON");
      //       client.publish(topic2,relayMsg);
      //     }
      //   }
      // }
    }
    else 
    {
      for (int i = 0; i < RELAY_COUNT; i++) 
      {
        relayStates[i] = getRelayState(i);
      }
    }
  } 
}

void callback(char* topic, byte* message, unsigned int length)
{

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.println("-----------------------");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();


  for (int i = 0; i < RELAY_COUNT; i++) {
    String relayTopic = "esp32/relayStatus" + String(i + 1);
    if (String(topic) == relayTopic) 
    {
      if (messageTemp == "ON") 
      {
        relayStates[i] = true;
        digitalWrite(relayIO[i], LOW);
      } 
      else if (messageTemp == "OFF") 
      {
        relayStates[i] = false;
        digitalWrite(relayIO[i], HIGH);
      }

      Serial.printf("Relay %d set to %s\n", i + 1, relayStates[i] ? "ON" : "OFF");
      break;
    }
  }
}

void loop(){
  if(!client.connected())
  {
    reconnect();
  }
  if (client.connected()) 
  {
    client.loop();
  }

  temp();


  delay(10000);
}
